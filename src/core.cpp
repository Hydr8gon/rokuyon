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

#include <chrono>
#include <thread>

#include "core.h"
#include "cpu.h"
#include "cpu_cp0.h"
#include "cpu_cp1.h"
#include "memory.h"
#include "mi.h"
#include "pi.h"
#include "pif.h"
#include "rsp.h"
#include "rsp_cp0.h"
#include "si.h"
#include "vi.h"

namespace Core
{
    std::thread *thread;
    bool running;
    bool rspRunning;

    int fps;
    int fpsCount;
    std::chrono::steady_clock::time_point lastFpsTime;

    void run();
}

void Core::run()
{
    while (Core::running)
    {
        // Run a frame's worth of CPU and RSP instructions
        for (uint32_t i = 0; i < (93750000 / 60) / 3; i++)
        {
            CPU::runOpcode();
            if (rspRunning) RSP::runOpcode();
            CPU::runOpcode();
            if (rspRunning) RSP::runOpcode();
            CPU::runOpcode();
        }

        // Draw a frame
        VI::drawFrame();

        // Update the FPS counter
        std::chrono::duration<double> fpsTime = std::chrono::steady_clock::now() - lastFpsTime;
        if (fpsTime.count() >= 1.0f)
        {
            // Save the FPS value after one second and reset the counter
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
}

int Core::bootRom(const std::string &path)
{
    // Try to open the PIF ROM file
    FILE *pifFile = fopen("pif_rom.bin", "rb");
    if (!pifFile) return 1;

    // Try to open the specified ROM file
    FILE *romFile = fopen(path.c_str(), "rb");
    if (!romFile) return 2;

    // Stop and reset the emulator
    Core::stop();
    Memory::reset();
    MI::reset();
    PI::reset(romFile);
    SI::reset();
    VI::reset();
    PIF::reset(pifFile);
    CPU::reset();
    CPU_CP0::reset();
    CPU_CP1::reset();
    RSP::reset();
    RSP_CP0::reset();

    // Start the emulation thread
    running = true;
    thread = new std::thread(run);
    return 0;
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
