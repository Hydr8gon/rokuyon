/*
    Copyright 2022-2023 Hydr8gon

    This file is part of rokuyon.

    rokuyon is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    rokuyon is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
    General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with rokuyon. If not, see <https://www.gnu.org/licenses/>.
*/

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <thread>
#include <vector>

#include "core.h"
#include "ai.h"
#include "cpu.h"
#include "cpu_cp0.h"
#include "cpu_cp1.h"
#include "log.h"
#include "memory.h"
#include "mi.h"
#include "pi.h"
#include "pif.h"
#include "rdp.h"
#include "rsp.h"
#include "rsp_cp0.h"
#include "rsp_cp2.h"
#include "si.h"
#include "vi.h"

struct Task
{
    Task(void (*function)(), uint32_t cycles):
        function(function), cycles(cycles) {}

    void (*function)();
    uint32_t cycles;

    bool operator<(const Task &task) const
    {
        return cycles < task.cycles;
    }
};

namespace Core
{
    std::thread *emuThread;
    std::thread *saveThread;
    std::condition_variable condVar;
    std::mutex waitMutex;
    std::mutex saveMutex;

    bool running;
    bool cpuRunning;
    bool rspRunning;

    std::vector<Task> tasks;
    uint32_t globalCycles;
    uint32_t cpuCycles;
    uint32_t rspCycles;

    int fps;
    int fpsCount;
    std::chrono::steady_clock::time_point lastFpsTime;

    std::string savePath;
    uint8_t *rom;
    uint8_t *save;
    uint32_t romSize;
    uint32_t saveSize;
    bool saveDirty;

    void runLoop();
    void saveLoop();
    void updateSave();
    void resetCycles();
}

bool Core::bootRom(const std::string &path)
{
    // Try to open the specified ROM file
    FILE *romFile = fopen(path.c_str(), "rb");
    if (!romFile) return false;

    // Ensure the emulator is stopped
    stop();

    // Load the ROM into memory
    if (rom) delete[] rom;
    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);
    fseek(romFile, 0, SEEK_SET);
    rom = new uint8_t[romSize];
    fread(rom, sizeof(uint8_t), romSize, romFile);
    fclose(romFile);

    // Derive the save path from the ROM path
    savePath = path.substr(0, path.rfind(".")) + ".sav";
    if (save) delete[] save;
    saveDirty = false;

    if (FILE *saveFile = fopen(savePath.c_str(), "rb"))
    {
        // Load the save file into memory if it exists
        fseek(saveFile, 0, SEEK_END);
        saveSize = ftell(saveFile);
        fseek(saveFile, 0, SEEK_SET);
        save = new uint8_t[saveSize];
        fread(save, sizeof(uint8_t), saveSize, saveFile);
        fclose(saveFile);
    }
    else
    {
        // If no save file exists, assume no save
        // TODO: some sort of detection, or a database
        saveSize = 0;
        save = nullptr;
    }

    // Reset the scheduler
    cpuRunning = true;
    tasks.clear();
    globalCycles = 0;
    cpuCycles = 0;
    rspCycles = 0;
    schedule(resetCycles, 0x7FFFFFFF);

    // Reset the emulated components
    Memory::reset();
    AI::reset();
    CPU::reset();
    CPU_CP0::reset();
    CPU_CP1::reset();
    MI::reset();
    PI::reset();
    SI::reset();
    VI::reset();
    PIF::reset();
    RDP::reset();
    RSP::reset();
    RSP_CP0::reset();
    RSP_CP2::reset();

    // Start the emulator
    start();
    return true;
}

void Core::resizeSave(uint32_t newSize)
{
    // Create a save with the new size
    saveMutex.lock();
    uint8_t *newSave = new uint8_t[newSize];

    if (saveSize < newSize) // New save is larger
    {
        // Copy all of the old save and fill the rest with 0xFF
        memcpy(newSave, save, saveSize * sizeof(uint8_t));
        memset(&newSave[saveSize], 0xFF, (newSize - saveSize) * sizeof(uint8_t));
    }
    else // New save is smaller
    {
        // Copy as much of the old save as possible
        memcpy(newSave, save, newSize * sizeof(uint8_t));
    }

    // Swap the old save for the new one
    delete[] save;
    save = newSave;
    saveSize = newSize;
    saveDirty = true;
    saveMutex.unlock();
    updateSave();
}

