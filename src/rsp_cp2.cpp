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
    extern const uint16_t rcpTable[0x200];

    uint16_t registers[32][8];
    int64_t accumulator[8];
    uint32_t divIn;
    uint16_t divOut;
    uint16_t vco;
    uint16_t vcc;
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

    void vlt(uint32_t opcode);
    void veq(uint32_t opcode);
    void vne(uint32_t opcode);
    void vge(uint32_t opcode);
    void vcl(uint32_t opcode);
    void vch(uint32_t opcode);
    void vmrg(uint32_t opcode);

    void vand(uint32_t opcode);
    void vnand(uint32_t opcode);
    void vor(uint32_t opcode);
    void vnor(uint32_t opcode);
    void vxor(uint32_t opcode);
    void vnxor(uint32_t opcode);

    void vrcp(uint32_t opcode);
    void vrcpl(uint32_t opcode);
    void vrcph(uint32_t opcode);
    void vmov(uint32_t opcode);

    void unk(uint32_t opcode);
}

// RSP vector unit instruction lookup table, using opcode bits 0-5
void (*RSP_CP2::vecInstrs[0x40])(uint32_t) =
{
    vmulf, vmulu, unk,   unk,  vmudl, vmudm, vmudn, vmudh, // 0x00-0x07
    vmacf, vmacu, unk,   unk,  vmadl, vmadm, vmadn, vmadh, // 0x08-0x0F
    vadd,  vsub,  unk,   unk,  vaddc, vsubc, unk,   unk,   // 0x10-0x17
    unk,   unk,   unk,   unk,  unk,   vsar,  unk,   unk,   // 0x18-0x1F
    vlt,   veq,   vne,   vge,  vcl,   vch,   unk,   vmrg,  // 0x20-0x27
    vand,  vnand, vor,   vnor, vxor,  vnxor, unk,   unk,   // 0x28-0x2F
    vrcp,  vrcpl, vrcph, vmov, unk,   unk,   unk,   unk,   // 0x30-0x37
    unk,   unk,   unk,   unk,  unk,   unk,   unk,   unk    // 0x38-0x3F
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

// Lookup table for calculating a reciprocal
const uint16_t RSP_CP2::rcpTable[0x200] =
{
    0xFFFF, 0xFF00, 0xFE01, 0xFD04, 0xFC07, 0xFB0C, 0xFA11, 0xF918,
    0xF81F, 0xF727, 0xF631, 0xF53B, 0xF446, 0xF352, 0xF25F, 0xF16D,
    0xF07C, 0xEF8B, 0xEE9C, 0xEDAE, 0xECC0, 0xEBD3, 0xEAE8, 0xE9FD,
    0xE913, 0xE829, 0xE741, 0xE65A, 0xE573, 0xE48D, 0xE3A9, 0xE2C5,
    0xE1E1, 0xE0FF, 0xE01E, 0xDF3D, 0xDE5D, 0xDD7E, 0xDCA0, 0xDBC2,
    0xDAE6, 0xDA0A, 0xD92F, 0xD854, 0xD77B, 0xD6A2, 0xD5CA, 0xD4F3,
    0xD41D, 0xD347, 0xD272, 0xD19E, 0xD0CB, 0xCFF8, 0xCF26, 0xCE55,
    0xCD85, 0xCCB5, 0xCBE6, 0xCB18, 0xCA4B, 0xC97E, 0xC8B2, 0xC7E7,
    0xC71C, 0xC652, 0xC589, 0xC4C0, 0xC3F8, 0xC331, 0xC26B, 0xC1A5,
    0xC0E0, 0xC01C, 0xBF58, 0xBE95, 0xBDD2, 0xBD10, 0xBC4F, 0xBB8F,
    0xBACF, 0xBA10, 0xB951, 0xB894, 0xB7D6, 0xB71A, 0xB65E, 0xB5A2,
    0xB4E8, 0xB42E, 0xB374, 0xB2BB, 0xB203, 0xB14B, 0xB094, 0xAFDE,
    0xAF28, 0xAE73, 0xADBE, 0xAD0A, 0xAC57, 0xABA4, 0xAAF1, 0xAA40,
    0xA98E, 0xA8DE, 0xA82E, 0xA77E, 0xA6D0, 0xA621, 0xA574, 0xA4C6,
    0xA41A, 0xA36E, 0xA2C2, 0xA217, 0xA16D, 0xA0C3, 0xA01A, 0x9F71,
    0x9EC8, 0x9E21, 0x9D79, 0x9CD3, 0x9C2D, 0x9B87, 0x9AE2, 0x9A3D,
    0x9999, 0x98F6, 0x9852, 0x97B0, 0x970E, 0x966C, 0x95CB, 0x952B,
    0x948B, 0x93EB, 0x934C, 0x92AD, 0x920F, 0x9172, 0x90D4, 0x9038,
    0x8F9C, 0x8F00, 0x8E65, 0x8DCA, 0x8D30, 0x8C96, 0x8BFC, 0x8B64,
    0x8ACB, 0x8A33, 0x899C, 0x8904, 0x886E, 0x87D8, 0x8742, 0x86AD,
    0x8618, 0x8583, 0x84F0, 0x845C, 0x83C9, 0x8336, 0x82A4, 0x8212,
    0x8181, 0x80F0, 0x8060, 0x7FD0, 0x7F40, 0x7EB1, 0x7E22, 0x7D93,
    0x7D05, 0x7C78, 0x7BEB, 0x7B5E, 0x7AD2, 0x7A46, 0x79BA, 0x792F,
    0x78A4, 0x781A, 0x7790, 0x7706, 0x767D, 0x75F5, 0x756C, 0x74E4,
    0x745D, 0x73D5, 0x734F, 0x72C8, 0x7242, 0x71BC, 0x7137, 0x70B2,
    0x702E, 0x6FA9, 0x6F26, 0x6EA2, 0x6E1F, 0x6D9C, 0x6D1A, 0x6C98,
    0x6C16, 0x6B95, 0x6B14, 0x6A94, 0x6A13, 0x6993, 0x6914, 0x6895,
    0x6816, 0x6798, 0x6719, 0x669C, 0x661E, 0x65A1, 0x6524, 0x64A8,
    0x642C, 0x63B0, 0x6335, 0x62BA, 0x623F, 0x61C5, 0x614B, 0x60D1,
    0x6058, 0x5FDF, 0x5F66, 0x5EED, 0x5E75, 0x5DFD, 0x5D86, 0x5D0F,
    0x5C98, 0x5C22, 0x5BAB, 0x5B35, 0x5AC0, 0x5A4B, 0x59D6, 0x5961,
    0x58ED, 0x5879, 0x5805, 0x5791, 0x571E, 0x56AC, 0x5639, 0x55C7,
    0x5555, 0x54E3, 0x5472, 0x5401, 0x5390, 0x5320, 0x52AF, 0x5240,
    0x51D0, 0x5161, 0x50F2, 0x5083, 0x5015, 0x4FA6, 0x4F38, 0x4ECB,
    0x4E5E, 0x4DF1, 0x4D84, 0x4D17, 0x4CAB, 0x4C3F, 0x4BD3, 0x4B68,
    0x4AFD, 0x4A92, 0x4A27, 0x49BD, 0x4953, 0x48E9, 0x4880, 0x4817,
    0x47AE, 0x4745, 0x46DC, 0x4674, 0x460C, 0x45A5, 0x453D, 0x44D6,
    0x446F, 0x4408, 0x43A2, 0x433C, 0x42D6, 0x4270, 0x420B, 0x41A6,
    0x4141, 0x40DC, 0x4078, 0x4014, 0x3FB0, 0x3F4C, 0x3EE8, 0x3E85,
    0x3E22, 0x3DC0, 0x3D5D, 0x3CFB, 0x3C99, 0x3C37, 0x3BD6, 0x3B74,
    0x3B13, 0x3AB2, 0x3A52, 0x39F1, 0x3991, 0x3931, 0x38D2, 0x3872,
    0x3813, 0x37B4, 0x3755, 0x36F7, 0x3698, 0x363A, 0x35DC, 0x357F,
    0x3521, 0x34C4, 0x3467, 0x340A, 0x33AE, 0x3351, 0x32F5, 0x3299,
    0x323E, 0x31E2, 0x3187, 0x312C, 0x30D1, 0x3076, 0x301C, 0x2FC2,
    0x2F68, 0x2F0E, 0x2EB4, 0x2E5B, 0x2E02, 0x2DA9, 0x2D50, 0x2CF8,
    0x2C9F, 0x2C47, 0x2BEF, 0x2B97, 0x2B40, 0x2AE8, 0x2A91, 0x2A3A,
    0x29E4, 0x298D, 0x2937, 0x28E0, 0x288B, 0x2835, 0x27DF, 0x278A,
    0x2735, 0x26E0, 0x268B, 0x2636, 0x25E2, 0x258D, 0x2539, 0x24E5,
    0x2492, 0x243E, 0x23EB, 0x2398, 0x2345, 0x22F2, 0x22A0, 0x224D,
    0x21FB, 0x21A9, 0x2157, 0x2105, 0x20B4, 0x2063, 0x2012, 0x1FC1,
    0x1F70, 0x1F1F, 0x1ECF, 0x1E7F, 0x1E2E, 0x1DDF, 0x1D8F, 0x1D3F,
    0x1CF0, 0x1CA1, 0x1C52, 0x1C03, 0x1BB4, 0x1B66, 0x1B17, 0x1AC9,
    0x1A7B, 0x1A2D, 0x19E0, 0x1992, 0x1945, 0x18F8, 0x18AB, 0x185E,
    0x1811, 0x17C4, 0x1778, 0x172C, 0x16E0, 0x1694, 0x1648, 0x15FD,
    0x15B1, 0x1566, 0x151B, 0x14D0, 0x1485, 0x143B, 0x13F0, 0x13A6,
    0x135C, 0x1312, 0x12C8, 0x127F, 0x1235, 0x11EC, 0x11A3, 0x1159,
    0x1111, 0x10C8, 0x107F, 0x1037, 0x0FEF, 0x0FA6, 0x0F5E, 0x0F17,
    0x0ECF, 0x0E87, 0x0E40, 0x0DF9, 0x0DB2, 0x0D6B, 0x0D24, 0x0CDD,
    0x0C97, 0x0C50, 0x0C0A, 0x0BC4, 0x0B7E, 0x0B38, 0x0AF2, 0x0AAD,
    0x0A68, 0x0A22, 0x09DD, 0x0998, 0x0953, 0x090F, 0x08CA, 0x0886,
    0x0842, 0x07FD, 0x07B9, 0x0776, 0x0732, 0x06EE, 0x06AB, 0x0668,
    0x0624, 0x05E1, 0x059E, 0x055C, 0x0519, 0x04D6, 0x0494, 0x0452,
    0x0410, 0x03CE, 0x038C, 0x034A, 0x0309, 0x02C7, 0x0286, 0x0245,
    0x0204, 0x01C3, 0x0182, 0x0141, 0x0101, 0x00C0, 0x0080, 0x0040
};

void RSP_CP2::reset()
{
    // Reset the RSP CP2 to its initial state
    memset(registers, 0, sizeof(registers));
    memset(accumulator, 0, sizeof(accumulator));
    divIn = 0;
    divOut = 0;
    vco = 0;
    vcc = 0;
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
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
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
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
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
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Add two vector registers with modifier for the second, and set the overflow bits
    vco = 0;
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = vs[i] + vt[e[i]];
        vco |= ((accumulator[i] >> 16) & 1) << i;
        vd[i] = (accumulator[i] &= 0xFFFF);
    }
}

