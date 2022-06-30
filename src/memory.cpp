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

#include "memory.h"

static uint8_t rdram[0x400000]; // 4MB RDRAM

namespace Registers
{
    template <typename T> T read(uint32_t address)
    {
        printf("Unknown register read: 0x%X\n", address);
        return 0;
    }

    template <typename T> void write(uint32_t address, T value)
    {
        printf("Unknown register write: 0x%X\n", address);
    }
}

template uint8_t  Memory::read(uint32_t address);
template uint16_t Memory::read(uint32_t address);
template uint32_t Memory::read(uint32_t address);
template uint64_t Memory::read(uint32_t address);
template <typename T> T Memory::read(uint32_t address)
{
    // Mirror the kseg0 and kseg1 spaces
    if ((address & 0xC0000000) == 0x80000000)
        address &= 0x1FFFFFFF;

    uint8_t *data = nullptr;

    // Get a pointer to readable N64 memory based on the address
    if (address < 0x400000)
        data = &rdram[address & 0x3FFFFF];
    else if (address >= 0x3F00000 && address < 0x4900000)
        return Registers::read<T>(address);

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
    // Mirror the kseg0 and kseg1 spaces
    if ((address & 0xC0000000) == 0x80000000)
        address &= 0x1FFFFFFF;

    uint8_t *data = nullptr;

    // Get a pointer to writable N64 memory based on the address
    if (address < 0x400000)
        data = &rdram[address & 0x3FFFFF];
    else if (address >= 0x3F00000 && address < 0x4900000)
        return Registers::write<T>(address, value);

    if (data != nullptr)
    {
        // Write a value to the pointer, big-endian style (MSB first)
        for (size_t i = 0; i < sizeof(T); i++)
            data[i] = value >> ((sizeof(T) - 1 - i) * 8);
        return;
    }

    printf("Unknown memory write: 0x%X\n", address);
}
