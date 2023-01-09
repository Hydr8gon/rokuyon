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
    uint16_t sBase;
    uint16_t sMask;
    bool sMirror;
    bool sClamp;

    uint16_t tBase;
    uint16_t tMask;
    bool tMirror;
    bool tClamp;
    
    uint16_t address;
    uint16_t width;
    uint8_t palette;
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
    uint8_t blendA[2];
    uint8_t blendB[2];
    uint8_t blendC[2];
    uint8_t blendD[2];

    uint32_t texAddress;
    uint16_t texWidth;
    Format texFormat;
    uint32_t zAddress;
    uint32_t colorAddress;
    uint16_t colorWidth;
    Format colorFormat;
    Tile tiles[8];

    uint16_t scissorX1;
    uint16_t scissorX2;
    uint16_t scissorY1;
    uint16_t scissorY2;

    uint32_t fillColor;
    uint32_t combColor;
    uint32_t texelColor;
    uint32_t primColor;
    uint32_t shadeColor;
    uint32_t envColor;
    uint32_t combAlpha;
    uint32_t texelAlpha;
    uint32_t primAlpha;
    uint32_t shadeAlpha;
    uint32_t envAlpha;
    uint32_t fogColor;
    uint32_t blendColor;
    uint32_t pixelAlpha;
    uint32_t memColor;
    uint32_t maxColor;
    uint32_t minColor;

    uint32_t *combineA[4];
    uint32_t *combineB[4];
    uint32_t *combineC[4];
    uint32_t *combineD[4];

    uint32_t RGBA16toRGBA32(uint16_t color);
    uint16_t RGBA32toRGBA16(uint32_t color);
    uint32_t colorToAlpha(uint32_t color);

    uint32_t getTexel(Tile &tile, int s, int t);
    bool blendPixel(bool cycle, uint32_t &color);
    bool drawPixel(int x, int y);

    void runCommands();
    void triangle();
    void triDepth();
    void triTexture();
    void triDepthTex();
    void triShade();
    void triDepthSha();
    void triShadeTex();
    void triDepShaTex();
    void texRectangle();
    void syncFull();
    void setScissor();
    void setOtherModes();
    void loadTlut();
    void setTileSize();
    void loadBlock();
    void loadTile();
    void setTile();
    void fillRectangle();
    void setFillColor();
    void setFogColor();
    void setBlendColor();
    void setPrimColor();
    void setEnvColor();
    void setCombine();
    void setTexImage();
    void setZImage();
    void setColorImage();
    void noOp();
    void unknown();
}

