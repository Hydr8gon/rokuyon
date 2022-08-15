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

#include "rsp_cp2.h"
#include "log.h"
#include "rsp.h"

namespace RSP_CP2
{
    extern const uint8_t elements[16][8];

    uint16_t registers[32][8];
    int64_t accumulator[8];
    uint16_t vcc;
    uint16_t vco;
    uint16_t vce;

    uint16_t clampSigned(int64_t value);
    uint16_t clampUnsigned(int64_t value);

    void vmulf(uint32_t opcode);
    void vmulu(uint32_t opcode);
    void vmudl(uint32_t opcode);
    void vmudm(uint32_t opcode);
    void vmudn(uint32_t opcode);
    void vmudh(uint32_t opcode);
    void vmacf(uint32_t opcode);
    void vmacu(uint32_t opcode);
    void vmadl(uint32_t opcode);
    void vmadm(uint32_t opcode);
    void vmadn(uint32_t opcode);
    void vmadh(uint32_t opcode);

    void vadd(uint32_t opcode);
    void vsub(uint32_t opcode);
    void vaddc(uint32_t opcode);
    void vsubc(uint32_t opcode);
    void vsar(uint32_t opcode);
    void vand(uint32_t opcode);
    void vnand(uint32_t opcode);
    void vor(uint32_t opcode);
    void vnor(uint32_t opcode);
    void vxor(uint32_t opcode);
    void vnxor(uint32_t opcode);

    void unk(uint32_t opcode);
}

// RSP vector unit instruction lookup table, using opcode bits 0-5
void (*RSP_CP2::vecInstrs[0x40])(uint32_t) =
{
    vmulf, vmulu, unk, unk,  vmudl, vmudm, vmudn, vmudh, // 0x00-0x07
    vmacf, vmacu, unk, unk,  vmadl, vmadm, vmadn, vmadh, // 0x08-0x0F
    vadd,  vsub,  unk, unk,  vaddc, vsubc, unk,   unk,   // 0x10-0x17
    unk,   unk,   unk, unk,  unk,   vsar,  unk,   unk,   // 0x18-0x1F
    unk,   unk,   unk, unk,  unk,   unk,   unk,   unk,   // 0x20-0x27
    vand,  vnand, vor, vnor, vxor,  vnxor, unk,   unk,   // 0x28-0x2F
    unk,   unk,   unk, unk,  unk,   unk,   unk,   unk,   // 0x30-0x37
    unk,   unk,   unk, unk,  unk,   unk,   unk,   unk    // 0x38-0x3F
};

// Lane modifiers for each element
const uint8_t RSP_CP2::elements[16][8] =
{
    { 0, 1, 2, 3, 4, 5, 6, 7 },
    { 0, 1, 2, 3, 4, 5, 6, 7 },
    { 0, 0, 2, 2, 4, 4, 6, 6 },
    { 1, 1, 3, 3, 5, 5, 7, 7 },
    { 0, 0, 0, 0, 4, 4, 4, 4 },
    { 1, 1, 1, 1, 5, 5, 5, 5 },
    { 2, 2, 2, 2, 6, 6, 6, 6 },
    { 3, 3, 3, 3, 7, 7, 7, 7 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 1, 1, 1, 1, 1, 1, 1, 1 },
    { 2, 2, 2, 2, 2, 2, 2, 2 },
    { 3, 3, 3, 3, 3, 3, 3, 3 },
    { 4, 4, 4, 4, 4, 4, 4, 4 },
    { 5, 5, 5, 5, 5, 5, 5, 5 },
    { 6, 6, 6, 6, 6, 6, 6, 6 },
    { 7, 7, 7, 7, 7, 7, 7, 7 }
};

void RSP_CP2::reset()
{
    // Reset the RSP CP2 to its initial state
    memset(registers, 0, sizeof(registers));
    memset(accumulator, 0, sizeof(accumulator));
    vcc = 0;
    vco = 0;
    vce = 0;
}

