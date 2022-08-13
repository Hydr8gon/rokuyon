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
    extern void (*commands[])(uint64_t);

    uint32_t startAddr;
    uint32_t endAddr;
    uint32_t status;

    uint32_t colorBuffer;
    uint32_t colorWidth;
    uint32_t colorShift;
    uint32_t fillColor;

    void runCommand();
    void texRectangle(uint64_t opcode);
    void syncFull(uint64_t opcode);
    void fillRectangle(uint64_t opcode);
    void setFillColor(uint64_t opcode);
    void setColorImage(uint64_t opcode);
    void unknown(uint64_t opcode);
}

// RDP command lookup table, based on opcode bits 56-61
void (*RDP::commands[0x40])(uint64_t) =
{
    unknown,      unknown,  unknown,       unknown,      // 0x00-0x03
    unknown,      unknown,  unknown,       unknown,      // 0x04-0x07
    unknown,      unknown,  unknown,       unknown,      // 0x08-0x0B
    unknown,      unknown,  unknown,       unknown,      // 0x0C-0x0F
    unknown,      unknown,  unknown,       unknown,      // 0x10-0x13
    unknown,      unknown,  unknown,       unknown,      // 0x14-0x17
    unknown,      unknown,  unknown,       unknown,      // 0x18-0x1B
    unknown,      unknown,  unknown,       unknown,      // 0x1C-0x1F
    unknown,      unknown,  unknown,       unknown,      // 0x20-0x23
    texRectangle, unknown,  unknown,       unknown,      // 0x24-0x27
    unknown,      syncFull, unknown,       unknown,      // 0x28-0x2B
    unknown,      unknown,  unknown,       unknown,      // 0x2C-0x2F
    unknown,      unknown,  unknown,       unknown,      // 0x30-0x33
    unknown,      unknown,  fillRectangle, setFillColor, // 0x34-0x37
    unknown,      unknown,  unknown,       unknown,      // 0x38-0x3B
    unknown,      unknown,  unknown,       setColorImage // 0x3C-0x3F
};

void RDP::reset()
{
    // Reset the RDP to its initial state
    startAddr = 0;
    endAddr = 0;
    status = 0;
    colorBuffer = 0;
    colorWidth = 0;
    colorShift = 0;
    fillColor = 0;
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
    // Execute an RDP opcode from either RSP DMEM or RDRAM
    uint32_t base = (status & 0x1) ? 0xA4000000 : 0xA0000000;
    uint32_t mask = (status & 0x1) ? 0x00000FFF : 0x003FFFFF;
    uint32_t address = base + (startAddr & mask);
    uint64_t opcode = Memory::read<uint64_t>(address);
    (*commands[(opcode >> 56) & 0x3F])(opcode);
    startAddr = endAddr;
}

void RDP::texRectangle(uint64_t opcode)
{
    // Decode the operands
    uint16_t y1 = ((opcode >>  0) & 0xFFF) >> 2;
    uint16_t x1 = ((opcode >> 12) & 0xFFF) >> colorShift;
    uint16_t y2 = ((opcode >> 32) & 0xFFF) >> 2;
    uint16_t x2 = ((opcode >> 44) & 0xFFF) >> colorShift;

    // Draw a rectangle using the inverse of the fill color
    // TODO: actually draw textures
    for (uint16_t y = y1; y < y2; y++)
        for (uint16_t x = x1; x < x2; x += 4)
            Memory::write<uint32_t>(colorBuffer + y * (colorWidth >> colorShift) + x, ~fillColor);
}

void RDP::syncFull(uint64_t opcode)
{
    // Trigger a DP interrupt right away
    // TODO: make things not execute instantly
    MI::setInterrupt(5);
}

void RDP::fillRectangle(uint64_t opcode)
{
    // Decode the operands
    uint16_t y1 = ((opcode >>  0) & 0xFFF) >> 2;
    uint16_t x1 = ((opcode >> 12) & 0xFFF) >> colorShift;
    uint16_t y2 = ((opcode >> 32) & 0xFFF) >> 2;
    uint16_t x2 = ((opcode >> 44) & 0xFFF) >> colorShift;

    // Draw a rectangle using the fill color
    for (uint16_t y = y1; y < y2; y++)
        for (uint16_t x = x1; x < x2; x += 4)
            Memory::write<uint32_t>(colorBuffer + y * (colorWidth >> colorShift) + x, fillColor);
}

void RDP::setFillColor(uint64_t opcode)
{
    // Set the fill color
    fillColor = opcode;
}

void RDP::setColorImage(uint64_t opcode)
{
    // Set the color buffer parameters
    // TODO: handle color format bits
    colorBuffer = 0xA0000000 + (opcode & 0x3FFFFF);
    colorWidth = (((opcode >> 32) & 0x3FF) + 1) << 2;
    colorShift = ((opcode >> 51) & 0x3) ^ 0x3;
}

void RDP::unknown(uint64_t opcode)
{
    // Warn about unknown commands
    LOG_CRIT("Unknown RDP opcode: 0x%lX @ 0x%X\n", opcode, address);
}
