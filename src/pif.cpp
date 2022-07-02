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

#include <cstring>

#include "pif.h"
#include "memory.h"

uint8_t PIF::memory[0x800]; // 2KB-64B PIF ROM + 64B PIF RAM

// Small command lookup table for PIF command bits
static void (*pifCommands[7])(int) =
{
    PIF::unknownCmd, PIF::unknownCmd,     PIF::unknownCmd, PIF::unknownCmd, // 0-3
    PIF::unknownCmd, PIF::verifyChecksum, PIF::clearMemory                  // 4-6
};

void PIF::reset(FILE *pifFile)
{
    // Load the PIF ROM into memory
    fread(memory, sizeof(uint8_t), 0x7C0, pifFile);
    fclose(pifFile);
    clearMemory(0);
}

void PIF::runCommand()
{
    // Execute commands for any set bits, and clear the bits
    for (int i = 0; i < 7; i++)
    {
        if (memory[0x7FF] & (1 << i))
        {
            (*pifCommands[i])(i);
            memory[0x7FF] &= ~(1 << i);
        }
    }
}

void PIF::verifyChecksum(int bit)
{
    // On hardware, this command verifies if a checksum matches one given by the CIC
    // It doesn't matter for emulation, so just set the result bit
    memory[0x7FF] |= 0x80;
}

void PIF::clearMemory(int bit)
{
    // Clear the 64 bytes of PIF RAM
    memset(&memory[0x7C0], 0, 0x40);
}

void PIF::unknownCmd(int bit)
{
    // Warn about unknown commands
    printf("Unknown PIF command bit: %d\n", bit);
}
