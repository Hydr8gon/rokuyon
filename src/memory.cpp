/*
    Copyright 2022-2023 Hydr8gon

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
#include "core.h"
#include "cpu_cp0.h"
#include "log.h"
#include "mi.h"
#include "pi.h"
#include "pif.h"
#include "rdp.h"
#include "rsp.h"
#include "rsp_cp0.h"
#include "settings.h"
#include "si.h"
#include "vi.h"

enum FlashState
{
    FLASH_NONE = 0,
    FLASH_STATUS,
    FLASH_READ,
    FLASH_WRITE,
    FLASH_ERASE
};

struct TLBEntry
{
    uint32_t entryLo0;
    uint32_t entryLo1;
    uint32_t entryHi;
    uint32_t pageMask;
};

namespace Memory
{
    uint8_t rdram[0x800000]; // 8MB RDRAM
    uint8_t rspMem[0x2000];  // 4KB RSP DMEM + 4KB RSP IMEM
    TLBEntry entries[32];
    uint32_t ramSize;

    uint8_t writeBuf[0x80];
    uint32_t writeOfs;
    uint32_t eraseOfs;
    FlashState state;

    void writeFlash(uint32_t value);
}

void Memory::reset()
{
    // Reset memory to its initial state
    memset(rdram, 0, sizeof(rdram));
    memset(rspMem, 0, sizeof(rspMem));
    memset(writeBuf, 0, sizeof(writeBuf));
    ramSize = Settings::expansionPak ? 0x800000 : 0x400000;
    writeOfs = 0;
    eraseOfs = 0;
    state = FLASH_NONE;

    // Map TLB entries to inaccessible locations
    for (int i = 0; i < 32; i++)
        entries[i].entryHi = 0x80000000;
}

void Memory::getEntry(uint32_t index, uint32_t &entryLo0, uint32_t &entryLo1, uint32_t &entryHi, uint32_t &pageMask)
{
    // Get the TLB entry at the given index
    TLBEntry &entry = entries[index & 0x1F];
    entryLo0 = entry.entryLo0;
    entryLo1 = entry.entryLo1;
    entryHi = entry.entryHi;
    pageMask = entry.pageMask;
}

void Memory::setEntry(uint32_t index, uint32_t entryLo0, uint32_t entryLo1, uint32_t entryHi, uint32_t pageMask)
{
    // Set the TLB entry at the given index
    TLBEntry &entry = entries[index & 0x1F];
    entry.entryLo0 = entryLo0;
    entry.entryLo1 = entryLo1;
    entry.entryHi = entryHi;
    entry.pageMask = pageMask;
}

template uint8_t  Memory::read(uint32_t address);
template uint16_t Memory::read(uint32_t address);
template uint32_t Memory::read(uint32_t address);
template uint64_t Memory::read(uint32_t address);
template <typename T> T Memory::read(uint32_t address)
{
    uint8_t *data = nullptr;
    uint32_t pAddr = 0x80000000;

    // Get a physical address from a virtual one
    if ((address & 0xC0000000) == 0x80000000) // kseg0, kseg1
    {
        // Mask the virtual address to get a physical one
        pAddr = address & 0x1FFFFFFF;
    }
    else // TLB
    {
        // Search the TLB entries for a page that contains the virtual address
        // TODO: actually use the ASID and CDVG bits, and support TLB invalid exceptions
        for (int i = 0; i < 32; i++)
        {
            uint32_t vAddr = entries[i].entryHi & 0xFFFFE000;
            uint32_t mask = entries[i].pageMask | 0x1FFF;

            if (address - vAddr <= mask)
            {
                // Choose between the even or odd physical pages, and add the masked offset
                if (address - vAddr <= (mask >> 1))
                    pAddr = ((entries[i].entryLo0 & 0x3FFFFC0) << 6) + (address & (mask >> 1));
                else
                    pAddr = ((entries[i].entryLo1 & 0x3FFFFC0) << 6) + (address & (mask >> 1));
                goto lookup;
            }
        }

        // Trigger a TLB load miss exception if a TLB entry wasn't found
        CPU_CP0::exception(2);
        CPU_CP0::setTlbAddress(address);
        return 0;
    }

lookup:
    // Look up the physical address
    if (pAddr < ramSize)
    {
        // Get a pointer to data in RDRAM
        // TODO: figure out RDRAM registers and how they affect mapping
        data = &rdram[pAddr & 0x3FFFFF];
    }
    else if (pAddr >= 0x4000000 && pAddr < 0x4040000)
    {
        // Read a value from RSP DMEM/IMEM, with wraparound
        T value = 0;
        for (size_t i = 0; i < sizeof(T); i++)
            value |= (T)rspMem[(pAddr & 0x1000) | ((pAddr + i) & 0xFFF)] << ((sizeof(T) - 1 - i) * 8);
        return value;
    }
    else if (pAddr >= 0x8000000 && pAddr < 0x8008000 && Core::saveSize == 0x8000)
    {
        // Get a pointer to data in cart SRAM, if it exists
        data = &Core::save[pAddr & 0x7FFF];
    }
    else if (pAddr >= 0x8000000 && pAddr < 0x8020000 && state == FLASH_READ)
    {
        // Get a pointer to data in cart FLASH, if it's readable
        data = &Core::save[address & 0x1FFFF];
    }
    else if (pAddr >= 0x10000000 && pAddr < 0x10000000 + std::min(Core::romSize, 0xFC00000U))
    {
        // Get a pointer to data in cart ROM
        data = &Core::rom[pAddr - 0x10000000];
    }
    else if (pAddr >= 0x1FC00000 && pAddr < 0x1FC00800)
    {
        // Get a pointer to data in PIF ROM/RAM
        data = &PIF::memory[pAddr & 0x7FF];
    }
    else if (sizeof(T) != sizeof(uint32_t))
    {
        // Ignore I/O writes that aren't 32-bit
    }
    else if (pAddr >= 0x4040000 && pAddr < 0x4040020)
    {
        // Read a value from an RSP CP0 register
        return RSP_CP0::read((pAddr & 0x1F) >> 2);
    }
    else if (pAddr == 0x4080000)
    {
        // Read a value from the RSP program counter
        return RSP::readPC();
    }
    else if (pAddr >= 0x4100000 && pAddr < 0x4100020)
    {
        // Read a value from an RDP register
        return RDP::read((pAddr & 0x1F) >> 2);
    }
    else if (pAddr == 0x470000C)
    {
        // Stub the RI_SELECT register
        return 0x1;
    }
    else
    {
        // Read a value from a group of registers
        switch (pAddr >> 20)
        {
            case 0x43: return MI::read(pAddr);
            case 0x44: return VI::read(pAddr);
            case 0x45: return AI::read(pAddr);
            case 0x46: return PI::read(pAddr);
            case 0x48: return SI::read(pAddr);
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
    uint32_t pAddr = 0x80000000;

    // Get a physical address from a virtual one
    if ((address & 0xC0000000) == 0x80000000) // kseg0, kseg1
    {
        // Mask the virtual address to get a physical one
        pAddr = address & 0x1FFFFFFF;
    }
    else // TLB
    {
        // Search the TLB entries for a page that contains the virtual address
        // TODO: actually use the ASID and CDVG bits, and support TLB invalid exceptions
        for (int i = 0; i < 32; i++)
        {
            uint32_t vAddr = entries[i].entryHi & 0xFFFFE000;
            uint32_t mask = entries[i].pageMask | 0x1FFF;

            if (address - vAddr <= mask)
            {
                // Choose between the even or odd physical pages, and add the masked offset
                if (address - vAddr <= (mask >> 1))
                {
                    if (entries[i].entryLo0 & 0x4) // Dirty
                    {
                        pAddr = ((entries[i].entryLo0 & 0x3FFFFC0) << 6) + (address & (mask >> 1));
                        goto lookup;
                    }
                }
                else
                {
                    if (entries[i].entryLo1 & 0x4) // Dirty
                    {
                        pAddr = ((entries[i].entryLo1 & 0x3FFFFC0) << 6) + (address & (mask >> 1));
                        goto lookup;
                    }
                }

                // Trigger a TLB modification exception if the TLB entry isn't writable
                CPU_CP0::exception(1);
                CPU_CP0::setTlbAddress(address);
                return;
            }
        }

        // Trigger a TLB store miss exception if a TLB entry wasn't found
        CPU_CP0::exception(3);
        CPU_CP0::setTlbAddress(address);
        return;
    }

lookup:
    // Look up the physical address
    if (pAddr < ramSize)
    {
        // Get a pointer to data in RDRAM
        // TODO: figure out RDRAM registers and how they affect mapping
        data = &rdram[pAddr & 0x3FFFFF];
    }
    else if (pAddr >= 0x4000000 && pAddr < 0x4040000)
    {
        // Write a value to RSP DMEM/IMEM, with wraparound
        for (size_t i = 0; i < sizeof(T); i++)
            rspMem[(pAddr & 0x1000) | ((pAddr + i) & 0xFFF)] = value >> ((sizeof(T) - 1 - i) * 8);
        return;
    }
    else if (pAddr >= 0x8000000 && pAddr < 0x8008000 && Core::saveSize == 0x8000)
    {
        // Write a value to cart SRAM, if it exists
        for (size_t i = 0; i < sizeof(T); i++)
            Core::writeSave((pAddr + i) & 0x7FFF, value >> ((sizeof(T) - 1 - i) * 8));
        return;
    }
    else if (pAddr >= 0x8000000 && pAddr < 0x8000080 && state == FLASH_WRITE)
    {
        // Get a pointer to data in the FLASH write buffer, if it's writable
        data = &writeBuf[address & 0x7F];
    }
    else if (pAddr >= 0x1FC007C0 && pAddr < 0x1FC00800)
    {
        // Get a pointer to data in PIF ROM/RAM
        data = &PIF::memory[pAddr & 0x7FF];

        // Catch writes to the PIF command byte and call the PIF
        if (pAddr >= 0x1FC00800 - sizeof(T))
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
    else if (pAddr >= 0x4040000 && pAddr < 0x4040020)
    {
        // Write a value to an RSP CP0 register
        return RSP_CP0::write((pAddr & 0x1F) >> 2, value);
    }
    else if (pAddr == 0x4080000)
    {
        // Write a value to the RSP program counter
        return RSP::writePC(value);
    }
    else if (pAddr >= 0x4100000 && pAddr < 0x4100020)
    {
        // Write a value to an RDP register
        return RDP::write((pAddr & 0x1F) >> 2, value);
    }
    else if (pAddr == 0x8010000 && Core::saveSize == 0x20000)
    {
        // Write a value to the FLASH register
        return writeFlash(value);
    }
    else
    {
        // Write a value to a group of registers
        switch (pAddr >> 20)
        {
            case 0x43: return MI::write(pAddr, value);
            case 0x44: return VI::write(pAddr, value);
            case 0x45: return AI::write(pAddr, value);
            case 0x46: return PI::write(pAddr, value);
            case 0x48: return SI::write(pAddr, value);
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

void Memory::writeFlash(uint32_t value)
{
    // Handle a FLASH register write based on the command byte
    switch (uint8_t command = value >> 24)
    {
        case 0xD2: // Execute
            switch (state)
            {
                case FLASH_WRITE:
                    // Copy the write buffer to a save block
                    for (int i = 0; i < 0x80; i++)
                        Core::writeSave(writeOfs + i, writeBuf[i]);
                    return;

                case FLASH_ERASE:
                    // Reset the contents of a save block
                    for (int i = 0; i < 0x80; i++)
                        Core::writeSave(eraseOfs + i, 0xFF);
                    return;

                default:
                    LOG_WARN("Executing FLASH in invalid state: %d\n", state);
                    return;
            }

        case 0xE1: // Status
            // Change the FLASH state to status
            state = FLASH_STATUS;
            return;

        case 0xF0: // Read
            // Change the FLASH state to read
            state = FLASH_READ;
            return;

        case 0xB4: // Write
            // Change the FLASH state to write
            state = FLASH_WRITE;
            return;

        case 0x78: // Erase
            // Change the FLASH state to erase
            state = FLASH_ERASE;
            return;

        case 0x4B: // Erase Offset
            // Set the address of the save block to erase
            eraseOfs = (value & 0xFFFF) << 7;
            return;

        case 0xA5: // Write Offset
            // Set the address of the save block to write
            writeOfs = (value & 0xFFFF) << 7;
            return;

        default:
            LOG_CRIT("Unknown FLASH command: 0x%02X\n", command);
            return;
    }
}