int16_t RSP_CP2::read(bool control, int index, int byte)
{
    if (!control)
    {
        // Wrap reads that start out of bounds
        byte &= 0xF;

        if (byte == 15)
        {
            // Wrap around to the first byte if the starting byte is 15
            return (registers[index][7] << 8) | (registers[index][0] >> 8);
        }
        else if (!(byte & 1))
        {
            // Read from a single register lane if the starting byte is even
            return registers[index][byte / 2];
        }
        else
        {
            // Read bytes from 2 register lanes if the starting byte is odd
            return (registers[index][byte / 2] << 8) | (registers[index][byte / 2 + 1] >> 8);
        }
    }
    else
    {
        // Read from an RSP CP2 control register if one exists at the given index
        switch (index)
        {
            case 0:
                // Get the VCO register
                return vco;

            case 1:
                // Get the VCC register
                return vcc;

            case 2:
                // Get the VCE register
                return vce;

            default:
                LOG_WARN("Read from unknown RSP CP2 control register: %d\n", index);
                return 0;
        }
    }
}

void RSP_CP2::write(bool control, int index, int byte, int16_t value)
{
    if (!control)
    {
        if (byte > 15)
        {
            // Ignore writes that start out of bounds
            return;
        }
        else if (byte == 15)
        {
            // Ignore the second byte if the starting byte is 15
            registers[index][7] = (registers[index][7] & ~0xFF) | ((value >> 8) & 0xFF);
        }
        else if (!(byte & 1))
        {
            // Write to a single register lane if the starting byte is even
            registers[index][byte / 2] = value;
        }
        else
        {
            // Write bytes to 2 register lanes if the starting byte is odd
            registers[index][byte / 2 + 0] = (registers[index][byte / 2 + 0] & ~0xFF) | ((value >> 8) &  0xFF);
            registers[index][byte / 2 + 1] = (registers[index][byte / 2 + 1] &  0xFF) | ((value << 8) & ~0xFF);
        }
    }
    else
    {
        // Write to an RSP CP2 control register if one exists at the given index
        switch (index)
        {
            case 0:
                // Set the VCO register
                vco = value;
                return;

            case 1:
                // Set the VCC register
                vcc = value;
                return;

            case 2:
                // Set the VCE register
                vce = value;
                return;

            default:
                LOG_WARN("Write to unknown RSP CP2 control register: %d\n", index);
                return;
        }
    }
}

uint16_t RSP_CP2::clampSigned(int64_t value)
{
    // Clamp a value to the signed 16-bit range
    if (value < -32768) return -32768;
    if (value >  32767) return  32767;
    return value;
}

uint16_t RSP_CP2::clampUnsigned(int64_t value)
{
    // Clamp a value to the unsigned 16-bit range (bugged)
    if (value <     0) return     0;
    if (value > 32767) return 65535;
    return value;
}

void RSP_CP2::vmulf(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Multiply two signed vector registers with rounding and signed clamping
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = ((vs[i] * vt[e[i]]) << 1) + 0x8000;
        vd[i] = clampSigned(accumulator[i] >> 16);
    }
}

void RSP_CP2::vmulu(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Multiply two signed vector registers with rounding and unsigned clamping
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = ((vs[i] * vt[e[i]]) << 1) + 0x8000;
        vd[i] = clampUnsigned(accumulator[i] >> 16);
    }
}

void RSP_CP2::vmudl(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Multiply two unsigned vector registers with signed clamping
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = ((vs[i] * vt[e[i]]) >> 16) & 0xFFFF;
        vd[i] = clampSigned(accumulator[i]);
    }
}

void RSP_CP2::vmudm(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Multiply a signed and an unsigned vector register with signed clamping
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = vs[i] * vt[e[i]];
        vd[i] = clampSigned(accumulator[i] >> 16);
    }
}

void RSP_CP2::vmudn(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Multiply an unsigned and a signed vector register
    for (int i = 0; i < 8; i++)
        vd[i] = accumulator[i] = vs[i] * vt[e[i]];
}

void RSP_CP2::vmudh(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Multiply two signed vector registers with signed clamping
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = (int32_t)(vs[i] * vt[e[i]]);
        accumulator[i] <<= 16;
        vd[i] = clampSigned(accumulator[i] >> 16);
    }
}

void RSP_CP2::vmacf(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Accumulate the product of two signed vector registers with signed clamping
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] += (vs[i] * vt[e[i]]) << 1;
        vd[i] = clampSigned(accumulator[i] >> 16);
    }
}

