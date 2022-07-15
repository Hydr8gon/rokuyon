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

#include "si.h"
#include "log.h"
#include "memory.h"
#include "mi.h"
#include "pif.h"

namespace SI
{
    uint32_t dramAddr;

    void performReadDma(uint32_t address);
    void performWriteDma(uint32_t address);
}

void SI::reset()
{
    // Reset the SI to its initial state
    dramAddr = 0;
}

uint32_t SI::read(uint32_t address)
{
    // Read from an I/O register if one exists at the given address
    switch (address)
    {
        default:
            LOG_WARN("Unknown SI register read: 0x%X\n", address);
            return 0;
    }
}

void SI::write(uint32_t address, uint32_t value)
{
    // Write to an I/O register if one exists at the given address
    switch (address)
    {
        case 0x4800000: // SI_DRAM_ADDR
            // Set the RDRAM DMA address
            dramAddr = value & 0xFFFFFF;
            return;

        case 0x4800004: // SI_PIF_AD_RD64B
            // Start a DMA transfer from PIF to RDRAM
            performReadDma(value & 0x7FC);
            return;

        case 0x4800010: // SI_PIF_AD_WR64B
            // Start a DMA transfer from RDRAM to PIF
            performWriteDma(value & 0x7FC);
            return;

        case 0x4800018: // SI_STATUS
            // Acknowledge an SI interrupt
            MI::clearInterrupt(1);
            return;

        default:
            LOG_WARN("Unknown SI register write: 0x%X\n", address);
            return;
    }
}

void SI::performReadDma(uint32_t address)
{
    LOG_INFO("SI DMA from PIF 0x%X to RDRAM 0x%X with size 0x40\n", address, dramAddr);

    // Re-trigger the last PIF command on DMA reads
    // TODO: properly look into how PIF command triggers work
    PIF::runCommand();

    // Copy 64 bytes from PIF RAM to RDRAM
    for (uint32_t i = 0; i < 0x40; i++)
    {
        uint8_t value = Memory::read<uint8_t>(0x9FC00000 + address + i);
        Memory::write<uint8_t>(0x80000000 + dramAddr + i, value);
    }

    // Request an SI interrupt when the DMA finishes
    // TODO: make DMAs not instant
    MI::setInterrupt(1);
}

void SI::performWriteDma(uint32_t address)
{
    LOG_INFO("SI DMA from RDRAM 0x%X to PIF 0x%X with size 0x40\n", dramAddr, address);

    // Copy 64 bytes from RDRAM to PIF RAM
    for (uint32_t i = 0; i < 0x40; i++)
    {
        uint8_t value = Memory::read<uint8_t>(0x80000000 + dramAddr + i);
        Memory::write<uint8_t>(0x9FC00000 + address + i, value);
    }

    // Request an SI interrupt when the DMA finishes
    // TODO: make DMAs not instant
    MI::setInterrupt(1);
}
