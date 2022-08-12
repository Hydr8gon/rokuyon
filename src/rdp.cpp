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

#include "rdp.h"
#include "log.h"
#include "memory.h"
#include "mi.h"

namespace RDP
{
    uint32_t startAddr;
    uint32_t endAddr;
    uint32_t status;

    void runCommand();
}

void RDP::reset()
{
    // Reset the RDP to its initial state
    startAddr = 0;
    endAddr = 0;
    status = 0;
}

uint32_t RDP::read(int index)
{
    // Read from an RDP register if one exists at the given index
    switch (index)
    {
        case 0: // DP_START
            // Get the command start address
            return startAddr;

        case 1: // DP_END
        case 2: // DP_CURRENT
            // Get the command end address
            return endAddr;

        case 3: // DP_STATUS
            // Get the status register
            return status;

        default:
            LOG_WARN("Read from unknown RDP register: %d\n", index);
            return 0;
    }
}

void RDP::write(int index, uint32_t value)
{
    // Write to an RDP register if one exists at the given index
    switch (index)
    {
        case 0: // DP_START
            // Set the command start address
            startAddr = value;
            return;

        case 1: // DP_END
            // Set the command end address and run a command
            // TODO: make commands not instant
            endAddr = value;
            runCommand();
            return;

        case 3: // DP_STATUS
            // Set or clear some status bits
            for (int i = 0; i < 6; i += 2)
            {
                if (value & (1 << i))
                    status &= ~(1 << (i / 2));
                else if (value & (1 << (i + 1)))
                    status |= (1 << (i / 2));
            }

            // Keep track of unimplemented bits that should do something
            if (uint32_t bits = (value & 0x3C0))
                LOG_WARN("Unimplemented RDP status bits set: 0x%X\n", bits);
            return;

        default:
            LOG_WARN("Write to unknown RDP register: %d\n", index);
            return;
    }
}

void RDP::runCommand()
{
    // Read an RDP opcode from either RSP DMEM or RDRAM
    uint32_t base = (status & 0x1) ? 0xA4000000 : 0xA0000000;
    uint32_t mask = (status & 0x1) ? 0x00000FFF : 0x003FFFFF;
    uint32_t address = base + (startAddr & mask);
    uint64_t opcode = Memory::read<uint64_t>(address);

    // Trigger a DP interrupt on a Sync Full command
    if (((opcode >> 56) & 0x3F) == 0x29)
        MI::setInterrupt(5);

    // Stub RDP commands for now
    LOG_CRIT("Unknown RDP opcode: 0x%lX @ 0x%X\n", opcode, address);
    startAddr = endAddr;
}
