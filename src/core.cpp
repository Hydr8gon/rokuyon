/*
    Copyright 2022 Hydr8gon

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
#include <thread>
#include <vector>

#include "core.h"
#include "ai.h"
#include "cpu.h"
#include "cpu_cp0.h"
#include "cpu_cp1.h"
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
    std::thread *thread;
    bool running;
    bool rspRunning;

    std::vector<Task> tasks;
    uint32_t globalCycles;
    uint32_t cpuCycles;
    uint32_t rspCycles;

    int fps;
    int fpsCount;
    std::chrono::steady_clock::time_point lastFpsTime;

    void run();
    void resetCycles();
}

int Core::bootRom(const std::string &path)
{
    // Try to open the PIF ROM file
    FILE *pifFile = fopen("pif_rom.bin", "rb");
    if (!pifFile) return 1;

    // Try to open the specified ROM file
    FILE *romFile = fopen(path.c_str(), "rb");
    if (!romFile) return 2;

    // Derive the save path from the ROM path
    std::string savePath = path.substr(0, path.rfind(".")) + ".sav";

    // Ensure the emulator is stopped
    stop();

    // Reset the scheduler
    tasks.clear();
    globalCycles = 0;
    cpuCycles = 0;
    rspCycles = 0;
    schedule(resetCycles, 0x7FFFFFFF);

    // Reset the emulated components
    Memory::reset();
    AI::reset();
    MI::reset();
    PI::reset(romFile);
    SI::reset();
    VI::reset();
    PIF::reset(pifFile, savePath);
    CPU::reset();
    CPU_CP0::reset();
    CPU_CP1::reset();
    RDP::reset();
    RSP::reset();
    RSP_CP0::reset();
    RSP_CP2::reset();

    // Start the emulator
    start();
    return 0;
}

void Core::start()
{
    // Start the emulation thread if it wasn't running
    if (!running)
    {
        running = true;
        thread = new std::thread(run);
    }
}

void Core::stop()
{
    // Stop the emulation thread if it was running
    if (running)
    {
        running = false;
        thread->join();
        delete thread;
    }
}

void Core::run()
{
    while (running)
    {
        // Run the CPUs until the next scheduled task
        while (tasks[0].cycles > globalCycles)
        {
            // Run a CPU opcode if ready and schedule the next one
            if (globalCycles >= cpuCycles)
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
            globalCycles = rspRunning ? std::min(cpuCycles, rspCycles) : cpuCycles;
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

void Core::resetCycles()
{
    // Reset the cycle counts to prevent overflow
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