// RDP command lookup table, based on opcode bits 56-61
void (*RDP::commands[0x40])() =
{
    noOp,         unknown,       unknown,       unknown,       // 0x00-0x03
    unknown,      unknown,       unknown,       unknown,       // 0x04-0x07
    triangle,     triDepth,      triTexture,    triDepthTex,   // 0x08-0x0B
    triShade,     triDepthSha,   triShadeTex,   triDepShaTex,  // 0x0C-0x0F
    unknown,      unknown,       unknown,       unknown,       // 0x10-0x13
    unknown,      unknown,       unknown,       unknown,       // 0x14-0x17
    unknown,      unknown,       unknown,       unknown,       // 0x18-0x1B
    unknown,      unknown,       unknown,       unknown,       // 0x1C-0x1F
    unknown,      unknown,       unknown,       unknown,       // 0x20-0x23
    texRectangle, unknown,       unknown,       unknown,       // 0x24-0x27
    unknown,      syncFull,      unknown,       unknown,       // 0x28-0x2B
    unknown,      setScissor,    unknown,       setOtherModes, // 0x2C-0x2F
    loadTlut,     unknown,       setTileSize,   loadBlock,     // 0x30-0x33
    loadTile,     setTile,       fillRectangle, setFillColor,  // 0x34-0x37
    setFogColor,  setBlendColor, setPrimColor,  setEnvColor,   // 0x38-0x3B
    setCombine,   setTexImage,   setZImage,     setColorImage  // 0x3C-0x3F
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
    blendA[0] = blendA[1] = 0;
    blendB[0] = blendB[1] = 0;
    blendC[0] = blendC[1] = 0;
    blendD[0] = blendD[1] = 0;
    texAddress = 0xA0000000;
    texWidth = 0;
    texFormat = RGBA4;
    zAddress = 0xA0000000;
    colorAddress = 0xA0000000;
    colorWidth = 0;
    colorFormat = RGBA4;
    memset(tiles, 0, sizeof(tiles));
    scissorX1 = 0;
    scissorX2 = 0;
    scissorY1 = 0;
    scissorY2 = 0;
    fillColor = 0x00000000;
    combColor = 0x00000000;
    texelColor = 0x00000000;
    primColor = 0x00000000;
    shadeColor = 0x00000000;
    envColor = 0x00000000;
    combAlpha = 0x00000000;
    texelAlpha = 0x00000000;
    primAlpha = 0x00000000;
    shadeAlpha = 0x00000000;
    envAlpha = 0x00000000;
    fogColor = 0x00000000;
    blendColor = 0x00000000;
    pixelAlpha = 0x00000000;
    memColor = 0x00000000;
    maxColor = 0xFFFFFFFF;
    minColor = 0x00000000;
    for (int i = 0; i < 4; i++)
    {
        combineA[i] = &maxColor;
        combineB[i] = &minColor;
        combineC[i] = &maxColor;
        combineD[i] = &minColor;
    }
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
            // Disable setting the freeze bit as a hack for Banjo-Kazooie
            // TODO: actually implement this bit (it should block DMAs?)
            value &= ~0x8;

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

inline uint32_t RDP::RGBA16toRGBA32(uint16_t color)
{
    // Convert an RGBA16 color to RGBA32
    uint8_t r = ((color >> 8) & 0xF8) | ((color >> 13) & 0x7);
    uint8_t g = ((color >> 3) & 0xF8) | ((color >>  8) & 0x7);
    uint8_t b = ((color << 2) & 0xF8) | ((color >>  3) & 0x7);
    uint8_t a = (color & 0x1) ? 0xFF : 0x00;
    return (r << 24) | (g << 16) | (b << 8) | a;
}

inline uint16_t RDP::RGBA32toRGBA16(uint32_t color)
{
    // Convert an RGBA32 color to RGBA16
    uint8_t r = ((color >> 27) & 0x1F);
    uint8_t g = ((color >> 19) & 0x1F);
    uint8_t b = ((color >> 11) & 0x1F);
    uint8_t a = (color & 0xFF) ? 0x1 : 0x0;
    return (r << 11) | (g << 6) | (b << 1) | a;
}

inline uint32_t RDP::colorToAlpha(uint32_t color)
{
    // Mirror an RGBA32 color's alpha to all components
    uint8_t a = color & 0xFF;
    return (a << 24) | (a << 16) | (a << 8) | a;
}

uint32_t RDP::getTexel(Tile &tile, int s, int t)
{
    // Offset the texture coordinates relative to the tile
    s = (s - tile.sBase) >> 5;
    t = (t - tile.tBase) >> 5;

    // Clamp, mirror, or mask the S-coordinate based on tile settings
    if (tile.sClamp)
        s = std::max<int>(std::min<int>(s, tile.sMask), 0);
    else if (tile.sMirror)
        s = ((s & (tile.sMask + 1)) ? ~s : s) & tile.sMask;
    else
        s &= tile.sMask;

    // Clamp, mirror, or mask the T-coordinate based on tile settings
    if (tile.tClamp)
        t = std::max<int>(std::min<int>(t, tile.tMask), 0);
    else if (tile.tMirror)
        t = ((t & (tile.tMask + 1)) ? ~t : t) & tile.tMask;
    else
        t &= tile.tMask;

    // Get an RGBA32 texel from a tile at the given coordinates
    switch (tile.format)
    {
        case RGBA16:
        {
            // Convert an RGBA16 texel to RGBA32
            uint8_t *value = &tmem[(tile.address + t * tile.width + s * 2) & 0xFFE];
            return RGBA16toRGBA32((value[0] << 8) | value[1]);
        }

        case RGBA32:
        {
            // Read an RGBA32 texel, split across high and low banks
            uint8_t *valueL = &tmem[(tile.address + 0x000 + t * tile.width + s * 2) & 0xFFE];
            uint8_t *valueH = &tmem[(tile.address + 0x800 + t * tile.width + s * 2) & 0xFFE];
            return (valueH[0] << 24) | (valueH[1] << 16) | (valueL[0] << 8) | valueL[1];
        }

        case CI4:
        {
            // Convert a CI4 texel to RGBA32, using a TLUT in the high banks
            uint8_t index = (tmem[(tile.address + t * tile.width + s / 2) & 0xFFF] >> (~s & 1) * 4) & 0xF;
            uint8_t *value = &tmem[(tile.address + 0x800 + (tile.palette + index) * 2) & 0xFFE];
            return RGBA16toRGBA32((value[0] << 8) | value[1]);
        }

        case CI8:
        {
            // Convert a CI8 texel to RGBA32, using a TLUT in the high banks
            uint8_t index = tmem[(tile.address + t * tile.width + s) & 0xFFF];
            uint8_t *value = &tmem[(tile.address + 0x800 + index * 2) & 0xFFE];
            return RGBA16toRGBA32((value[0] << 8) | value[1]);
        }

        case IA4:
        {
            // Convert an IA4 texel to RGBA32
            uint8_t value = tmem[(tile.address + t * tile.width + s / 2) & 0xFFF] >> (~s & 1) * 4;
            uint8_t i = ((value << 4) & 0xE0) | ((value << 1) & 0x1C) | ((value >> 2) & 0x3);
            uint8_t a = (value & 0x1) ? 0xFF : 0x0;
            return (i << 24) | (i << 16) | (i << 8) | a;
        }

        case IA8:
        {
            // Convert an IA8 texel to RGBA32
            uint8_t value = tmem[(tile.address + t * tile.width + s) & 0xFFF];
            uint8_t i = (value & 0xF0) | (value >> 4);
            uint8_t a = (value & 0x0F) | (value << 4);
            return (i << 24) | (i << 16) | (i << 8) | a;
        }

        case IA16:
        {
            // Convert an IA16 texel to RGBA32
            uint8_t *value = &tmem[(tile.address + t * tile.width + s * 2) & 0xFFE];
            uint8_t i = value[0];
            uint8_t a = value[1];
            return (i << 24) | (i << 16) | (i << 8) | a;
        }

        case I4:
        {
            // Convert an I4 texel to RGBA32
            uint8_t value = tmem[(tile.address + t * tile.width + s / 2) & 0xFFF] >> (~s & 1) * 4;
            uint8_t i = (value << 4) | (value & 0xF);
            return (i << 24) | (i << 16) | (i << 8) | i;
        }

        case I8:
        {
            // Convert an I8 texel to RGBA32
            uint8_t i = tmem[(tile.address + t * tile.width + s) & 0xFFF];
            return (i << 24) | (i << 16) | (i << 8) | i;
        }

        default:
            LOG_WARN("Unknown RDP texture format: %d\n", tile.format);
            return maxColor;
    }
}

bool RDP::blendPixel(bool cycle, uint32_t &color)
{
    // Select the first color for blending
    uint32_t color1;
    switch (blendA[cycle])
    {
        case 0: color1 = combColor;  break;
        case 1: color1 = memColor;   break;
        case 2: color1 = blendColor; break;
        case 3: color1 = fogColor;   break;
    }

    // Select the scale for the first color
    uint8_t scale1;
    switch (blendB[cycle])
    {
        case 0: scale1 = pixelAlpha; break;
        case 1: scale1 = fogColor;   break;
        case 2: scale1 = shadeAlpha; break;
        case 3: scale1 = 0x00;       break;
    }

    // Select the second color for blending
    uint32_t color2;
    switch (blendC[cycle])
    {
        case 0: color2 = combColor;  break;
        case 1: color2 = memColor;   break;
        case 2: color2 = blendColor; break;
        case 3: color2 = fogColor;   break;
    }

    // Select the scale for the second color
    uint8_t scale2;
    switch (blendD[cycle])
    {
        case 0: scale2 = ~scale1;  break;
        case 1: scale2 = memColor; break;
        case 2: scale2 = 0xFF;     break;
        case 3: scale2 = 0x00;     break;
    }

    // Blend the colors to form a new color
    if (uint16_t scale = scale1 + scale2)
    {
        uint8_t r = (((color1 >> 24) & 0xFF) * scale1 + ((color2 >> 24) & 0xFF) * scale2) / scale;
        uint8_t g = (((color1 >> 16) & 0xFF) * scale1 + ((color2 >> 16) & 0xFF) * scale2) / scale;
        uint8_t b = (((color1 >>  8) & 0xFF) * scale1 + ((color2 >>  8) & 0xFF) * scale2) / scale;
        color = (r << 24) | (g << 16) | (b << 8);
        return true;
    }

    return false;
}

bool RDP::drawPixel(int x, int y)
{
    switch (cycleType)
    {
        case ONE_CYCLE:
        {
            // Combine cycle 0 RGBA channels using the formula (A - B) * C + D
            uint8_t r = (((((*combineA[0] >> 24) - (*combineB[0] >> 24)) & 0xFF) * ((*combineC[0] >> 24) & 0xFF)) / 0xFF) + (*combineD[0] >> 24);
            uint8_t g = (((((*combineA[0] >> 16) - (*combineB[0] >> 16)) & 0xFF) * ((*combineC[0] >> 16) & 0xFF)) / 0xFF) + (*combineD[0] >> 16);
            uint8_t b = (((((*combineA[0] >>  8) - (*combineB[0] >>  8)) & 0xFF) * ((*combineC[0] >>  8) & 0xFF)) / 0xFF) + (*combineD[0] >>  8);
            uint8_t a = (((((*combineA[2] >>  0) - (*combineB[2] >>  0)) & 0xFF) * ((*combineC[2] >>  0) & 0xFF)) / 0xFF) + (*combineD[2] >>  0);
            combColor = (r << 24) | (g << 16) | (b << 8) | a;
            pixelAlpha = combAlpha = colorToAlpha(combColor);

            if (colorFormat == RGBA16)
            {
                // Blend the pixel with the previous RGBA16 pixel in the color buffer
                memColor = RGBA16toRGBA32(Memory::read<uint16_t>(colorAddress + (y * colorWidth + x) * 2)) & ~0xFF;
                if (blendPixel(false, memColor))
                {
                    Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2, RGBA32toRGBA16(memColor | 0xFF));
                    return true;
                }
            }
            else
            {
                // Blend the pixel with the previous RGBA32 pixel in the color buffer
                memColor = Memory::read<uint32_t>(colorAddress + (y * colorWidth + x) * 4) & ~0xFF;
                if (blendPixel(false, memColor))
                {
                    Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4, memColor | 0xFF);
                    return true;
                }
            }
            return false;
        }

        case TWO_CYCLE:
        {
            // Combine cycle 0 RGBA channels using the formula (A - B) * C + D
            uint8_t r = (((((*combineA[0] >> 24) - (*combineB[0] >> 24)) & 0xFF) * ((*combineC[0] >> 24) & 0xFF)) / 0xFF) + (*combineD[0] >> 24);
            uint8_t g = (((((*combineA[0] >> 16) - (*combineB[0] >> 16)) & 0xFF) * ((*combineC[0] >> 16) & 0xFF)) / 0xFF) + (*combineD[0] >> 16);
            uint8_t b = (((((*combineA[0] >>  8) - (*combineB[0] >>  8)) & 0xFF) * ((*combineC[0] >>  8) & 0xFF)) / 0xFF) + (*combineD[0] >>  8);
            uint8_t a = (((((*combineA[2] >>  0) - (*combineB[2] >>  0)) & 0xFF) * ((*combineC[2] >>  0) & 0xFF)) / 0xFF) + (*combineD[2] >>  0);
            combColor = (r << 24) | (g << 16) | (b << 8) | a;
            pixelAlpha = combAlpha = colorToAlpha(combColor);

            // Blend the pixel with the previous pixel in the color buffer
            if (colorFormat == RGBA16)
                memColor = RGBA16toRGBA32(Memory::read<uint16_t>(colorAddress + (y * colorWidth + x) * 2)) & ~0xFF;
            else
                memColor = Memory::read<uint32_t>(colorAddress + (y * colorWidth + x) * 4) & ~0xFF;
            bool blend = blendPixel(false, combColor);

            // Combine cycle 1 RGB channels using the formula (A - B) * C + D
            r = (((((*combineA[1] >> 24) - (*combineB[1] >> 24)) & 0xFF) * ((*combineC[1] >> 24) & 0xFF)) / 0xFF) + (*combineD[1] >> 24);
            g = (((((*combineA[1] >> 16) - (*combineB[1] >> 16)) & 0xFF) * ((*combineC[1] >> 16) & 0xFF)) / 0xFF) + (*combineD[1] >> 16);
            b = (((((*combineA[1] >>  8) - (*combineB[1] >>  8)) & 0xFF) * ((*combineC[1] >>  8) & 0xFF)) / 0xFF) + (*combineD[1] >>  8);
            a = (((((*combineA[3] >>  0) - (*combineB[3] >>  0)) & 0xFF) * ((*combineC[3] >>  0) & 0xFF)) / 0xFF) + (*combineD[3] >>  0);
            combColor = (r << 24) | (g << 16) | (b << 8) | a;
            combAlpha = colorToAlpha(combColor);

            // Blend the pixel again and write it to the color buffer
            uint32_t color = combColor;
            if (blendPixel(true, color) || blend)
            {
                if (colorFormat == RGBA16)
                    Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2, RGBA32toRGBA16(color | 0xFF));
                else
                    Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4, color | 0xFF);
                return true;
            }
            return false;
        }

        case COPY_MODE:
            // Copy a texel directly to the color buffer
            if (texelColor & 0xFF)
            {
                if (colorFormat == RGBA16)
                    Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2, RGBA32toRGBA16(texelColor));
                else
                    Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4, texelColor);
                return true;
            }
            return false;

        case FILL_MODE:
            // Copy the fill color directly to the color buffer
            if (colorFormat == RGBA16)
                Memory::write<uint16_t>(colorAddress + (y * colorWidth + x) * 2, fillColor >> ((~x & 1) * 16));
            else
                Memory::write<uint32_t>(colorAddress + (y * colorWidth + x) * 4, fillColor);
            return true;
    }

    return false;
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
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Draw pixels if they're within scissor bounds
        for (int x = xa; x != xb + inc; x += inc)
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2)
                drawPixel(x, y);
    }
}