void RSP_CP2::vsubc(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Subtract two vector registers with modifier for the second, and set the overflow bits
    vco = 0;
    for (int i = 0; i < 8; i++)
    {
        accumulator[i] = vs[i] - vt[e[i]];
        vco |= (((accumulator[i] & 0x1FFF) ? 0x100 : 0) | ((accumulator[i] >> 16) & 1)) << i;
        vd[i] = (accumulator[i] &= 0xFFFF);
    }
}

void RSP_CP2::vsar(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    int shift = ((2 - (opcode >> 21)) & 0x3) * 16;

    // Load a vector register with 16-bit portions of the accumulator
    for (int i = 0; i < 8; i++)
        vd[i] = accumulator[i] >> shift;
}

void RSP_CP2::vlt(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Perform a less than comparison on two vector registers, and set the compare bits
    vcc = 0;
    for (int i = 0; i < 8; i++)
    {
        bool res = (vs[i] < vt[e[i]] || (vs[i] == vt[e[i]] && (((vco & (vco >> 8)) >> i) & 1)));
        accumulator[i] = vd[i] = res ? vs[i] : vt[e[i]];
        vcc |= res << i;
    }
    vco = 0;
}

void RSP_CP2::veq(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Perform an equal comparison on two vector registers, and set the compare bits
    vcc = 0;
    for (int i = 0; i < 8; i++)
    {
        bool res = (vs[i] == vt[e[i]] && !(vco & (0x100 << i)));
        accumulator[i] = vd[i] = res ? vs[i] : vt[e[i]];
        vcc |= res << i;
    }
    vco = 0;
}

