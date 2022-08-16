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

#include <cstring>

#include "rdp.h"
#include "log.h"
#include "memory.h"
#include "mi.h"

enum Format
{
    RGBA4, RGBA8, RGBA16, RGBA32,
    YUV4,  YUV8,  YUV16,  YUV32,
    CI4,   CI8,   CI16,   CI32,
    IA4,   IA8,   IA16,   IA32,
    I4,    I8,    I16,    I32
};

enum CycleType
{
    ONE_CYCLE, TWO_CYCLE, COPY_MODE, FILL_MODE
};

struct Tile
{
    uint16_t address;
    uint16_t width;
    Format format;
};

namespace RDP
{
    extern void (*commands[])(uint64_t);

    uint8_t tmem[0x1000]; // 4KB TMEM
    uint32_t startAddr;
    uint32_t endAddr;
    uint32_t status;

    uint32_t addrBase;
    uint32_t addrMask;
    uint64_t texRectOp;

    CycleType cycleType;
    uint32_t fillColor;
    uint32_t texAddress;
    uint16_t texWidth;
    Format texFormat;
    uint32_t colorAddress;
    uint16_t colorWidth;
    Format colorFormat;
    Tile tiles[8];

    uint32_t RGBA16toRGBA32(uint16_t color);
    uint16_t RGBA32toRGBA16(uint32_t color);

    void runCommand();
    void texRectangle(uint64_t opcode);
    void texRectangle2(uint64_t opcode);
    void syncFull(uint64_t opcode);
    void setOtherModes(uint64_t opcode);
    void loadTile(uint64_t opcode);
    void setTile(uint64_t opcode);
    void fillRectangle(uint64_t opcode);
    void setFillColor(uint64_t opcode);
    void setTexImage(uint64_t opcode);
    void setColorImage(uint64_t opcode);
    void unknown(uint64_t opcode);
}

// RDP command lookup table, based on opcode bits 56-61
void (*RDP::commands[0x40])(uint64_t) =
{
    unknown,      unknown,     unknown,       unknown,       // 0x00-0x03
    unknown,      unknown,     unknown,       unknown,       // 0x04-0x07
    unknown,      unknown,     unknown,       unknown,       // 0x08-0x0B
    unknown,      unknown,     unknown,       unknown,       // 0x0C-0x0F
    unknown,      unknown,     unknown,       unknown,       // 0x10-0x13
    unknown,      unknown,     unknown,       unknown,       // 0x14-0x17
    unknown,      unknown,     unknown,       unknown,       // 0x18-0x1B
    unknown,      unknown,     unknown,       unknown,       // 0x1C-0x1F
    unknown,      unknown,     unknown,       unknown,       // 0x20-0x23
    texRectangle, unknown,     unknown,       unknown,       // 0x24-0x27
    unknown,      syncFull,    unknown,       unknown,       // 0x28-0x2B
    unknown,      unknown,     unknown,       setOtherModes, // 0x2C-0x2F
    unknown,      unknown,     unknown,       unknown,       // 0x30-0x33
    loadTile,     setTile,     fillRectangle, setFillColor,  // 0x34-0x37
    unknown,      unknown,     unknown,       unknown,       // 0x38-0x3B
    unknown,      setTexImage, unknown,       setColorImage  // 0x3C-0x3F
};

