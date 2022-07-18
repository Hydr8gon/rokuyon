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

#include "rsp_cp0.h"
#include "log.h"
#include "memory.h"
#include "mi.h"
#include "rsp.h"

namespace RSP_CP0
{
    uint32_t memAddr;
    uint32_t dramAddr;
    uint32_t status;

    void performReadDma(uint32_t address);
    void performWriteDma(uint32_t address);
}

void RSP_CP0::reset()
{
    // Reset the RSP CP0 to its initial state
    memAddr = 0;
    dramAddr = 0;
    status = 1;
}

uint32_t RSP_CP0::read(int index)
{
    // Read from an RSP CP0 register if one exists at the given address
    switch (index)
    {
        case 4: // SP_STATUS
            // Get the status register
            return status;

        default:
            LOG_WARN("Read from unknown RSP CP0 register: %d\n", index);
            return 0;
    }
}

void RSP_CP0::write(int index, uint32_t value)
{
    // Write to an RSP CP0 register if one exists at the given address
    switch (index)
    {
        case 0: // SP_MEM_ADDR
            // Set the RSP DMA address
            memAddr = value & 0x1FFF;
            return;

        case 1: // SP_DRAM_ADDR
            // Set the RDRAM DMA address
            dramAddr = value & 0xFFFFFF;
            return;

        case 2: // SP_RD_LEN
            // Start a DMA transfer from RDRAM to RSP MEM
            performReadDma((value & 0xFF8) + 1);

            // Keep track of unimplemented bits that should do something
            if (uint32_t bits = (value & 0xFF8FF000))
                LOG_WARN("Unimplemented RSP CP0 read length bits set: 0x%X\n", bits);
            return;

        case 3: // SP_WR_LEN
            // Start a DMA transfer from RSP MEM to RDRAM
            performWriteDma((value & 0xFF8) + 1);

            // Keep track of unimplemented bits that should do something
            if (uint32_t bits = (value & 0xFF8FF000))
                LOG_WARN("Unimplemented RSP CP0 write length bits set: 0x%X\n", bits);
            return;

        case 4: // SP_STATUS
            // Set or clear the halt flag and update the RSP's state
            if (value & 0x1)
                status &= ~0x1;
            else if (value & 0x2)
                status |= 0x1;
            RSP::setState(status & 0x1);

            // Clear the broke flag
            if (value & 0x4)
                status &= ~0x2;

            // Acknowledge or trigger an SP interrupt
            if (value & 0x8)
                MI::clearInterrupt(0);
            else if (value & 0x10)
                MI::setInterrupt(0);

            // Set or clear the remaining status bits
            for (int i = 0; i < 20; i += 2)
            {
                if (value & (1 << (i + 5)))
                    status &= ~(1 << ((i / 2) + 5));
                else if (value & (1 << (i + 6)))
                    status |= (1 << ((i / 2) + 5));
            }

            // Keep track of unimplemented bits that should do something
            if (uint32_t bits = (status & 0x20))
                LOG_WARN("Unimplemented RSP CP0 status bits set: 0x%X\n", bits);
            return;

        default:
            LOG_WARN("Write to unknown RSP CP0 register: %d\n", index);
            return;
    }
}

void RSP_CP0::triggerBreak()
{
    // Trigger an SP interrupt if enabled, halt the RSP, and set the broke flag
    if (status & 0x40)
        MI::setInterrupt(0);
    RSP::setState(true);
    status |= 0x2;
}

void RSP_CP0::performReadDma(uint32_t size)
{
    LOG_INFO("RSP DMA from RDRAM 0x%X to RSP MEM 0x%X with size 0x%X\n", dramAddr, memAddr, size);

    // Copy data from memory to the RSP
    for (uint32_t i = 0; i < size; i += 8)
    {
        uint32_t dst = 0x84000000 + ((memAddr + i) & 0x1FFF);
        uint32_t src = 0x80000000 + dramAddr + i;
        Memory::write<uint64_t>(dst, Memory::read<uint64_t>(src));
    }
}

void RSP_CP0::performWriteDma(uint32_t size)
{
    LOG_INFO("RSP DMA from RDRAM 0x%X to RSP MEM 0x%X with size 0x%X\n", dramAddr, memAddr, size);

    // Copy data from the RSP to memory
    for (uint32_t i = 0; i < size; i += 8)
    {
        uint32_t dst = 0x80000000 + dramAddr + i;
        uint32_t src = 0x84000000 + ((memAddr + i) & 0x1FFF);
        Memory::write<uint64_t>(dst, Memory::read<uint64_t>(src));
    }
}