void RDP::triDepth()
{
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Get the base triangle depth and gradients
    int32_t z1 = (opcode[4] >> 32);
    int32_t dzdx = (opcode[4] >> 0);
    int32_t dzde = (opcode[5] >> 32);

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Get the interpolated values at the start of the line
        int32_t za = (z1 += dzde);

        // Draw a line of the triangle
        for (int x = xa; x != xb + inc; x += inc)
        {
            // Get the current pixel's depth value
            uint16_t z = za >> 16;

            // Draw a pixel if within scissor bounds and the depth test passes
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2 &&
                Memory::read<uint16_t>(zAddress + (y * colorWidth + x) * 2) > z)
            {
                // Update the Z buffer if a pixel is drawn
                if (drawPixel(x, y))
                    Memory::write<uint16_t>(zAddress + (y * colorWidth + x) * 2, z);
            }

            // Interpolate the values across the line
            za += dzdx * inc;
        }
    }
}

void RDP::triTexture()
{
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Get the base triangle texture coordinates and gradients
    Tile &tile = tiles[(opcode[0] >> 48) & 0x7];
    int32_t s1 = (((opcode[4] >> 48) & 0xFFFF) << 16) | ((opcode[6] >> 48) & 0xFFFF);
    int32_t t1 = (((opcode[4] >> 32) & 0xFFFF) << 16) | ((opcode[6] >> 32) & 0xFFFF);
    int32_t w1 = (((opcode[4] >> 16) & 0xFFFF) << 16) | ((opcode[6] >> 16) & 0xFFFF);
    int32_t dsdx = (((opcode[5] >> 48) & 0xFFFF) << 16) | ((opcode[7] >> 48) & 0xFFFF);
    int32_t dtdx = (((opcode[5] >> 32) & 0xFFFF) << 16) | ((opcode[7] >> 32) & 0xFFFF);
    int32_t dwdx = (((opcode[5] >> 16) & 0xFFFF) << 16) | ((opcode[7] >> 16) & 0xFFFF);
    int32_t dsde = (((opcode[8] >> 48) & 0xFFFF) << 16) | ((opcode[10] >> 48) & 0xFFFF);
    int32_t dtde = (((opcode[8] >> 32) & 0xFFFF) << 16) | ((opcode[10] >> 32) & 0xFFFF);
    int32_t dwde = (((opcode[8] >> 16) & 0xFFFF) << 16) | ((opcode[10] >> 16) & 0xFFFF);

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Get the interpolated values at the start of the line
        int32_t sa = (s1 += dsde);
        int32_t ta = (t1 += dtde);
        int32_t wa = (w1 += dwde);

        // Draw a line of the triangle
        for (int x = xa; x != xb + inc; x += inc)
        {
            // Draw a pixel if it's within scissor bounds
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2)
            {
                // Update the texel color for the current pixel, with perspective correction
                if (wa >> 15)
                {
                    texelColor = getTexel(tile, sa / (wa >> 15), ta / (wa >> 15));
                    texelAlpha = colorToAlpha(texelColor);
                }

                drawPixel(x, y);
            }

            // Interpolate the values across the line
            sa += dsdx * inc;
            ta += dtdx * inc;
            wa += dwdx * inc;
        }
    }
}

