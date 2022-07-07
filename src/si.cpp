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
#include "mi.h"

namespace SI
{
    uint32_t dramAddr;
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
            // Set the DMA memory address
            dramAddr = value & 0xFFFFFF;
            return;

        case 0x4800004: // SI_PIF_AD_RD64B
            // Pretend a DMA transfer happened by requesting an SI interrupt
            // TODO: actually implement SI DMAs
            MI::setInterrupt(1);
            return;

        case 0x4800010: // SI_PIF_AD_WR64B
            // Pretend a DMA transfer happened by requesting an SI interrupt
            // TODO: actually implement SI DMAs
            MI::setInterrupt(1);
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