inline uint32_t RDP::RGBA16toRGBA32(uint16_t color)
{
    // Convert an RGBA16 color to RGBA32
    uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((color >>  6) & 0x1F) * 255 / 31;
    uint8_t b = ((color >>  1) & 0x1F) * 255 / 31;
    uint8_t a = (color & 0x1) ? 0xFF : 0x00;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

inline uint16_t RDP::RGBA32toRGBA16(uint32_t color)
{
    // Convert an RGBA32 color to RGBA16
    uint8_t r = ((color >> 24) & 0xFF) * 31 / 255;
    uint8_t g = ((color >> 16) & 0xFF) * 31 / 255;
    uint8_t b = ((color >>  8) & 0xFF) * 31 / 255;
    uint8_t a = (color & 0xFF) ? 0x1 : 0x0;
    return (r << 11) | (g << 6) | (b << 1) | a;
}

void RDP::reset()
{
    // Reset the RDP to its initial state
    startAddr = 0;
    endAddr = 0;
    status = 0;
    addrBase = 0;
    addrMask = 0;
    texRectOp = 0;
    cycleType = ONE_CYCLE;
    fillColor = 0;
    texAddress = 0;
    texWidth = 0;
    texFormat = RGBA4;
    colorAddress = 0;
    colorWidth = 0;
    colorFormat = RGBA4;
    memset(tiles, 0, sizeof(tiles));
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

            // Update the command address base and mask based on the RDRAM/DMEM bit
            addrBase = (status & 0x1) ? 0xA4000000 : 0xA0000000;
            addrMask = (status & 0x1) ? 0x00000FFF : 0x003FFFFF;

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
    // Execute the RDP opcode at the command start address
    uint64_t opcode = Memory::read<uint64_t>(addrBase + (startAddr & addrMask));
    texRectOp ? texRectangle2(opcode) : (*commands[(opcode >> 56) & 0x3F])(opcode);
    startAddr = endAddr;
}

void RDP::texRectangle(uint64_t opcode)
{
    // Store the first 64 bits of the command
    texRectOp = opcode;

    // If the second 64 bits are included in this transfer, process them right away
    if (endAddr - startAddr >= 16)
        texRectangle2(Memory::read<uint64_t>(addrBase + ((startAddr + 8) & addrMask)));
}

void RDP::texRectangle2(uint64_t opcode)
{
    // Decode the operands and reset the stored bits
    // TODO: actually use the texture coordinate bits
    Tile &tile = tiles[(texRectOp >> 24) & 0x7];
    uint16_t y1 = ((texRectOp >>  0) & 0xFFF) >> 2;
    uint16_t x1 = ((texRectOp >> 12) & 0xFFF) >> 2;
    uint16_t y2 = ((texRectOp >> 32) & 0xFFF) >> 2;
    uint16_t x2 = ((texRectOp >> 44) & 0xFFF) >> 2;
    int16_t dtdy = opcode >>  0;
    int16_t dsdx = opcode >> 16;
    texRectOp = 0;

    // Adjust some things based on the cycle type
    // TODO: hardware test this
    if (cycleType >= COPY_MODE)
    {
        dsdx >>= 2;
        x2++;
        y2++;
    }

    switch (colorFormat)
    {
        case RGBA16:
            switch (tile.format)
            {
                case RGBA16:
                    // Draw a rectangle to an RGBA16 buffer using an RGBA16 texture
                    for (int y = y1, t = 0; y < y2; y++, t += dtdy)
                        for (int x = x1, s = 0; x < x2; x++, s += dsdx)
                            Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2,
                                *(uint16_t*)&tmem[(tile.address + (t >> 10) * tile.width + (s >> 10) * 2) & 0xFFE]);
                    return;

                case RGBA32:
                    // Draw a rectangle to an RGBA16 buffer using an RGBA32 texture
                    for (int y = y1, t = 0; y < y2; y++, t += dtdy)
                        for (int x = x1, s = 0; x < x2; x++, s += dsdx)
                            Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2, RGBA32toRGBA16(
                                *(uint32_t*)&tmem[(tile.address + (t >> 10) * tile.width * 2 + (s >> 10) * 4) & 0xFFC]));
                    return;

                default:
                    LOG_CRIT("Unknown RDP texture format: %d\n", tile.format);
                    return;
            }

        case RGBA32:
            switch (tile.format)
            {
                case RGBA16:
                    // Draw a rectangle to an RGBA32 buffer using an RGBA16 texture
                    for (int y = y1, t = 0; y < y2; y++, t += dtdy)
                        for (int x = x1, s = 0; x < x2; x++, s += dsdx)
                            Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4, RGBA16toRGBA32(
                                *(uint16_t*)&tmem[(tile.address + (t >> 10) * tile.width + (s >> 10) * 2) & 0xFFE]));
                    return;

                case RGBA32:
                    // Draw a rectangle to an RGBA32 buffer using an RGBA32 texture
                    for (int y = y1, t = 0; y < y2; y++, t += dtdy)
                        for (int x = x1, s = 0; x < x2; x++, s += dsdx)
                            Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4,
                                *(uint32_t*)&tmem[(tile.address + (t >> 10) * tile.width * 2 + (s >> 10) * 4) & 0xFFC]);
                    return;

                default:
                    LOG_CRIT("Unknown RDP texture format: %d\n", tile.format);
                    return;
            }

        default:
            LOG_CRIT("Unknown RDP color buffer format: %d\n", colorFormat);
            return;
    }
}

void RDP::syncFull(uint64_t opcode)
{
    // Trigger a DP interrupt right away because everything finishes instantly
    MI::setInterrupt(5);
}

void RDP::setOtherModes(uint64_t opcode)
{
    // Set the cycle type
    // TODO: actually use the other bits
    cycleType = (CycleType)((opcode >> 52) & 0x3);
}

