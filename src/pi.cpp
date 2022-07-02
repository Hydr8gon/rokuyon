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

#include "pi.h"
#include "memory.h"

uint8_t *PI::rom;
uint32_t PI::romSize;

static uint32_t dramAddr;
static uint32_t cartAddr;

void PI::reset(FILE *romFile)
{
    // Load the first 4KB of the ROM into RSP DMEM
    uint8_t data[0x1000];
    fread(data, sizeof(uint8_t), 0x1000, romFile);
    for (size_t i = 0; i < 0x1000; i++)
        Memory::write<uint8_t>(0x84000000 + i, data[i]);

    // Load the cart ROM into memory
    if (rom) delete[] rom;
    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);
    fseek(romFile, 0, SEEK_SET);
    rom = new uint8_t[romSize];
    fread(rom, sizeof(uint8_t), romSize, romFile);
    fclose(romFile);

    // Reset the PI to its initial state
    dramAddr = 0;
    cartAddr = 0;
}

void PI::writeDramAddr(uint32_t value)
{
    // Set the DMA memory address
    dramAddr = value & 0xFFFFFF;
}

void PI::writeCartAddr(uint32_t value)
{
    // Set the DMA cartridge address
    cartAddr = value;
}

void PI::writeWrLen(uint32_t value)
{
    // Trigger a DMA transfer of the given size
    uint32_t size = (value & 0xFFFFFF) + 1;
    printf("PI DMA from 0x%X to 0x%X with size 0x%X\n", cartAddr, dramAddr, size);

    if (cartAddr >= 0x10000000 && cartAddr < 0x1FC00000) // Cartridge ROM
    {
        // Copy data from the cartridge to memory
        for (uint32_t i = 0; i < size; i++)
        {
            uint32_t j = cartAddr - 0x10000000 + i;
            Memory::write<uint8_t>(0x80000000 + dramAddr + i, (j < romSize) ? rom[j] : 0xFFFFFFFF);
        }
    }
}