void RSP_CP2::vmacu(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Accumulate the product of two signed vector registers with unsigned clamping
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] += (vs[i] * vt[e[i]]) << 1;
        vd[i] = clampUnsigned(accumulator[i] >> 16);
    }
}

void RSP_CP2::vmadl(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Accumulate the product of two unsigned vector registers
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] += ((vs[i] * vt[e[i]]) >> 16) & 0xFFFF;
        vd[i] = accumulator[i];
    }
}

void RSP_CP2::vmadm(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Accumulate the product of a signed and an unsigned vector register
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] += vs[i] * vt[e[i]];
        vd[i] = accumulator[i] >> 16;
    }
}

void RSP_CP2::vmadn(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Accumulate the product of an unsigned and a signed vector register
    for (int i = 0; i < 8; i++)
        vd[i] = accumulator[i] += vs[i] * vt[e[i]];
}

void RSP_CP2::vmadh(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Accumulate the product of two signed vector registers
    for (int i = 0; i < 8; i++)
    {
        int64_t value = (int32_t)(vs[i] * vt[e[i]]);
        accumulator[i] += value << 16;
        vd[i] = clampSigned(accumulator[i] >> 16);
    }
}

void RSP_CP2::vadd(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    int16_t *vd = (int16_t*)registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Add two vector registers with signed clamping and modifier for the second
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = vs[i] + vt[e[i]];
        vd[i] = clampSigned(accumulator[i]);
        accumulator[i] &= 0xFFFF;
    }
}

void RSP_CP2::vsub(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    int16_t *vd = (int16_t*)registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Subtract two vector registers with signed clamping and modifier for the second
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = vs[i] - vt[e[i]];
        vd[i] = clampSigned(accumulator[i]);
        accumulator[i] &= 0xFFFF;
    }
}

void RSP_CP2::vaddc(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    int16_t *vd = (int16_t*)registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Add two vector registers with modifier for the second
    // TODO: update VCO
    for (int i = 0; i < 8; i++)
        accumulator[i] = (vd[i] = vs[i] + vt[e[i]]) & 0xFFFF;
}

void RSP_CP2::vsubc(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    int16_t *vd = (int16_t*)registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Subtract two vector registers with modifier for the second
    // TODO: update VCO
    for (int i = 0; i < 8; i++)
        accumulator[i] = (vd[i] = vs[i] - vt[e[i]]) & 0xFFFF;
}

void RSP_CP2::vsar(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    int shift = ((2 - (opcode >> 21)) & 0x3) * 16;

    // Load a vector register with 16-bit portions of the accumulator
    for (int i = 0; i < 8; i++)
        vd[i] = accumulator[i] >> shift;
}

void RSP_CP2::vand(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Bitwise and two vector registers with modifier for the second
    for (int i = 0; i < 8; i++)
        accumulator[i] = vd[i] = vs[i] & vt[e[i]];
}

void RSP_CP2::vnand(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Negated bitwise and two vector registers with modifier for the second
    for (int i = 0; i < 8; i++)
        accumulator[i] = vd[i] = ~(vs[i] & vt[e[i]]);
}

void RSP_CP2::vor(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Bitwise or two vector registers with modifier for the second
    for (int i = 0; i < 8; i++)
        accumulator[i] = vd[i] = vs[i] | vt[e[i]];
}

void RSP_CP2::vnor(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Negated bitwise or two vector registers with modifier for the second
    for (int i = 0; i < 8; i++)
        accumulator[i] = vd[i] = ~(vs[i] | vt[e[i]]);
}

void RSP_CP2::vxor(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Bitwise exclusive or two vector registers with modifier for the second
    for (int i = 0; i < 8; i++)
        accumulator[i] = vd[i] = vs[i] ^ vt[e[i]];
}

void RSP_CP2::vnxor(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Negated bitwise exclusive or two vector registers with modifier for the second
    for (int i = 0; i < 8; i++)
        accumulator[i] = vd[i] = ~(vs[i] ^ vt[e[i]]);
}

void RSP_CP2::unk(uint32_t opcode)
{
    // Warn about unknown instructions
    LOG_CRIT("Unknown RSP VU opcode: 0x%08X @ 0x%X\n", opcode, 0x1000 | ((RSP::readPC() - 4) & 0xFFF));
}