void RDP::loadTile(uint64_t opcode)
{
    // Decode the operands
    Tile &tile = tiles[(opcode >> 24) & 0x7];
    uint16_t t2 = ((opcode >>  0) & 0xFFF) >> 2;
    uint16_t s2 = ((opcode >> 12) & 0xFFF) >> 2;
    uint16_t t1 = ((opcode >> 32) & 0xFFF) >> 2;
    uint16_t s1 = ((opcode >> 44) & 0xFFF) >> 2;

    // Only support loading textures without conversion for now
    if (texFormat != tile.format)
    {
        LOG_CRIT("Unimplemented RDP texture conversion: %d to %d\n", texFormat, tile.format);
        return;
    }

    switch (texFormat & 0x3)
    {
        case 0x0: // 4-bit
            // Copy a 4-bit texture from the texture buffer to a tile address in TMEM
            // TODO: arrange the data properly
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s += 2)
                    tmem[(tile.address + (t - t1) * tile.width + (s - s1) / 2) & 0xFFF] =
                        Memory::read<uint8_t>(texAddress + (t * texWidth + s) / 2);
            return;

        case 0x1: // 8-bit
            // Copy an 8-bit texture from the texture buffer to a tile address in TMEM
            // TODO: arrange the data properly
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s++)
                    tmem[(tile.address + (t - t1) * tile.width + (s - s1)) & 0xFFF] =
                        Memory::read<uint8_t>(texAddress + t * texWidth + s);
            return;

        case 0x2: // 16-bit
            // Copy a 16-bit texture from the texture buffer to a tile address in TMEM
            // TODO: arrange the data properly
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s++)
                    *(uint16_t*)&tmem[(tile.address + (t - t1) * tile.width + (s - s1) * 2) & 0xFFE] =
                        Memory::read<uint16_t>(texAddress + (t * texWidth + s) * 2);
            return;

        case 0x3: // 32-bit
            // Copy a 32-bit texture from the texture buffer to a tile address in TMEM
            // TODO: arrange the data properly and split across high and low banks
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s++)
                    *(uint32_t*)&tmem[(tile.address + (t - t1) * tile.width * 2 + (s - s1) * 4) & 0xFFC] =
                        Memory::read<uint32_t>(texAddress + (t * texWidth + s) * 4);
            return;
    }
}

void RDP::setTile(uint64_t opcode)
{
    // Set parameters for the specified tile
    // TODO: Actually use bits 0-23
    Tile &tile = tiles[(opcode >> 24) & 0x7];
    tile.address = (opcode >> 29) & 0xFF8;
    tile.width = (opcode >> 38) & 0xFF8;
    tile.format = (Format)((opcode >> 51) & 0x1F);
}

void RDP::fillRectangle(uint64_t opcode)
{
    // Decode the operands
    uint16_t y1 = ((opcode >>  0) & 0xFFF) >> 2;
    uint16_t x1 = ((opcode >> 12) & 0xFFF) >> 2;
    uint16_t y2 = ((opcode >> 32) & 0xFFF) >> 2;
    uint16_t x2 = ((opcode >> 44) & 0xFFF) >> 2;

    // Adjust some things based on the cycle type
    // TODO: hardware test this
    if (cycleType >= COPY_MODE)
    {
        x2++;
        y2++;
    }

    switch (colorFormat)
    {
        case RGBA16:
            // Draw a rectangle to an RGBA16 buffer using the fill color
            for (int y = y1; y < y2; y++)
                for (int x = x1; x < x2; x++)
                    Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2, fillColor >> ((~x & 1) * 16));
            return;

        case RGBA32:
            // Draw a rectangle to an RGBA32 buffer using the fill color
            for (int y = y1; y < y2; y++)
                for (int x = x1; x < x2; x++)
                    Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4, fillColor);
            return;

        default:
            LOG_CRIT("Unknown RDP color buffer format: %d\n", colorFormat);
            return;
    }
}

void RDP::setFillColor(uint64_t opcode)
{
    // Set the fill color
    fillColor = opcode;
}

void RDP::setTexImage(uint64_t opcode)
{
    // Set the texture buffer parameters
    texAddress = 0xA0000000 + (opcode & 0x3FFFFF);
    texWidth = ((opcode >> 32) & 0x3FF) + 1;
    texFormat = (Format)((opcode >> 51) & 0x1F);
}

void RDP::setColorImage(uint64_t opcode)
{
    // Set the color buffer parameters
    colorAddress = 0xA0000000 + (opcode & 0x3FFFFF);
    colorWidth = ((opcode >> 32) & 0x3FF) + 1;
    colorFormat = (Format)((opcode >> 51) & 0x1F);
}

void RDP::unknown(uint64_t opcode)
{
    // Warn about unknown commands
    LOG_CRIT("Unknown RDP opcode: 0x%016lX\n", opcode);
}