void RDP::triDepthTex()
{
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Get the base triangle texture coordinates and gradients
    Tile &tile = tiles[(opcode[0] >> 48) & 0x7];
    int32_t s1 = (((opcode[4] >> 48) & 0xFFFF) << 16) | ((opcode[6] >> 48) & 0xFFFF);
    int32_t t1 = (((opcode[4] >> 32) & 0xFFFF) << 16) | ((opcode[6] >> 32) & 0xFFFF);
    int32_t w1 = (((opcode[4] >> 16) & 0xFFFF) << 16) | ((opcode[6] >> 16) & 0xFFFF);
    int32_t dsdx = (((opcode[5] >> 48) & 0xFFFF) << 16) | ((opcode[7] >> 48) & 0xFFFF);
    int32_t dtdx = (((opcode[5] >> 32) & 0xFFFF) << 16) | ((opcode[7] >> 32) & 0xFFFF);
    int32_t dwdx = (((opcode[5] >> 16) & 0xFFFF) << 16) | ((opcode[7] >> 16) & 0xFFFF);
    int32_t dsde = (((opcode[8] >> 48) & 0xFFFF) << 16) | ((opcode[10] >> 48) & 0xFFFF);
    int32_t dtde = (((opcode[8] >> 32) & 0xFFFF) << 16) | ((opcode[10] >> 32) & 0xFFFF);
    int32_t dwde = (((opcode[8] >> 16) & 0xFFFF) << 16) | ((opcode[10] >> 16) & 0xFFFF);

    // Get the base triangle depth and gradients
    int32_t z1 = (opcode[12] >> 32);
    int32_t dzdx = (opcode[12] >> 0);
    int32_t dzde = (opcode[13] >> 32);

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Get the interpolated values at the start of the line
        int32_t sa = (s1 += dsde);
        int32_t ta = (t1 += dtde);
        int32_t wa = (w1 += dwde);
        int32_t za = (z1 += dzde);

        // Draw a line of the triangle
        for (int x = xa; x != xb + inc; x += inc)
        {
            // Get the current pixel's depth value
            uint16_t z = za >> 16;

            // Draw a pixel if within scissor bounds and the depth test passes
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2 &&
                Memory::read<uint16_t>(zAddress + (y * colorWidth + x) * 2) > z)
            {
                // Update the texel color for the current pixel, with perspective correction
                if (wa >> 15)
                {
                    texelColor = getTexel(tile, sa / (wa >> 15), ta / (wa >> 15));
                    texelAlpha = colorToAlpha(texelColor);
                }

                // Update the Z buffer if a pixel is drawn
                if (drawPixel(x, y))
                    Memory::write<uint16_t>(zAddress + (y * colorWidth + x) * 2, z);
            }

            // Interpolate the values across the line
            sa += dsdx * inc;
            ta += dtdx * inc;
            wa += dwdx * inc;
            za += dzdx * inc;
        }
    }
}

void RDP::triShade()
{
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Get the base triangle color components and gradients
    int32_t r1 = (((opcode[4] >> 48) & 0xFFFF) << 16) | ((opcode[6] >> 48) & 0xFFFF);
    int32_t g1 = (((opcode[4] >> 32) & 0xFFFF) << 16) | ((opcode[6] >> 32) & 0xFFFF);
    int32_t b1 = (((opcode[4] >> 16) & 0xFFFF) << 16) | ((opcode[6] >> 16) & 0xFFFF);
    int32_t a1 = (((opcode[4] >>  0) & 0xFFFF) << 16) | ((opcode[6] >>  0) & 0xFFFF);
    int32_t drdx = (((opcode[5] >> 48) & 0xFFFF) << 16) | ((opcode[7] >> 48) & 0xFFFF);
    int32_t dgdx = (((opcode[5] >> 32) & 0xFFFF) << 16) | ((opcode[7] >> 32) & 0xFFFF);
    int32_t dbdx = (((opcode[5] >> 16) & 0xFFFF) << 16) | ((opcode[7] >> 16) & 0xFFFF);
    int32_t dadx = (((opcode[5] >>  0) & 0xFFFF) << 16) | ((opcode[7] >>  0) & 0xFFFF);
    int32_t drde = (((opcode[8] >> 48) & 0xFFFF) << 16) | ((opcode[10] >> 48) & 0xFFFF);
    int32_t dgde = (((opcode[8] >> 32) & 0xFFFF) << 16) | ((opcode[10] >> 32) & 0xFFFF);
    int32_t dbde = (((opcode[8] >> 16) & 0xFFFF) << 16) | ((opcode[10] >> 16) & 0xFFFF);
    int32_t dade = (((opcode[8] >>  0) & 0xFFFF) << 16) | ((opcode[10] >>  0) & 0xFFFF);

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Get the interpolated values at the start of the line
        int32_t ra = (r1 += drde);
        int32_t ga = (g1 += dgde);
        int32_t ba = (b1 += dbde);
        int32_t aa = (a1 += dade);

        // Draw a line of the triangle
        for (int x = xa; x != xb + inc; x += inc)
        {
            // Draw a pixel if it's within scissor bounds
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2)
            {
                // Update the shade color for the current pixel
                uint8_t r = ((ra >> 16) & 0xFF);
                uint8_t g = ((ga >> 16) & 0xFF);
                uint8_t b = ((ba >> 16) & 0xFF);
                uint8_t a = ((aa >> 16) & 0xFF);
                shadeColor = (r << 24) | (g << 16) | (b << 8) | a;
                shadeAlpha = colorToAlpha(shadeColor);

                drawPixel(x, y);
            }

            // Interpolate the values across the line
            ra += drdx * inc;
            ga += dgdx * inc;
            ba += dbdx * inc;
            aa += dadx * inc;
        }
    }
}

void RDP::triDepthSha()
{
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Get the base triangle color components and gradients
    int32_t r1 = (((opcode[4] >> 48) & 0xFFFF) << 16) | ((opcode[6] >> 48) & 0xFFFF);
    int32_t g1 = (((opcode[4] >> 32) & 0xFFFF) << 16) | ((opcode[6] >> 32) & 0xFFFF);
    int32_t b1 = (((opcode[4] >> 16) & 0xFFFF) << 16) | ((opcode[6] >> 16) & 0xFFFF);
    int32_t a1 = (((opcode[4] >>  0) & 0xFFFF) << 16) | ((opcode[6] >>  0) & 0xFFFF);
    int32_t drdx = (((opcode[5] >> 48) & 0xFFFF) << 16) | ((opcode[7] >> 48) & 0xFFFF);
    int32_t dgdx = (((opcode[5] >> 32) & 0xFFFF) << 16) | ((opcode[7] >> 32) & 0xFFFF);
    int32_t dbdx = (((opcode[5] >> 16) & 0xFFFF) << 16) | ((opcode[7] >> 16) & 0xFFFF);
    int32_t dadx = (((opcode[5] >>  0) & 0xFFFF) << 16) | ((opcode[7] >>  0) & 0xFFFF);
    int32_t drde = (((opcode[8] >> 48) & 0xFFFF) << 16) | ((opcode[10] >> 48) & 0xFFFF);
    int32_t dgde = (((opcode[8] >> 32) & 0xFFFF) << 16) | ((opcode[10] >> 32) & 0xFFFF);
    int32_t dbde = (((opcode[8] >> 16) & 0xFFFF) << 16) | ((opcode[10] >> 16) & 0xFFFF);
    int32_t dade = (((opcode[8] >>  0) & 0xFFFF) << 16) | ((opcode[10] >>  0) & 0xFFFF);

    // Get the base triangle depth and gradients
    int32_t z1 = (opcode[12] >> 32);
    int32_t dzdx = (opcode[12] >> 0);
    int32_t dzde = (opcode[13] >> 32);

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Get the interpolated values at the start of the line
        int32_t ra = (r1 += drde);
        int32_t ga = (g1 += dgde);
        int32_t ba = (b1 += dbde);
        int32_t aa = (a1 += dade);
        int32_t za = (z1 += dzde);

        // Draw a line of the triangle
        for (int x = xa; x != xb + inc; x += inc)
        {
            // Get the current pixel's Z value
            uint16_t z = za >> 16;

            // Draw a pixel if within scissor bounds and the depth test passes
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2 &&
                Memory::read<uint16_t>(zAddress + (y * colorWidth + x) * 2) > z)
            {
                // Update the shade color for the current pixel
                uint8_t r = ((ra >> 16) & 0xFF);
                uint8_t g = ((ga >> 16) & 0xFF);
                uint8_t b = ((ba >> 16) & 0xFF);
                uint8_t a = ((aa >> 16) & 0xFF);
                shadeColor = (r << 24) | (g << 16) | (b << 8) | a;
                shadeAlpha = colorToAlpha(shadeColor);

                // Update the Z buffer if a pixel is drawn
                if (drawPixel(x, y))
                    Memory::write<uint16_t>(zAddress + (y * colorWidth + x) * 2, z);
            }

            // Interpolate the values across the line
            ra += drdx * inc;
            ga += dgdx * inc;
            ba += dbdx * inc;
            aa += dadx * inc;
            za += dzdx * inc;
        }
    }
}

