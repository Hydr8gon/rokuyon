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
    extern void (*commands[])();
    extern uint8_t paramCounts[];

    uint8_t tmem[0x1000]; // 4KB TMEM
    uint32_t startAddr;
    uint32_t endAddr;
    uint32_t status;

    uint32_t addrBase;
    uint32_t addrMask;
    uint8_t paramCount;
    uint64_t opcode[22];

    CycleType cycleType;
    uint32_t fillColor;
    uint32_t texAddress;
    uint16_t texWidth;
    Format texFormat;
    uint32_t colorAddress;
    uint16_t colorWidth;
    Format colorFormat;
    Tile tiles[8];

    uint16_t scissorX1;
    uint16_t scissorX2;
    uint16_t scissorY1;
    uint16_t scissorY2;

    uint32_t RGBA16toRGBA32(uint16_t color);
    uint16_t RGBA32toRGBA16(uint32_t color);
    uint16_t IA8toRGBA16(uint8_t color);
    uint32_t IA8toRGBA32(uint8_t color);

    void runCommands();
    void triangle();
    void texRectangle();
    void syncFull();
    void setScissor();
    void setOtherModes();
    void loadBlock();
    void loadTile();
    void setTile();
    void fillRectangle();
    void setFillColor();
    void setTexImage();
    void setColorImage();
    void unknown();
}

// RDP command lookup table, based on opcode bits 56-61
void (*RDP::commands[0x40])() =
{
    unknown,      unknown,     unknown,       unknown,       // 0x00-0x03
    unknown,      unknown,     unknown,       unknown,       // 0x04-0x07
    triangle,     triangle,    triangle,      triangle,      // 0x08-0x0B
    triangle,     triangle,    triangle,      triangle,      // 0x0C-0x0F
    unknown,      unknown,     unknown,       unknown,       // 0x10-0x13
    unknown,      unknown,     unknown,       unknown,       // 0x14-0x17
    unknown,      unknown,     unknown,       unknown,       // 0x18-0x1B
    unknown,      unknown,     unknown,       unknown,       // 0x1C-0x1F
    unknown,      unknown,     unknown,       unknown,       // 0x20-0x23
    texRectangle, unknown,     unknown,       unknown,       // 0x24-0x27
    unknown,      syncFull,    unknown,       unknown,       // 0x28-0x2B
    unknown,      setScissor,  unknown,       setOtherModes, // 0x2C-0x2F
    unknown,      unknown,     unknown,       loadBlock,     // 0x30-0x33
    loadTile,     setTile,     fillRectangle, setFillColor,  // 0x34-0x37
    unknown,      unknown,     unknown,       unknown,       // 0x38-0x3B
    unknown,      setTexImage, unknown,       setColorImage  // 0x3C-0x3F
};

