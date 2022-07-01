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

#include <cstdio>
#include <thread>

#include "core.h"
#include "memory.h"
#include "pi.h"
#include "vr4300.h"

static bool running;
static std::thread *thread;

static void run()
{
    // Run the CPU until stopped
    while (running)
        VR4300::runOpcode();
}

bool Core::bootRom(const std::string &path)
{
    // Stop and reset the emulator
    Core::stop();
    Memory::reset();

    // Open the specified ROM file if it exists
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) return false;

    // Load the first 4KB of the ROM into RSP DMEM
    uint8_t data[0x1000];
    fread(data, sizeof(uint8_t), 0x1000, file);
    for (size_t i = 0; i < 0x1000; i++)
        Memory::write<uint8_t>(0x84000000 + i, data[i]);

    PI::reset(file);
    VR4300::reset();

    // Start the emulation thread
    running = true;
    thread = new std::thread(run);
    return true;
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