void RDP::triShadeTex()
{
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Get the base triangle color components and gradients
    int32_t r1 = (((opcode[4] >> 48) & 0xFFFF) << 16) | ((opcode[6] >> 48) & 0xFFFF);
    int32_t g1 = (((opcode[4] >> 32) & 0xFFFF) << 16) | ((opcode[6] >> 32) & 0xFFFF);
    int32_t b1 = (((opcode[4] >> 16) & 0xFFFF) << 16) | ((opcode[6] >> 16) & 0xFFFF);
    int32_t a1 = (((opcode[4] >>  0) & 0xFFFF) << 16) | ((opcode[6] >>  0) & 0xFFFF);
    int32_t drdx = (((opcode[5] >> 48) & 0xFFFF) << 16) | ((opcode[7] >> 48) & 0xFFFF);
    int32_t dgdx = (((opcode[5] >> 32) & 0xFFFF) << 16) | ((opcode[7] >> 32) & 0xFFFF);
    int32_t dbdx = (((opcode[5] >> 16) & 0xFFFF) << 16) | ((opcode[7] >> 16) & 0xFFFF);
    int32_t dadx = (((opcode[5] >>  0) & 0xFFFF) << 16) | ((opcode[7] >>  0) & 0xFFFF);
    int32_t drde = (((opcode[8] >> 48) & 0xFFFF) << 16) | ((opcode[10] >> 48) & 0xFFFF);
    int32_t dgde = (((opcode[8] >> 32) & 0xFFFF) << 16) | ((opcode[10] >> 32) & 0xFFFF);
    int32_t dbde = (((opcode[8] >> 16) & 0xFFFF) << 16) | ((opcode[10] >> 16) & 0xFFFF);
    int32_t dade = (((opcode[8] >>  0) & 0xFFFF) << 16) | ((opcode[10] >>  0) & 0xFFFF);

    // Get the base triangle texture coordinates and gradients
    Tile &tile = tiles[(opcode[0] >> 48) & 0x7];
    int32_t s1 = (((opcode[12] >> 48) & 0xFFFF) << 16) | ((opcode[14] >> 48) & 0xFFFF);
    int32_t t1 = (((opcode[12] >> 32) & 0xFFFF) << 16) | ((opcode[14] >> 32) & 0xFFFF);
    int32_t w1 = (((opcode[12] >> 16) & 0xFFFF) << 16) | ((opcode[14] >> 16) & 0xFFFF);
    int32_t dsdx = (((opcode[13] >> 48) & 0xFFFF) << 16) | ((opcode[15] >> 48) & 0xFFFF);
    int32_t dtdx = (((opcode[13] >> 32) & 0xFFFF) << 16) | ((opcode[15] >> 32) & 0xFFFF);
    int32_t dwdx = (((opcode[13] >> 16) & 0xFFFF) << 16) | ((opcode[15] >> 16) & 0xFFFF);
    int32_t dsde = (((opcode[16] >> 48) & 0xFFFF) << 16) | ((opcode[18] >> 48) & 0xFFFF);
    int32_t dtde = (((opcode[16] >> 32) & 0xFFFF) << 16) | ((opcode[18] >> 32) & 0xFFFF);
    int32_t dwde = (((opcode[16] >> 16) & 0xFFFF) << 16) | ((opcode[18] >> 16) & 0xFFFF);

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Get the interpolated values at the start of the line
        int32_t ra = (r1 += drde);
        int32_t ga = (g1 += dgde);
        int32_t ba = (b1 += dbde);
        int32_t aa = (a1 += dade);
        int32_t sa = (s1 += dsde);
        int32_t ta = (t1 += dtde);
        int32_t wa = (w1 += dwde);

        // Draw a line of the triangle
        for (int x = xa; x != xb + inc; x += inc)
        {
            // Draw a pixel if it's within scissor bounds
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2)
            {
                // Update the shade color for the current pixel
                uint8_t r = ((ra >> 16) & 0xFF);
                uint8_t g = ((ga >> 16) & 0xFF);
                uint8_t b = ((ba >> 16) & 0xFF);
                uint8_t a = ((aa >> 16) & 0xFF);
                shadeColor = (r << 24) | (g << 16) | (b << 8) | a;
                shadeAlpha = colorToAlpha(shadeColor);

                // Update the texel color for the current pixel, with perspective correction
                if (wa >> 15)
                {
                    texelColor = getTexel(tile, sa / (wa >> 15), ta / (wa >> 15));
                    texelAlpha = colorToAlpha(texelColor);
                }

                drawPixel(x, y);
            }

            // Interpolate the values across the line
            ra += drdx * inc;
            ga += dgdx * inc;
            ba += dbdx * inc;
            aa += dadx * inc;
            sa += dsdx * inc;
            ta += dtdx * inc;
            wa += dwdx * inc;
        }
    }
}

