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
#include <cstring>

#include "memory.h"
#include "pi.h"
#include "vi.h"

static uint8_t rdram[0x400000]; // 4MB RDRAM
static uint8_t rspMem[0x2000];  // 4KB RSP DMEM + 4KB RSP IMEM
static uint8_t pifMem[0x800];   // 2KB-64B PIF ROM + 64B PIF RAM

void Memory::reset(FILE *pifFile)
{
    // Load the PIF ROM into memory
    fread(pifMem, sizeof(uint8_t), 0x7C0, pifFile);
    fclose(pifFile);

    // Clear the remaining memory locations
    memset(rdram,  0, sizeof(rdram));
    memset(rspMem, 0, sizeof(rspMem));
    memset(&pifMem[0x7C0], 0, 0x40);
}

namespace Registers
{
    uint32_t read(uint32_t address)
    {
        // Read from an I/O register if one exists at the given address
        switch (address)
        {
            case 0x4040010: return 0x00000001; // SP_STATUS
        }

        printf("Unknown register read: 0x%X\n", address);
        return 0;
    }

    void write(uint32_t address, uint32_t value)
    {
        // Write to an I/O register if one exists at the given address
        switch (address)
        {
            case 0x4400004: return VI::writeOrigin(value);   // VI_ORIGIN
            case 0x4600000: return PI::writeDramAddr(value); // PI_DRAM_ADDR
            case 0x4600004: return PI::writeCartAddr(value); // PI_CART_ADDR
            case 0x460000C: return PI::writeWrLen(value);    // PI_WR_LEN
        }

        printf("Unknown register write: 0x%X\n", address);
    }
}

template uint8_t  Memory::read(uint32_t address);
template uint16_t Memory::read(uint32_t address);
template uint32_t Memory::read(uint32_t address);
template uint64_t Memory::read(uint32_t address);
template <typename T> T Memory::read(uint32_t address)
{
    uint8_t *data = nullptr;

    // Get a pointer to readable N64 memory based on the address
    if ((address & 0xC0000000) == 0x80000000) // kseg0, kseg1
    {
        address &= 0x1FFFFFFF; // Mirror
        if (address < 0x400000) // RDRAM
            data = &rdram[address & 0x3FFFFF];
        else if (address >= 0x4000000 && address < 0x4040000) // RSP DMEM/IMEM
            data = &rspMem[address & 0x1FFF];
        else if (address >= 0x10000000 && address < 0x10000000 + std::min(PI::romSize, 0xFC00000U)) // Cart ROM
            data = &PI::rom[address - 0x10000000];
        else if (address >= 0x1FC00000 && address < 0x1FC00800) // PIF ROM/RAM
            data = &pifMem[address & 0x7FF];
        else if (address >= 0x3F00000 && address < 0x4900000) // Registers
            return Registers::read(address);
    }

    if (data != nullptr)
    {
        // Read a value from the pointer, big-endian style (MSB first)
        T value = 0;
        for (size_t i = 0; i < sizeof(T); i++)
            value |= data[i] << ((sizeof(T) - 1 - i) * 8);
        return value;
    }

    printf("Unknown memory read: 0x%X\n", address);
    return 0;
}

template void Memory::write(uint32_t address, uint8_t  value);
template void Memory::write(uint32_t address, uint16_t value);
template void Memory::write(uint32_t address, uint32_t value);
template void Memory::write(uint32_t address, uint64_t value);
template <typename T> void Memory::write(uint32_t address, T value)
{
    // TODO: remove this dumb hack that makes IPL2 boot
    if (address == 0xBFC007FC && value == 0x30)
        value |= 0x80;

    uint8_t *data = nullptr;

    // Get a pointer to writable N64 memory based on the address
    if ((address & 0xC0000000) == 0x80000000) // kseg0, kseg1
    {
        address &= 0x1FFFFFFF; // Mirror
        if (address < 0x400000) // RDRAM
            data = &rdram[address & 0x3FFFFF];
        else if (address >= 0x4000000 && address < 0x4040000) // RSP DMEM/IMEM
            data = &rspMem[address & 0x1FFF];
        else if (address >= 0x1FC007C0 && address < 0x1FC00800) // PIF RAM
            data = &pifMem[address & 0x7FF];
        else if (address >= 0x3F00000 && address < 0x4900000) // Registers
            return Registers::write(address, value);
    }

    if (data != nullptr)
    {
        // Write a value to the pointer, big-endian style (MSB first)
        for (size_t i = 0; i < sizeof(T); i++)
            data[i] = value >> ((sizeof(T) - 1 - i) * 8);
        return;
    }

    printf("Unknown memory write: 0x%X\n", address);
}