uint8_t RDP::paramCounts[0x40] =
{
    1, 1,  1,  1,  1,  1,  1,  1, // 0x00-0x07
    4, 6, 12, 14, 12, 14, 20, 22, // 0x08-0x0F
    1, 1,  1,  1,  1,  1,  1,  1, // 0x10-0x17
    1, 1,  1,  1,  1,  1,  1,  1, // 0x18-0x1F
    1, 1,  1,  1,  2,  2,  1,  1, // 0x20-0x27
    1, 1,  1,  1,  1,  1,  1,  1, // 0x28-0x2F
    1, 1,  1,  1,  1,  1,  1,  1, // 0x30-0x37
    1, 1,  1,  1,  1,  1,  1,  1  // 0x38-0x3F
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

inline uint16_t RDP::IA8toRGBA16(uint8_t color)
{
    // Convert an IA8 color to RGBA16
    uint8_t i = ((color >> 4) & 0xF) * 255 / 15;
    uint8_t a = ((color >> 0) & 0xF) * 255 / 15;
    return (i << 11) | (i << 6) | (i << 1) | a;
}

inline uint32_t RDP::IA8toRGBA32(uint8_t color)
{
    // Convert an IA8 color to RGBA32
    uint8_t i = ((color >> 4) & 0xF) * 31 / 15;
    uint8_t a = (color & 0xF) ? 0x1 : 0x0;
    return (i << 24) | (i << 16) | (i << 8) | a;
}

void RDP::reset()
{
    // Reset the RDP to its initial state
    memset(tmem, 0, sizeof(tmem));
    startAddr = 0;
    endAddr = 0;
    status = 0;
    addrBase = 0xA0000000;
    addrMask = 0x3FFFFF;
    paramCount = 0;
    cycleType = ONE_CYCLE;
    fillColor = 0;
    texAddress = 0xA0000000;
    texWidth = 0;
    texFormat = RGBA4;
    colorAddress = 0xA0000000;
    colorWidth = 0;
    colorFormat = RGBA4;
    memset(tiles, 0, sizeof(tiles));
    scissorX1 = 0;
    scissorX2 = 0;
    scissorY1 = 0;
    scissorY2 = 0;
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
            // Set the command end address and run the commands
            // TODO: make commands not instant
            endAddr = value;
            runCommands();
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

void RDP::runCommands()
{
    // Process RDP commands until the end address is reached
    while (startAddr < endAddr)
    {
        // Add a parameter to the buffer
        opcode[paramCount++] = Memory::read<uint64_t>(addrBase + (startAddr & addrMask));

        // Execute a command once all of its parameters have been received
        if (paramCount >= paramCounts[(opcode[0] >> 56) & 0x3F])
        {
            (*commands[(opcode[0] >> 56) & 0x3F])();
            paramCount = 0;
        }

        // Move to the next parameter
        startAddr += 8;
    }
}

void RDP::triangle()
{
    // Get the vertex Y coordinates and sort them for each edge
    int y1 = (int16_t)(opcode[0] <<  2) >> 4;
    int y2 = (int16_t)(opcode[0] >> 14) >> 4;
    int y3 = (int16_t)(opcode[0] >> 30) >> 4;
    int ys[3][2] = { { y2, y3 }, { y1, y3 }, { y1, y2 } };

    // Draw the lower, higher, and middle edges
    // TODO: properly implement triangles
    for (int i = 0; i < 3; i++)
    {
        // Get the slope and starting X-coordinate
        int32_t slope = opcode[i + 1];
        int32_t x1 = opcode[i + 1] >> 32;
        int32_t x = x1;

        // Draw a line between two Y-coordinates, incrementing X along the way
        for (int y = ys[i][0]; y < ys[i][1]; y++)
        {
            int32_t x0 = x >> 16;
            if (x0 >= scissorX1 && x0 < scissorX2 && y >= scissorY1 && y < scissorY2)
                Memory::write<uint16_t>(colorAddress + (y * colorWidth + x0) * 2, ~fillColor);
            x += slope;
        }
    }
}

void RDP::texRectangle()
{
    // Decode the operands
    // TODO: actually use the texture coordinate bits
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    uint16_t y1 = ((opcode[0] >>  0) & 0xFFF) >> 2;
    uint16_t x1 = ((opcode[0] >> 12) & 0xFFF) >> 2;
    uint16_t y2 = ((opcode[0] >> 32) & 0xFFF) >> 2;
    uint16_t x2 = ((opcode[0] >> 44) & 0xFFF) >> 2;
    int16_t dtdy = opcode[1] >>  0;
    int16_t dsdx = opcode[1] >> 16;

    // Adjust some things based on the cycle type
    // TODO: hardware test this
    if (cycleType >= COPY_MODE)
    {
        dsdx >>= 2;
        x2++;
        y2++;
    }

    // Clip the coordinates to be within scissor bounds
    x1 = std::max(x1, scissorX1);
    x2 = std::min(x2, scissorX2);
    y1 = std::max(y1, scissorY1);
    y2 = std::min(y2, scissorY2);

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

                case IA8:
                    // Draw a rectangle to an RGBA16 buffer using an IA8 texture
                    for (int y = y1, t = 0; y < y2; y++, t += dtdy)
                        for (int x = x1, s = 0; x < x2; x++, s += dsdx)
                            Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2, IA8toRGBA16(
                                tmem[(tile.address + (t >> 10) * tile.width + (s >> 10)) & 0xFFF]));
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

                case IA8:
                    // Draw a rectangle to an RGBA32 buffer using an IA8 texture
                    for (int y = y1, t = 0; y < y2; y++, t += dtdy)
                        for (int x = x1, s = 0; x < x2; x++, s += dsdx)
                            Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4, IA8toRGBA32(
                                    tmem[(tile.address + (t >> 10) * tile.width + (s >> 10)) & 0xFFF]));
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

void RDP::syncFull()
{
    // Trigger a DP interrupt right away because everything finishes instantly
    MI::setInterrupt(5);
}

void RDP::setScissor()
{
    // Set the scissor bounds
    // TODO: actually use the scissor field bits
    scissorY2 = ((opcode[0] >>  0) & 0xFFF) >> 2;
    scissorX2 = ((opcode[0] >> 12) & 0xFFF) >> 2;
    scissorY1 = ((opcode[0] >> 32) & 0xFFF) >> 2;
    scissorX1 = ((opcode[0] >> 44) & 0xFFF) >> 2;
}

void RDP::setOtherModes()
{
    // Set the cycle type
    // TODO: actually use the other bits
    cycleType = (CycleType)((opcode[0] >> 52) & 0x3);
}

void RDP::loadBlock()
{
    // Decode the operands
    // TODO: actually use the other bits
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    uint16_t count = ((opcode[0] >> 12) & 0xFFF);

    switch (texFormat & 0x3)
    {
        case 0x0: // 4-bit
            // Copy 4-bit texture data from the texture buffer to TMEM
            // TODO: arrange the data properly
            for (int i = 0; i <= count / 2; i++)
                tmem[(tile.address + i) & 0xFFF] = Memory::read<uint8_t>(texAddress + i);
            return;

        case 0x1: // 8-bit
            // Copy 8-bit texture data from the texture buffer to TMEM
            // TODO: arrange the data properly
            for (int i = 0; i <= count; i++)
                tmem[(tile.address + i) & 0xFFF] = Memory::read<uint8_t>(texAddress + i);
            return;

        case 0x2: // 16-bit
            // Copy 16-bit texture data from the texture buffer to TMEM
            // TODO: arrange the data properly
            for (int i = 0; i <= count * 2; i += 2)
                *(uint16_t*)&tmem[(tile.address + i) & 0xFFE] = Memory::read<uint16_t>(texAddress + i);
            return;

        case 0x3: // 32-bit
            // Copy 32-bit texture data from the texture buffer to TMEM
            // TODO: arrange the data properly and split across high and low banks
            for (int i = 0; i <= count * 4; i += 4)
                *(uint32_t*)&tmem[(tile.address + i) & 0xFFC] = Memory::read<uint32_t>(texAddress + i);
            return;
    }
}

void RDP::loadTile()
{
    // Decode the operands
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    uint16_t t2 = ((opcode[0] >>  0) & 0xFFF) >> 2;
    uint16_t s2 = ((opcode[0] >> 12) & 0xFFF) >> 2;
    uint16_t t1 = ((opcode[0] >> 32) & 0xFFF) >> 2;
    uint16_t s1 = ((opcode[0] >> 44) & 0xFFF) >> 2;

    // Only support loading textures without conversion for now
    if (texFormat != tile.format)
    {
        LOG_CRIT("Unimplemented RDP texture conversion: %d to %d\n", texFormat, tile.format);
        return;
    }

    switch (texFormat & 0x3)
    {
        case 0x0: // 4-bit
            // Cut out a 4-bit texture from the textrue buffer and copy it to TMEM
            // TODO: arrange the data properly
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s += 2)
                    tmem[(tile.address + (t - t1) * tile.width + (s - s1) / 2) & 0xFFF] =
                        Memory::read<uint8_t>(texAddress + (t * texWidth + s) / 2);
            return;

        case 0x1: // 8-bit
            // Cut out an 8-bit texture from the textrue buffer and copy it to TMEM
            // TODO: arrange the data properly
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s++)
                    tmem[(tile.address + (t - t1) * tile.width + (s - s1)) & 0xFFF] =
                        Memory::read<uint8_t>(texAddress + t * texWidth + s);
            return;

        case 0x2: // 16-bit
            // Cut out a 16-bit texture from the textrue buffer and copy it to TMEM
            // TODO: arrange the data properly
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s++)
                    *(uint16_t*)&tmem[(tile.address + (t - t1) * tile.width + (s - s1) * 2) & 0xFFE] =
                        Memory::read<uint16_t>(texAddress + (t * texWidth + s) * 2);
            return;

        case 0x3: // 32-bit
            // Cut out a 32-bit texture from the textrue buffer and copy it to TMEM
            // TODO: arrange the data properly and split across high and low banks
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s++)
                    *(uint32_t*)&tmem[(tile.address + (t - t1) * tile.width * 2 + (s - s1) * 4) & 0xFFC] =
                        Memory::read<uint32_t>(texAddress + (t * texWidth + s) * 4);
            return;
    }
}