void RDP::triDepShaTex()
{
    // Decode the base triangle parameters
    int32_t y1 = (int16_t)(opcode[0] <<  2) >> 4; // High Y-coord
    int32_t y2 = (int16_t)(opcode[0] >> 14) >> 4; // Middle Y-coord
    int32_t y3 = (int16_t)(opcode[0] >> 30) >> 4; // Low Y-coord
    int32_t slope1 = opcode[1]; // Low edge slope
    int32_t slope2 = opcode[2]; // High edge slope
    int32_t slope3 = opcode[3]; // Middle edge slope
    int32_t x1 = (opcode[1] >> 32) - slope1; // Low edge X-coord
    int32_t x2 = (opcode[2] >> 32); // High edge X-coord
    int32_t x3 = (opcode[3] >> 32); // Middle edge X-coord

    // Get the base triangle color components and gradients
    int32_t r1 = (((opcode[4] >> 48) & 0xFFFF) << 16) | ((opcode[6] >> 48) & 0xFFFF);
    int32_t g1 = (((opcode[4] >> 32) & 0xFFFF) << 16) | ((opcode[6] >> 32) & 0xFFFF);
    int32_t b1 = (((opcode[4] >> 16) & 0xFFFF) << 16) | ((opcode[6] >> 16) & 0xFFFF);
    int32_t a1 = (((opcode[4] >>  0) & 0xFFFF) << 16) | ((opcode[6] >>  0) & 0xFFFF);
    int32_t drdx = (((opcode[5] >> 48) & 0xFFFF) << 16) | ((opcode[7] >> 48) & 0xFFFF);
    int32_t dgdx = (((opcode[5] >> 32) & 0xFFFF) << 16) | ((opcode[7] >> 32) & 0xFFFF);
    int32_t dbdx = (((opcode[5] >> 16) & 0xFFFF) << 16) | ((opcode[7] >> 16) & 0xFFFF);
    int32_t dadx = (((opcode[5] >>  0) & 0xFFFF) << 16) | ((opcode[7] >>  0) & 0xFFFF);
    int32_t drde = (((opcode[8] >> 48) & 0xFFFF) << 16) | ((opcode[10] >> 48) & 0xFFFF);
    int32_t dgde = (((opcode[8] >> 32) & 0xFFFF) << 16) | ((opcode[10] >> 32) & 0xFFFF);
    int32_t dbde = (((opcode[8] >> 16) & 0xFFFF) << 16) | ((opcode[10] >> 16) & 0xFFFF);
    int32_t dade = (((opcode[8] >>  0) & 0xFFFF) << 16) | ((opcode[10] >>  0) & 0xFFFF);

    // Get the base triangle texture coordinates and gradients
    Tile &tile = tiles[(opcode[0] >> 48) & 0x7];
    int32_t s1 = (((opcode[12] >> 48) & 0xFFFF) << 16) | ((opcode[14] >> 48) & 0xFFFF);
    int32_t t1 = (((opcode[12] >> 32) & 0xFFFF) << 16) | ((opcode[14] >> 32) & 0xFFFF);
    int32_t w1 = (((opcode[12] >> 16) & 0xFFFF) << 16) | ((opcode[14] >> 16) & 0xFFFF);
    int32_t dsdx = (((opcode[13] >> 48) & 0xFFFF) << 16) | ((opcode[15] >> 48) & 0xFFFF);
    int32_t dtdx = (((opcode[13] >> 32) & 0xFFFF) << 16) | ((opcode[15] >> 32) & 0xFFFF);
    int32_t dwdx = (((opcode[13] >> 16) & 0xFFFF) << 16) | ((opcode[15] >> 16) & 0xFFFF);
    int32_t dsde = (((opcode[16] >> 48) & 0xFFFF) << 16) | ((opcode[18] >> 48) & 0xFFFF);
    int32_t dtde = (((opcode[16] >> 32) & 0xFFFF) << 16) | ((opcode[18] >> 32) & 0xFFFF);
    int32_t dwde = (((opcode[16] >> 16) & 0xFFFF) << 16) | ((opcode[18] >> 16) & 0xFFFF);

    // Get the base triangle depth and gradients
    int32_t z1 = (opcode[20] >> 32);
    int32_t dzdx = (opcode[20] >> 0);
    int32_t dzde = (opcode[21] >> 32);

    // Draw a triangle from top to bottom
    for (int y = y1; y < y3; y++)
    {
        // Get the X-bounds of the triangle on the current line
        // From Y1 to Y2, the high and middle edges are used
        // From Y2 to Y3, the high and low edges are used
        int xa = (x2 += slope2) >> 16;
        int xb = ((y < y2) ? (x3 += slope3) : (x1 += slope1)) >> 16;
        int inc = (xa < xb) ? 1 : -1;

        // Get the interpolated values at the start of the line
        int32_t ra = (r1 += drde);
        int32_t ga = (g1 += dgde);
        int32_t ba = (b1 += dbde);
        int32_t aa = (a1 += dade);
        int32_t sa = (s1 += dsde);
        int32_t ta = (t1 += dtde);
        int32_t wa = (w1 += dwde);
        int32_t za = (z1 += dzde);

        // Draw a line of the triangle
        for (int x = xa; x != xb + inc; x += inc)
        {
            // Get the current pixel's Z value
            uint16_t z = za >> 16;

            // Draw a pixel if within scissor bounds and the depth test passes
            if (x >= scissorX1 && x < scissorX2 && y >= scissorY1 && y < scissorY2 &&
                Memory::read<uint16_t>(zAddress + (y * colorWidth + x) * 2) > z)
            {
                // Update the shade color for the current pixel
                uint8_t r = ((ra >> 16) & 0xFF);
                uint8_t g = ((ga >> 16) & 0xFF);
                uint8_t b = ((ba >> 16) & 0xFF);
                uint8_t a = ((aa >> 16) & 0xFF);
                shadeColor = (r << 24) | (g << 16) | (b << 8) | a;
                shadeAlpha = colorToAlpha(shadeColor);

                // Update the texel color for the current pixel, with perspective correction
                if (wa >> 15)
                {
                    texelColor = getTexel(tile, sa / (wa >> 15), ta / (wa >> 15));
                    texelAlpha = colorToAlpha(texelColor);
                }

                // Update the Z buffer if a pixel is drawn
                if (drawPixel(x, y))
                    Memory::write<uint16_t>(zAddress + (y * colorWidth + x) * 2, z);
            }

            // Interpolate the values across the line
            ra += drdx * inc;
            ga += dgdx * inc;
            ba += dbdx * inc;
            aa += dadx * inc;
            sa += dsdx * inc;
            ta += dtdx * inc;
            wa += dwdx * inc;
            za += dzdx * inc;
        }
    }
}

