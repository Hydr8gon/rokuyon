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

#include <thread>

#include "core.h"
#include "memory.h"
#include "pi.h"
#include "pif.h"
#include "vi.h"
#include "vr4300.h"

bool Core::running;
static std::thread *thread;

static void run()
{
    while (Core::running)
    {
        // Run a frame's worth of instructions and then draw a frame
        for (uint32_t i = 0; i < 93750000 / 60; i++)
            VR4300::runOpcode();
        VI::drawFrame();
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
    PI::reset(romFile);
    PIF::reset(pifFile);
    VI::reset();
    VR4300::reset();

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
