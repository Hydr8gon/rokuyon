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

#include "pi.h"
#include "log.h"
#include "memory.h"
#include "mi.h"

namespace PI
{
    uint32_t dramAddr;
    uint32_t cartAddr;

    void performReadDma(uint32_t length);
    void performWriteDma(uint32_t length);
}

void PI::reset()
{
    // Reset the PI to its initial state
    dramAddr = 0;
    cartAddr = 0;
}

uint32_t PI::read(uint32_t address)
{
    // Read from an I/O register if one exists at the given address
    switch (address)
    {
        default:
            LOG_WARN("Unknown PI register read: 0x%X\n", address);
            return 0;
    }
}

void PI::write(uint32_t address, uint32_t value)
{
    // Write to an I/O register if one exists at the given address
    switch (address)
    {
        case 0x4600000: // PI_DRAM_ADDR
            // Set the RDRAM DMA address
            dramAddr = value & 0xFFFFFF;
            return;

        case 0x4600004: // PI_CART_ADDR
            // Set the cart DMA address
            cartAddr = value;
            return;

        case 0x4600008: // PI_RD_LEN
            // Start a DMA transfer from RDRAM to PI
            performWriteDma((value & 0xFFFFFF) + 1);
            return;

        case 0x460000C: // PI_WR_LEN
            // Start a DMA transfer from PI to RDRAM
            performReadDma((value & 0xFFFFFF) + 1);
            return;

        case 0x4600010: // PI_STATUS
            // Acknowledge a PI interrupt when bit 1 is set
            // TODO: handle bit 0
            if (value & 0x2)
                MI::clearInterrupt(4);
            return;

        default:
            LOG_WARN("Unknown PI register write: 0x%X\n", address);
            return;
    }
}

void PI::performReadDma(uint32_t size)
{
    LOG_INFO("PI DMA from cart 0x%X to RDRAM 0x%X with size 0x%X\n", cartAddr, dramAddr, size);

    // Copy data from the PI bus to memory
    // TODO: check bounds
    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t value = Memory::read<uint8_t>(0x80000000 + cartAddr + i);
        Memory::write<uint8_t>(0x80000000 + dramAddr + i, value);
    }

    // Request a PI interrupt when the DMA finishes
    // TODO: make DMAs not instant
    MI::setInterrupt(4);
}


void PI::performWriteDma(uint32_t size)
{
    LOG_INFO("PI DMA from RDRAM 0x%X to cart 0x%X with size 0x%X\n", dramAddr, cartAddr, size);

    // Copy data from memory to the PI bus
    // TODO: check bounds
    for (uint32_t i = 0; i < size; i++)
    {
        uint8_t value = Memory::read<uint8_t>(0x80000000 + dramAddr + i);
        Memory::write<uint8_t>(0x80000000 + cartAddr + i, value);
    }

    // Request a PI interrupt when the DMA finishes
    // TODO: make DMAs not instant
    MI::setInterrupt(4);
}