void RDP::texRectangle()
{
    // Decode the operands
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    uint16_t y1 = ((opcode[0] >>  0) & 0xFFF) >> 2;
    uint16_t x1 = ((opcode[0] >> 12) & 0xFFF) >> 2;
    uint16_t y2 = ((opcode[0] >> 32) & 0xFFF) >> 2;
    uint16_t x2 = ((opcode[0] >> 44) & 0xFFF) >> 2;
    int16_t dtdy = opcode[1] >>  0;
    int16_t dsdx = opcode[1] >> 16;
    int t1 = (int16_t)(opcode[1] >> 32) << 5;
    int s1 = (int16_t)(opcode[1] >> 48) << 5;

    // Adjust some things based on the cycle type
    // TODO: handle this more accurately
    if (cycleType >= COPY_MODE)
    {
        dsdx >>= 2;
        x2++;
        y2++;
    }

    // Draw a rectangle using a texture
    for (int y = y1, t = t1; y < y2; y++, t += dtdy)
    {
        for (int x = x1, s = s1; x < x2; x++, s += dsdx)
        {
            // Scissoring applies only to triangle primitives
            texelColor = getTexel(tile, s >> 5, t >> 5);
            texelAlpha = colorToAlpha(texelColor);
            drawPixel(x, y);
        }
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
    // Set the cycle type and blend inputs
    // TODO: actually use the other bits
    cycleType = (CycleType)((opcode[0] >> 52) & 0x3);
    blendA[0] = (opcode[0] >> 30) & 0x3;
    blendA[1] = (opcode[0] >> 28) & 0x3;
    blendB[0] = (opcode[0] >> 26) & 0x3;
    blendB[1] = (opcode[0] >> 24) & 0x3;
    blendC[0] = (opcode[0] >> 22) & 0x3;
    blendC[1] = (opcode[0] >> 20) & 0x3;
    blendD[0] = (opcode[0] >> 18) & 0x3;
    blendD[1] = (opcode[0] >> 16) & 0x3;
}

void RDP::loadTlut()
{
    // Decode the operands
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    uint16_t indexL = ((opcode[0] >> 44) & 0xFFF) >> 1;
    uint16_t indexH = ((opcode[0] >> 12) & 0xFFF) >> 1;

    // Copy 16-bit texture lookup values into TMEM
    for (int i = indexL; i < indexH; i += 2)
    {
        uint16_t src = Memory::read<uint16_t>(texAddress + i);
        uint8_t *dst = &tmem[(tile.address + i) & 0xFFE];
        dst[0] = src >> 8;
        dst[1] = src >> 0;
    }
}

void RDP::setTileSize()
{
    // Set the low texture coordinates for reference
    // TODO: use the high coordinates for something?
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    tile.sBase = ((opcode[0] >> 44) & 0xFFF) << 3;
    tile.tBase = ((opcode[0] >> 32) & 0xFFF) << 3;
}

void RDP::loadBlock()
{
    // Decode the operands
    // TODO: actually use the other bits
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    uint16_t count = ((opcode[0] >> 12) & 0xFFF);

    // Adjust the byte count based on the texel size
    count = (count << 2) >> (~texFormat & 0x3);

    if ((texFormat & 0x3) == 0x3)
    {
        // Copy texture data from the texture buffer to TMEM, 8 bytes at a time, split across high and low banks
        for (int i = 0; i <= count; i += 8)
        {
            uint64_t src = Memory::read<uint64_t>(texAddress + i);
            uint8_t *dstL = &tmem[(tile.address + 0x000 + i / 2) & 0xFFC];
            uint8_t *dstH = &tmem[(tile.address + 0x800 + i / 2) & 0xFFC];

            for (int j = 0; j < 4; j += 2)
            {
                dstH[j + 0] = src >> (56 - j * 16);
                dstH[j + 1] = src >> (48 - j * 16);
                dstL[j + 0] = src >> (40 - j * 16);
                dstL[j + 1] = src >> (32 - j * 16);
            }
        }
    }
    else
    {
        // Copy texture data from the texture buffer to TMEM, 8 bytes at a time
        for (int i = 0; i <= count; i += 8)
        {
            uint64_t src = Memory::read<uint64_t>(texAddress + i);
            uint8_t *dst = &tmem[(tile.address + i) & 0xFF8];

            for (int j = 0; j < 8; j++)
                dst[j] = src >> (7 - j) * 8;
        }
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

    // Save the low texture coordinates for reference
    tile.sBase = ((opcode[0] >> 44) & 0xFFF) << 3;
    tile.tBase = ((opcode[0] >> 32) & 0xFFF) << 3;

    // Only support loading textures without conversion for now
    if (texFormat != tile.format)
    {
        LOG_WARN("Unimplemented RDP texture conversion: %d to %d\n", texFormat, tile.format);
        return;
    }

    switch (texFormat & 0x3)
    {
        case 0x0: // 4-bit
            // Cut out a 4-bit texture from the texture buffer and copy it to TMEM
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s += 2)
                    tmem[(tile.address + (t - t1) * tile.width + (s - s1) / 2) & 0xFFF] =
                        Memory::read<uint8_t>(texAddress + (t * texWidth + s) / 2);
            return;

        case 0x1: // 8-bit
            // Cut out an 8-bit texture from the texture buffer and copy it to TMEM
            for (int t = t1; t <= t2; t++)
                for (int s = s1; s <= s2; s++)
                    tmem[(tile.address + (t - t1) * tile.width + (s - s1)) & 0xFFF] =
                        Memory::read<uint8_t>(texAddress + t * texWidth + s);
            return;

        case 0x2: // 16-bit
            // Cut out a 16-bit texture from the texture buffer and copy it to TMEM
            for (int t = t1; t <= t2; t++)
            {
                for (int s = s1; s <= s2; s++)
                {
                    uint16_t src = Memory::read<uint16_t>(texAddress + (t * texWidth + s) * 2);
                    uint8_t *dst = &tmem[(tile.address + (t - t1) * tile.width + (s - s1) * 2) & 0xFFE];
                    dst[0] = src >> 8;
                    dst[1] = src >> 0;
                }
            }
            return;

        case 0x3: // 32-bit
            // Cut out a 32-bit texture from the texture buffer and copy it to TMEM, split across high and low banks
            for (int t = t1; t <= t2; t++)
            {
                for (int s = s1; s <= s2; s++)
                {
                    uint32_t src = Memory::read<uint32_t>(texAddress + (t * texWidth + s) * 4);
                    uint8_t *dstL = &tmem[(tile.address + 0x000 + (t - t1) * tile.width + (s - s1) * 2) & 0xFFE];
                    uint8_t *dstH = &tmem[(tile.address + 0x800 + (t - t1) * tile.width + (s - s1) * 2) & 0xFFE];
                    dstH[0] = src >> 24;
                    dstH[1] = src >> 16;
                    dstL[0] = src >>  8;
                    dstL[1] = src >>  0;
                }
            }
            return;
    }
}

void RDP::setTile()
{
    // Set parameters for the specified tile
    // TODO: Actually use the detail shifts
    Tile &tile = tiles[(opcode[0] >> 24) & 0x7];
    tile.sMask = (((opcode[0] >> 4) & 0xF) ? (1 << ((opcode[0] >> 4) & 0xF)) : 0) - 1;
    tile.sMirror = ((opcode[0] >> 8) & 0x1);
    tile.sClamp = ((opcode[0] >> 9) & 0x1);
    tile.tMask = (((opcode[0] >> 14) & 0xF) ? (1 << ((opcode[0] >> 14) & 0xF)) : 0) - 1;
    tile.tMirror = ((opcode[0] >> 18) & 0x1);
    tile.tClamp = ((opcode[0] >> 19) & 0x1);
    tile.palette = ((opcode[0] >> 20) & 0xF) << 4;
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
    // TODO: handle this more accurately
    if (cycleType >= COPY_MODE)
    {
        x2++;
        y2++;
    }

    // Scissoring applies only to triangle primitives

    // Draw a rectangle
    for (int y = y1; y < y2; y++)
        for (int x = x1; x < x2; x++)
            drawPixel(x, y);
}

void RDP::setFillColor()
{
    // Set the fill color
    fillColor = opcode[0];
}

void RDP::setFogColor()
{
    // Set the fog color
    fogColor = opcode[0];
}

void RDP::setBlendColor()
{
    // Set the blend color
    blendColor = opcode[0];
}

void RDP::setPrimColor()
{
    // Set the primitive color
    // TODO: actually use LOD bits
    primColor = opcode[0];
    primAlpha = colorToAlpha(primColor);
}

void RDP::setEnvColor()
{
    // Set the environment color
    envColor = opcode[0];
    envAlpha = colorToAlpha(envColor);
}

void RDP::setCombine()
{
    for (int i = 0; i < 2; i++)
    {
        // Set the A input for color combiner RGB components
        static const uint8_t shiftsA[2] = { 52, 37 };
        switch (uint8_t srcA = (opcode[0] >> shiftsA[i]) & 0xF)
        {
            case 0: combineA[i] = &combColor;  break;
            case 1: combineA[i] = &texelColor; break;
            case 3: combineA[i] = &primColor;  break;
            case 4: combineA[i] = &shadeColor; break;
            case 5: combineA[i] = &envColor;   break;
            case 6: combineA[i] = &maxColor;   break;

            default:
                if (srcA >= 8)
                {
                    combineA[i] = &minColor;
                    break;
                }

                LOG_WARN("Unimplemented CC cycle %d RGB source A: %d\n", i, srcA);
                combineA[i] = &maxColor;
                break;
        }

        // Set the B input for color combiner RGB components
        static const uint8_t shiftsB[2] = { 28, 24 };
        switch (uint8_t srcB = (opcode[0] >> shiftsB[i]) & 0xF)
        {
            case 0: combineB[i] = &combColor;  break;
            case 1: combineB[i] = &texelColor; break;
            case 3: combineB[i] = &primColor;  break;
            case 4: combineB[i] = &shadeColor; break;
            case 5: combineB[i] = &envColor;   break;

            default:
                if (srcB >= 8)
                {
                    combineB[i] = &minColor;
                    break;
                }

                LOG_WARN("Unimplemented CC cycle %d RGB source B: %d\n", i, srcB);
                combineB[i] = &minColor;
                break;
        }

        // Set the C input for color combiner RGB components
        static const uint8_t shiftsC[2] = { 47, 32 };
        switch (uint8_t srcC = (opcode[0] >> shiftsC[i]) & 0x1F)
        {
            case  0: combineC[i] = &combColor;  break;
            case  1: combineC[i] = &texelColor; break;
            case  3: combineC[i] = &primColor;  break;
            case  4: combineC[i] = &shadeColor; break;
            case  5: combineC[i] = &envColor;   break;
            case  7: combineC[i] = &combAlpha;  break;
            case  8: combineC[i] = &texelAlpha; break;
            case 10: combineC[i] = &primAlpha;  break;
            case 11: combineC[i] = &shadeAlpha; break;
            case 12: combineC[i] = &envAlpha;   break;

            default:
                if (srcC >= 16)
                {
                    combineC[i] = &minColor;
                    break;
                }

                LOG_WARN("Unimplemented CC cycle %d RGB source C: %d\n", i, srcC);
                combineC[i] = &maxColor;
                break;
        }

        // Set the D input for color combiner RGB components
        static const uint8_t shiftsD[2] = { 15, 6 };
        switch (uint8_t srcD = (opcode[0] >> shiftsD[i]) & 0x7)
        {
            case 0: combineD[i] = &combColor;  break;
            case 1: combineD[i] = &texelColor; break;
            case 3: combineD[i] = &primColor;  break;
            case 4: combineD[i] = &shadeColor; break;
            case 5: combineD[i] = &envColor;   break;
            case 6: combineD[i] = &maxColor;   break;
            case 7: combineD[i] = &minColor;   break;

            default:
                LOG_WARN("Unimplemented CC cycle %d RGB source D: %d\n", i, srcD);
                combineD[i] = &minColor;
                break;
        }
    }

    for (int i = 2; i < 4; i++)
    {
        // Set the A input for color combiner alpha components
        static const uint8_t shiftsA[2] = { 44, 21 };
        switch (uint8_t srcA = (opcode[0] >> shiftsA[i - 2]) & 0x7)
        {
            case 0: combineA[i] = &combAlpha;  break;
            case 1: combineA[i] = &texelAlpha; break;
            case 3: combineA[i] = &primAlpha;  break;
            case 4: combineA[i] = &shadeAlpha; break;
            case 5: combineA[i] = &envAlpha;   break;
            case 6: combineA[i] = &maxColor;   break;
            case 7: combineA[i] = &minColor;   break;

            default:
                LOG_WARN("Unimplemented CC cycle %d alpha source A: %d\n", i, srcA);
                combineA[i] = &maxColor;
                break;
        }

        // Set the B input for color combiner alpha components
        static const uint8_t shiftsB[2] = { 12, 3 };
        switch (uint8_t srcB = (opcode[0] >> shiftsB[i - 2]) & 0x7)
        {
            case 0: combineB[i] = &combAlpha;  break;
            case 1: combineB[i] = &texelAlpha; break;
            case 3: combineB[i] = &primAlpha;  break;
            case 4: combineB[i] = &shadeAlpha; break;
            case 5: combineB[i] = &envAlpha;   break;
            case 6: combineB[i] = &maxColor;   break;
            case 7: combineB[i] = &minColor;   break;

            default:
                LOG_WARN("Unimplemented CC cycle %d alpha source B: %d\n", i, srcB);
                combineB[i] = &minColor;
                break;
        }

        // Set the C input for color combiner alpha components
        static const uint8_t shiftsC[2] = { 41, 18 };
        switch (uint8_t srcC = (opcode[0] >> shiftsC[i - 2]) & 0x7)
        {
            case 1: combineC[i] = &texelAlpha; break;
            case 3: combineC[i] = &primAlpha;  break;
            case 4: combineC[i] = &shadeAlpha; break;
            case 5: combineC[i] = &envAlpha;   break;
            case 7: combineC[i] = &minColor;   break;

            default:
                LOG_WARN("Unimplemented CC cycle %d alpha source C: %d\n", i, srcC);
                combineC[i] = &maxColor;
                break;
        }

        // Set the D input for color combiner alpha components
        static const uint8_t shiftsD[2] = { 9, 0 };
        switch (uint8_t srcD = (opcode[0] >> shiftsD[i - 2]) & 0x7)
        {
            case 0: combineD[i] = &combAlpha;  break;
            case 1: combineD[i] = &texelAlpha; break;
            case 3: combineD[i] = &primAlpha;  break;
            case 4: combineD[i] = &shadeAlpha; break;
            case 5: combineD[i] = &envAlpha;   break;
            case 6: combineD[i] = &maxColor;   break;
            case 7: combineD[i] = &minColor;   break;

            default:
                LOG_WARN("Unimplemented CC cycle %d alpha source D: %d\n", i, srcD);
                combineD[i] = &minColor;
                break;
        }
    }
}

void RDP::setTexImage()
{
    // Set the texture buffer parameters
    texAddress = 0xA0000000 + (opcode[0] & 0x3FFFFF);
    texWidth = ((opcode[0] >> 32) & 0x3FF) + 1;
    texFormat = (Format)((opcode[0] >> 51) & 0x1F);
}

void RDP::setZImage()
{
    // Set the Z buffer parameters
    zAddress = 0xA0000000 + (opcode[0] & 0x3FFFFF);
}

void RDP::setColorImage()
{
    // Set the color buffer parameters
    colorAddress = 0xA0000000 + (opcode[0] & 0x3FFFFF);
    colorWidth = ((opcode[0] >> 32) & 0x3FF) + 1;
    colorFormat = (Format)((opcode[0] >> 51) & 0x1F);

    if (colorFormat != RGBA16 && colorFormat != RGBA32)
    {
        LOG_CRIT("Unknown RDP color buffer format: %d\n", colorFormat);
        colorFormat = RGBA16;
    }
}

void RDP::noOp()
{
    // Do nothing
}

void RDP::unknown()
{
    // Warn about unknown commands
    LOG_CRIT("Unknown RDP opcode: 0x%016lX\n", opcode[0]);
}