void RDP::setTile()
{
    // Set parameters for the specified tile
    // TODO: Actually use bits 0-23
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    tile.address = (opcode[0] >> 29) & 0xFF8;
    tile.width = (opcode[0] >> 38) & 0xFF8;
    tile.format = (Format)((opcode[0] >> 51) & 0x1F);
}

void RDP::fillRectangle()
{
    // Decode the operands
    uint16_t y1 = ((opcode[0] >>  0) & 0xFFF) >> 2;
    uint16_t x1 = ((opcode[0] >> 12) & 0xFFF) >> 2;
    uint16_t y2 = ((opcode[0] >> 32) & 0xFFF) >> 2;
    uint16_t x2 = ((opcode[0] >> 44) & 0xFFF) >> 2;

    // Adjust some things based on the cycle type
    // TODO: hardware test this
    if (cycleType >= COPY_MODE)
    {
        x2++;
        y2++;
    }

    // Clip the coordinates to be within scissor bounds
    x1 = std::max(x1, scissorX1);
    x2 = std::min(x2, scissorX2);
    y1 = std::max(y1, scissorY1);
    y2 = std::min(y2, scissorY2);

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

void RDP::setFillColor()
{
    // Set the fill color
    fillColor = opcode[0];
}

void RDP::setTexImage()
{
    // Set the texture buffer parameters
    texAddress = 0xA0000000 + (opcode[0] & 0x3FFFFF);
    texWidth = ((opcode[0] >> 32) & 0x3FF) + 1;
    texFormat = (Format)((opcode[0] >> 51) & 0x1F);
}

void RDP::setColorImage()
{
    // Set the color buffer parameters
    colorAddress = 0xA0000000 + (opcode[0] & 0x3FFFFF);
    colorWidth = ((opcode[0] >> 32) & 0x3FF) + 1;
    colorFormat = (Format)((opcode[0] >> 51) & 0x1F);
}

void RDP::unknown()
{
    // Warn about unknown commands
    LOG_CRIT("Unknown RDP opcode: 0x%016lX\n", opcode[0]);
}
