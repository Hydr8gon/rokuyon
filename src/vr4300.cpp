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

#include "vr4300.h"
#include "memory.h"

static uint32_t programCounter;

void VR4300::reset()
{
    // Reset the interpreter to its initial state
    programCounter = 0x84000040; // IPL3
}

void VR4300::runOpcode()
{
    uint32_t opcode = Memory::read<uint32_t>(programCounter);
    printf("Unknown opcode: 0x%08X @ 0x%X\n", opcode, programCounter);
    exit(0);
}