void RSP_CP2::vne(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Perform a not equal comparison on two vector registers, and set the compare bits
    vcc = 0;
    for (int i = 0; i < 8; i++)
    {
        bool res = (vs[i] != vt[e[i]] || (vco & (0x100 << i)));
        accumulator[i] = vd[i] = res ? vs[i] : vt[e[i]];
        vcc |= res << i;
    }
    vco = 0;
}

void RSP_CP2::vge(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Perform a greater or equal comparison on two vector registers, and set the compare bits
    vcc = 0;
    for (int i = 0; i < 8; i++)
    {
        bool res = (vs[i] > vt[e[i]] || (vs[i] == vt[e[i]] && !(((vco & (vco >> 8)) >> i) & 1)));
        accumulator[i] = vd[i] = res ? vs[i] : vt[e[i]];
        vcc |= res << i;
    }
    vco = 0;
}

void RSP_CP2::vcl(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Clip a vector register, treating it as the lower half of 32-bit values following VCH
    for (int i = 0; i < 8; i++)
    {
        if (!((vco >> i) & 0x101))
            vcc = (vcc & ~(0x100 << i)) | ((vs[i] >= vt[e[i]]) << (i + 8));
        else if (((vco >> i) & 0x101) == 0x1)
            vcc = (vcc & ~(1 << i)) | ((((vce >> i) & 1) ? (vs[i] <= -vt[e[i]]) : (vs[i] == -vt[e[i]])) << i);
        bool diff = (vco >> i) & 1;
        int16_t abs = (diff ? -vt[e[i]] : vt[e[i]]);
        accumulator[i] = vd[i] = ((vcc >> (!diff * 8 + i)) & 1) ? abs : vs[i];
    }
}

