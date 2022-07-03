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
#include "pif.h"
#include "vi.h"

static uint8_t rdram[0x400000]; // 4MB RDRAM
static uint8_t rspMem[0x2000];  // 4KB RSP DMEM + 4KB RSP IMEM

void Memory::reset()
{
    // Clear the remaining memory locations
    memset(rdram,  0, sizeof(rdram));
    memset(rspMem, 0, sizeof(rspMem));
}

namespace Registers
{
    uint32_t read(uint32_t address)
    {
        // Read from an I/O register if one exists at the given address
        switch (address)
        {
            case 0x4040010: return 0x00000001; // SP_STATUS
            case 0x470000C: return 0x00000001; // RI_SELECT_REG
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
        uint32_t addr = address & 0x1FFFFFFF; // Mirror
        if (addr < 0x400000) // RDRAM
            data = &rdram[addr & 0x3FFFFF];
        else if (addr >= 0x4000000 && addr < 0x4040000) // RSP DMEM/IMEM
            data = &rspMem[addr & 0x1FFF];
        else if (addr >= 0x10000000 && addr < 0x10000000 + std::min(PI::romSize, 0xFC00000U)) // Cart ROM
            data = &PI::rom[addr - 0x10000000];
        else if (addr >= 0x3F00000 && addr < 0x4900000) // Registers
            return Registers::read(addr);
        else if (addr >= 0x1FC00000 && addr < 0x1FC00800) // PIF ROM/RAM
            data = &PIF::memory[addr & 0x7FF];
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
    uint8_t *data = nullptr;

    // Get a pointer to writable N64 memory based on the address
    if ((address & 0xC0000000) == 0x80000000) // kseg0, kseg1
    {
        uint32_t addr = address & 0x1FFFFFFF; // Mirror
        if (addr < 0x400000) // RDRAM
            data = &rdram[addr & 0x3FFFFF];
        else if (addr >= 0x4000000 && addr < 0x4040000) // RSP DMEM/IMEM
            data = &rspMem[addr & 0x1FFF];
        else if (addr >= 0x3F00000 && addr < 0x4900000) // Registers
            return Registers::write(addr, value);
        else if (addr >= 0x1FC007C0 && addr < 0x1FC00800) // PIF RAM
        {
            data = &PIF::memory[addr & 0x7FF];

            // Catch writes to the PIF command byte and call the PIF
            if (addr >= 0x1FC00800 - sizeof(T))
            {
                for (size_t i = 0; i < sizeof(T); i++)
                    data[i] = value >> ((sizeof(T) - 1 - i) * 8);
                PIF::runCommand();
                return;
            }
        }
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