void Core::start()
{
    // Start the threads if emulation wasn't running
    if (!running)
    {
        running = true;
        emuThread = new std::thread(runLoop);
        saveThread = new std::thread(saveLoop);
    }
}

void Core::stop()
{
    if (running)
    {
        {
            // Signal for the threads to stop
            std::lock_guard<std::mutex> guard(waitMutex);
            running = false;
            condVar.notify_one();
        }

        // Stop the threads if emulation was running
        emuThread->join();
        saveThread->join();
        delete emuThread;
        delete saveThread;
        RDP::finishThread();
    }
}

void Core::runLoop()
{
    while (running)
    {
        // Run the CPUs until the next scheduled task
        while (tasks[0].cycles > globalCycles)
        {
            // Run a CPU opcode if ready and schedule the next one
            if (cpuRunning && globalCycles >= cpuCycles)
            {
                CPU::runOpcode();
                cpuCycles = globalCycles + 2;
            }

            // Run an RSP opcode if ready and schedule the next one
            if (rspRunning && globalCycles >= rspCycles)
            {
                RSP::runOpcode();
                rspCycles = globalCycles + 3;
            }

            // Jump to the next soonest opcode
            globalCycles = std::min<uint32_t>(cpuRunning ? cpuCycles : -1, rspRunning ? rspCycles : -1);
        }

        // Jump to the next scheduled task
        globalCycles = tasks[0].cycles;

        // Run all tasks that are scheduled now
        while (tasks[0].cycles <= globalCycles)
        {
            (*tasks[0].function)();
            tasks.erase(tasks.begin());
        }
    }
}

void Core::saveLoop()
{
    while (running)
    {
        // Every few seconds, check if the save file should be updated
        std::unique_lock<std::mutex> lock(waitMutex);
        condVar.wait_for(lock, std::chrono::seconds(3), [&]{ return !running; });
        updateSave();
    }
}

void Core::countFrame()
{
    // Calculate the time since the FPS was last updated
    std::chrono::duration<double> fpsTime = std::chrono::steady_clock::now() - lastFpsTime;

    if (fpsTime.count() >= 1.0f)
    {
        // Update the FPS value after one second and reset the counter
        fps = fpsCount;
        fpsCount = 0;
        lastFpsTime = std::chrono::steady_clock::now();
    }
    else
    {
        // Count another frame
        fpsCount++;
    }
}

void Core::writeSave(uint32_t address, uint8_t value)
{
    // Safely write a byte of data to the current save
    saveMutex.lock();
    save[address] = value;
    saveDirty = true;
    saveMutex.unlock();
}

void Core::updateSave()
{
    // Update the save file if the data changed
    saveMutex.lock();
    if (saveDirty)
    {
        if (FILE *saveFile = fopen(savePath.c_str(), "wb"))
        {
            LOG_INFO("Writing save file to disk\n");
            fwrite(save, sizeof(uint8_t), saveSize, saveFile);
            fclose(saveFile);
            saveDirty = false;
        }
    }
    saveMutex.unlock();
}

void Core::resetCycles()
{
    // Reset the cycle counts to prevent overflow
    CPU_CP0::resetCycles();
    for (size_t i = 0; i < tasks.size(); i++)
        tasks[i].cycles -= globalCycles;
    cpuCycles -= std::min(globalCycles, cpuCycles);
    rspCycles -= std::min(globalCycles, rspCycles);
    globalCycles -= globalCycles;

    // Schedule the next cycle reset
    schedule(resetCycles, 0x7FFFFFFF);
}

void Core::schedule(void (*function)(), uint32_t cycles)
{
    // Add a task to the scheduler, sorted by least to most cycles until execution
    // Cycles run at 93.75 * 2 MHz
    Task task(function, globalCycles + cycles);
    auto it = std::upper_bound(tasks.cbegin(), tasks.cend(), task);
    tasks.insert(it, task);
}