void RSP_CP2::vch(uint32_t opcode)
{
    // Decode the operands
    int16_t *vt = (int16_t*)registers[(opcode >> 16) & 0x1F];
    int16_t *vs = (int16_t*)registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >> 6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Clip a vector register with respect to another register, and set the status bits
    vco = vcc = vce = 0;
    for (int i = 0; i < 8; i++)
    {
        bool diff = (vs[i] ^ vt[e[i]]) & 0x8000;
        bool comp = (diff && vs[i] == -vt[e[i]] - 1);
        int16_t abs = (diff ? -vt[e[i]] : vt[e[i]]);
        vco |= (((!comp && vs[i] != abs) << 8) | diff) << i;
        vcc |= (((vs[i] >= vt[e[i]]) << 8) | (vs[i] <= -vt[e[i]])) << i;
        vce |= comp << i;
        accumulator[i] = vd[i] = ((vcc >> (!diff * 8 + i)) & 1) ? abs : vs[i];
    }
}

void RSP_CP2::vmrg(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t *vs = registers[(opcode >> 11) & 0x1F];
    uint16_t *vd = registers[(opcode >>  6) & 0x1F];
    const uint8_t *e = elements[(opcode >> 21) & 0xF];

    // Merge two vector registers using the compare bits to choose lanes
    for (int i = 0; i < 8; i++)
        accumulator[i] = vd[i] = (vcc & (1 << i)) ? vs[i] : vt[e[i]];
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

void RSP_CP2::vrcp(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    int32_t vte = (int16_t)vt[(opcode >> 21) & 0x7];
    uint16_t &vd = registers[(opcode >>  6) & 0x1F][(opcode >> 11) & 0x7];

    // Lookup the 32-bit reciprocal of a signed 16-bit value
    for (int i = 0; i < 8; i++)
        accumulator[i] = vt[i];
    uint32_t value = abs(vte);
    int shift; for (shift = 0; value >> shift; shift++);
    uint32_t result = rcpTable[((value << (32 - shift)) >> 22) & 0x1FF];
    result = (((0x10000 | result) << 15) >> shift) ^ (vte >> 31);
    divOut = result >> 16;
    vd = result;
}

void RSP_CP2::vrcpl(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    int32_t vte = (divIn & 0x10000) ? ((divIn << 16) | vt[(opcode >> 21) & 0x7]) : (int16_t)vt[(opcode >> 21) & 0x7];
    uint16_t &vd = registers[(opcode >>  6) & 0x1F][(opcode >> 11) & 0x7];
    divIn = 0;

    // Lookup the 32-bit reciprocal of a signed 32-bit value, or 16 if upper isn't set
    for (int i = 0; i < 8; i++)
        accumulator[i] = vt[i];
    uint32_t value = abs(vte);
    int shift; for (shift = 0; value >> shift; shift++);
    uint32_t result = rcpTable[((value << (32 - shift)) >> 22) & 0x1FF];
    result = (((0x10000 | result) << 15) >> shift) ^ (vte >> 31);
    divOut = result >> 16;
    vd = result;
}

void RSP_CP2::vrcph(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t &vd = registers[(opcode >>  6) & 0x1F][(opcode >> 11) & 0x7];

    // Set the upper half of the reciprocal input and get the lower half of the output
    for (int i = 0; i < 8; i++)
        accumulator[i] = vt[i];
    divIn = 0x10000 | vt[(opcode >> 21) & 0x7];
    vd = divOut;
}

void RSP_CP2::vmov(uint32_t opcode)
{
    // Decode the operands
    uint16_t *vt = registers[(opcode >> 16) & 0x1F];
    uint16_t &vd = registers[(opcode >>  6) & 0x1F][(opcode >> 11) & 0x7];

    // Copy a single lane from one vector register to another
    for (int i = 0; i < 8; i++)
        accumulator[i] = vt[i];
    vd = vt[(opcode >> 21) & 0x7];
}

void RSP_CP2::unk(uint32_t opcode)
{
    // Warn about unknown instructions
    LOG_CRIT("Unknown RSP VU opcode: 0x%08X @ 0x%X\n", opcode, 0x1000 | ((RSP::readPC() - 4) & 0xFFF));
}
