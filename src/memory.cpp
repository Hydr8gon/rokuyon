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
#include "ai.h"
#include "log.h"
#include "mi.h"
#include "pi.h"
#include "pif.h"
#include "rdp.h"
#include "rsp.h"
#include "rsp_cp0.h"
#include "si.h"
#include "vi.h"

namespace Memory
{
    uint8_t rdram[0x400000]; // 4MB RDRAM
    uint8_t rspMem[0x2000];  // 4KB RSP DMEM + 4KB RSP IMEM
}

void Memory::reset()
{
    // Clear the remaining memory locations
    memset(rdram,  0, sizeof(rdram));
    memset(rspMem, 0, sizeof(rspMem));
}

template uint8_t  Memory::read(uint32_t address);
template uint16_t Memory::read(uint32_t address);
template uint32_t Memory::read(uint32_t address);
template uint64_t Memory::read(uint32_t address);
template <typename T> T Memory::read(uint32_t address)
{
    uint8_t *data = nullptr;

    if ((address & 0xC0000000) == 0x80000000) // kseg0, kseg1
    {
        // Mask the virtual address to get a physical one
        uint32_t addr = address & 0x1FFFFFFF;

        if (addr < 0x400000)
        {
            // Get a pointer to data in RDRAM
            data = &rdram[addr & 0x3FFFFF];
        }
        else if (addr >= 0x4000000 && addr < 0x4040000)
        {
            // Get a pointer to data in RSP DMEM/IMEM
            data = &rspMem[addr & 0x1FFF];
        }
        else if (addr >= 0x10000000 && addr < 0x10000000 + std::min(PI::romSize, 0xFC00000U))
        {
            // Get a pointer to data in cart ROM
            data = &PI::rom[addr - 0x10000000];
        }
        else if (addr >= 0x1FC00000 && addr < 0x1FC00800)
        {
            // Get a pointer to data in PIF ROM/RAM
            data = &PIF::memory[addr & 0x7FF];
        }
        else if (sizeof(T) != sizeof(uint32_t))
        {
            // Ignore I/O writes that aren't 32-bit
        }
        else if (addr >= 0x4040000 && addr < 0x4040020)
        {
            // Read a value from an RSP CP0 register
            return RSP_CP0::read((addr & 0x1F) >> 2);
        }
        else if (addr == 0x4080000)
        {
            // Read a value from the RSP program counter
            return RSP::readPC();
        }
        else if (addr >= 0x4100000 && addr < 0x4100020)
        {
            // Read a value from an RDP register
            return RDP::read((addr & 0x1F) >> 2);
        }
        else if (addr == 0x470000C)
        {
            // Stub the RI_SELECT register
            return 0x1;
        }
        else
        {
            // Read a value from a group of registers
            switch (addr >> 20)
            {
                case 0x43: return MI::read(addr);
                case 0x44: return VI::read(addr);
                case 0x45: return AI::read(addr);
                case 0x46: return PI::read(addr);
                case 0x48: return SI::read(addr);
            }
        }
    }

    if (data != nullptr)
    {
        // Read a value from the pointer, big-endian style (MSB first)
        T value = 0;
        for (size_t i = 0; i < sizeof(T); i++)
            value |= (T)data[i] << ((sizeof(T) - 1 - i) * 8);
        return value;
    }

    LOG_WARN("Unknown memory read: 0x%X\n", address);
    return 0;
}

template void Memory::write(uint32_t address, uint8_t  value);
template void Memory::write(uint32_t address, uint16_t value);
template void Memory::write(uint32_t address, uint32_t value);
template void Memory::write(uint32_t address, uint64_t value);
template <typename T> void Memory::write(uint32_t address, T value)
{
    uint8_t *data = nullptr;

    if ((address & 0xC0000000) == 0x80000000) // kseg0, kseg1
    {
        // Mask the virtual address to get a physical one
        uint32_t addr = address & 0x1FFFFFFF;

        if (addr < 0x400000)
        {
            // Get a pointer to data in RDRAM
            data = &rdram[addr & 0x3FFFFF];
        }
        else if (addr >= 0x4000000 && addr < 0x4040000)
        {
            // Get a pointer to data in RSP DMEM/IMEM
            data = &rspMem[addr & 0x1FFF];
        }
        else if (addr >= 0x1FC007C0 && addr < 0x1FC00800)
        {
            // Get a pointer to data in PIF ROM/RAM
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
        else if (sizeof(T) != sizeof(uint32_t))
        {
            // Ignore I/O writes that aren't 32-bit
        }
        else if (addr >= 0x4040000 && addr < 0x4040020)
        {
            // Write a value to an RSP CP0 register
            return RSP_CP0::write((addr & 0x1F) >> 2, value);
        }
        else if (addr == 0x4080000)
        {
            // Write a value to the RSP program counter
            return RSP::writePC(value);
        }
        else if (addr >= 0x4100000 && addr < 0x4100020)
        {
            // Write a value to an RDP register
            return RDP::write((addr & 0x1F) >> 2, value);
        }
        else
        {
            // Write a value to a group of registers
            switch (addr >> 20)
            {
                case 0x43: return MI::write(addr, value);
                case 0x44: return VI::write(addr, value);
                case 0x45: return AI::write(addr, value);
                case 0x46: return PI::write(addr, value);
                case 0x48: return SI::write(addr, value);
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

    LOG_WARN("Unknown memory write: 0x%X\n", address);
}
