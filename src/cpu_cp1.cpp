/*
    Copyright 2022-2023 Hydr8gon

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

#include <cmath>
#include <cstring>
#include <fenv.h>

#include "cpu_cp1.h"
#include "cpu.h"
#include "log.h"

namespace CPU_CP1
{
    bool fullMode;
    uint64_t registers[32];
    uint32_t status;

    float &getFloat(int index);
    double &getDouble(int index);

    void addS(uint32_t opcode);
    void addD(uint32_t opcode);
    void subS(uint32_t opcode);
    void subD(uint32_t opcode);
    void mulS(uint32_t opcode);
    void mulD(uint32_t opcode);
    void divS(uint32_t opcode);
    void divD(uint32_t opcode);
    void sqrtS(uint32_t opcode);
    void sqrtD(uint32_t opcode);
    void absS(uint32_t opcode);
    void absD(uint32_t opcode);
    void movS(uint32_t opcode);
    void movD(uint32_t opcode);
    void negS(uint32_t opcode);
    void negD(uint32_t opcode);

    void roundWS(uint32_t opcode);
    void roundWD(uint32_t opcode);
    void roundLS(uint32_t opcode);
    void roundLD(uint32_t opcode);
    void truncWS(uint32_t opcode);
    void truncWD(uint32_t opcode);
    void truncLS(uint32_t opcode);
    void truncLD(uint32_t opcode);
    void ceilWS(uint32_t opcode);
    void ceilWD(uint32_t opcode);
    void ceilLS(uint32_t opcode);
    void ceilLD(uint32_t opcode);
    void floorWS(uint32_t opcode);
    void floorWD(uint32_t opcode);
    void floorLS(uint32_t opcode);
    void floorLD(uint32_t opcode);

    void cvtSD(uint32_t opcode);
    void cvtSW(uint32_t opcode);
    void cvtSL(uint32_t opcode);
    void cvtDS(uint32_t opcode);
    void cvtDW(uint32_t opcode);
    void cvtDL(uint32_t opcode);
    void cvtWS(uint32_t opcode);
    void cvtWD(uint32_t opcode);
    void cvtLS(uint32_t opcode);
    void cvtLD(uint32_t opcode);

    void cf(uint32_t opcode);
    void cunS(uint32_t opcode);
    void cunD(uint32_t opcode);
    void ceqS(uint32_t opcode);
    void ceqD(uint32_t opcode);
    void cueqS(uint32_t opcode);
    void cueqD(uint32_t opcode);
    void coltS(uint32_t opcode);
    void coltD(uint32_t opcode);
    void cultS(uint32_t opcode);
    void cultD(uint32_t opcode);
    void coleS(uint32_t opcode);
    void coleD(uint32_t opcode);
    void culeS(uint32_t opcode);
    void culeD(uint32_t opcode);
    void cngleS(uint32_t opcode);
    void cngleD(uint32_t opcode);
    void cseqS(uint32_t opcode);
    void cseqD(uint32_t opcode);
    void cnglS(uint32_t opcode);
    void cnglD(uint32_t opcode);
    void cltS(uint32_t opcode);
    void cltD(uint32_t opcode);
    void cngeS(uint32_t opcode);
    void cngeD(uint32_t opcode);
    void cleS(uint32_t opcode);
    void cleD(uint32_t opcode);
    void cngtS(uint32_t opcode);
    void cngtD(uint32_t opcode);

    void unk(uint32_t opcode);
}

// Single-precision FPU instruction lookup table, using opcode bits 0-5
void (*CPU_CP1::sglInstrs[0x40])(uint32_t) =
{
    addS,    subS,    mulS,   divS,    sqrtS,   absS,    movS,   negS,    // 0x00-0x07
    roundLS, truncLS, ceilLS, floorLS, roundWS, truncWS, ceilWS, floorWS, // 0x08-0x0F
    unk,     unk,     unk,    unk,     unk,     unk,     unk,    unk,     // 0x10-0x17
    unk,     unk,     unk,    unk,     unk,     unk,     unk,    unk,     // 0x18-0x1F
    unk,     cvtDS,   unk,    unk,     cvtWS,   cvtLS,   unk,    unk,     // 0x20-0x27
    unk,     unk,     unk,    unk,     unk,     unk,     unk,    unk,     // 0x28-0x2F
    cf,      cunS,    ceqS,   cueqS,   coltS,   cultS,   coleS,  culeS,   // 0x30-0x37
    cf,      cunS,    ceqS,   cueqS,   coltS,   cultS,   coleS,  culeS    // 0x38-0x3F
};

// Double-precision FPU instruction lookup table, using opcode bits 0-5
void (*CPU_CP1::dblInstrs[0x40])(uint32_t) =
{
    addD,    subD,    mulD,   divD,    sqrtD,   absD,    movD,   negD,    // 0x00-0x07
    roundLD, truncLD, ceilLD, floorLD, roundWD, truncWD, ceilWD, floorWD, // 0x08-0x0F
    unk,     unk,     unk,    unk,     unk,     unk,     unk,    unk,     // 0x10-0x17
    unk,     unk,     unk,    unk,     unk,     unk,     unk,    unk,     // 0x18-0x1F
    cvtSD,   unk,     unk,    unk,     cvtWD,   cvtLD,   unk,    unk,     // 0x20-0x27
    unk,     unk,     unk,    unk,     unk,     unk,     unk,    unk,     // 0x28-0x2F
    cf,      cunD,    ceqD,   cueqD,   coltD,   cultD,   coleD,  culeD,   // 0x30-0x37
    cf,      cunD,    ceqD,   cueqD,   coltD,   cultD,   coleD,  culeD    // 0x38-0x3F
};

// 32-bit integer FPU instruction lookup table, using opcode bits 0-5
void (*CPU_CP1::wrdInstrs[0x40])(uint32_t) =
{
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x00-0x07
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x08-0x0F
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x10-0x17
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x18-0x1F
    cvtSW, cvtDW, unk, unk, unk, unk, unk, unk, // 0x20-0x27
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x28-0x2F
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x30-0x37
    unk,   unk,   unk, unk, unk, unk, unk, unk  // 0x38-0x3F
};

// 64-bit integer FPU instruction lookup table, using opcode bits 0-5
void (*CPU_CP1::lwdInstrs[0x40])(uint32_t) =
{
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x00-0x07
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x08-0x0F
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x10-0x17
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x18-0x1F
    cvtSL, cvtDL, unk, unk, unk, unk, unk, unk, // 0x20-0x27
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x28-0x2F
    unk,   unk,   unk, unk, unk, unk, unk, unk, // 0x30-0x37
    unk,   unk,   unk, unk, unk, unk, unk, unk  // 0x38-0x3F
};

void CPU_CP1::reset()
{
    // Reset the CPU CP1 to its initial state
    fullMode = false;
    memset(registers, 0, sizeof(registers));
    status = 0;
}

uint64_t CPU_CP1::read(CP1Type type, int index)
{
    switch (type)
    {
        case CP1_32BIT:
            // Read a 32-bit register value, mapped based on the current mode
            // TODO: make this endian-safe
            return fullMode ? *(uint32_t*)&registers[index] :
                *((uint32_t*)&registers[index & ~1] + (index & 1));

        case CP1_64BIT:
            // Read a 64-bit register value
            return registers[index];

        default:
            // Read from a CPU CP1 control register if one exists at the given index
            switch (index)
            {
                case 31: // Status
                    // Get the status register
                    return status;

                default:
                    LOG_WARN("Read from unknown CPU CP1 control register: %d\n", index);
                    return 0;
            }
    }
}

void CPU_CP1::write(CP1Type type, int index, uint64_t value)
{
    switch (type)
    {
        case CP1_32BIT:
            // Write a 32-bit value to a register, mapped based on the current mode
            // TODO: make this endian-safe
            (fullMode ? *(uint32_t*)&registers[index] :
                *((uint32_t*)&registers[index & ~1] + (index & 1))) = value;
            return;

        case CP1_64BIT:
            // Write a 64-bit value to a register
            registers[index] = value;
            return;

        default:
            // Write to a CPU CP1 control register if one exists at the given index
            switch (index)
            {
                case 31: // Status
                    // Set the status register
                    status = value & 0x183FFFF;

                    // Keep track of unimplemented bits that should do something
                    if (uint32_t bits = (value & 0x1000F83))
                        LOG_WARN("Unimplemented CPU CP1 status bits set: 0x%X\n", bits);
                    return;

                default:
                    LOG_WARN("Write to unknown CPU CP1 control register: %d\n", index);
                    return;
            }
    }
}

void CPU_CP1::setRegMode(bool full)
{
    // Set the register mode to either full or half
    fullMode = full;
}

inline float &CPU_CP1::getFloat(int index)
{
    // Get a 32-bit register as a float, mapped based on the current mode
    // TODO: make this endian-safe
    return fullMode ? *(float*)&registers[index] :
        *((float*)&registers[index & ~1] + (index & 1));
}

inline double &CPU_CP1::getDouble(int index)
{
    // Get a 64-bit register as a double
    // This should be endian-safe as long as double and uint64_t have the same size
    return *(double*)&registers[index];
}

void CPU_CP1::addS(uint32_t opcode)
{
    // Add a float to a float and store the result
    float value = getFloat((opcode >> 11) & 0x1F) + getFloat((opcode >> 16) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::addD(uint32_t opcode)
{
    // Add a double to a double and store the result
    double value = getDouble((opcode >> 11) & 0x1F) + getDouble((opcode >> 16) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::subS(uint32_t opcode)
{
    // Subtract a float from a float and store the result
    float value = getFloat((opcode >> 11) & 0x1F) - getFloat((opcode >> 16) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::subD(uint32_t opcode)
{
    // Subtract a double from a double and store the result
    double value = getDouble((opcode >> 11) & 0x1F) - getDouble((opcode >> 16) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::mulS(uint32_t opcode)
{
    // Multiply a float by a float and store the result
    float value = getFloat((opcode >> 11) & 0x1F) * getFloat((opcode >> 16) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::mulD(uint32_t opcode)
{
    // Multiply a double by a double and store the result
    double value = getDouble((opcode >> 11) & 0x1F) * getDouble((opcode >> 16) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::divS(uint32_t opcode)
{
    // Divide a float by a float and store the result
    float value = getFloat((opcode >> 11) & 0x1F) / getFloat((opcode >> 16) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::divD(uint32_t opcode)
{
    // Divide a double by a double and store the result
    double value = getDouble((opcode >> 11) & 0x1F) / getDouble((opcode >> 16) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::sqrtS(uint32_t opcode)
{
    // Store the square root of a float
    getFloat((opcode >> 6) & 0x1F) = sqrt(getFloat((opcode >> 11) & 0x1F));
}

void CPU_CP1::sqrtD(uint32_t opcode)
{
    // Store the square root of a double
    getDouble((opcode >> 6) & 0x1F) = sqrt(getDouble((opcode >> 11) & 0x1F));
}

void CPU_CP1::absS(uint32_t opcode)
{
    // Store the absolute value of a float
    getFloat((opcode >> 6) & 0x1F) = fabs(getFloat((opcode >> 11) & 0x1F));
}

void CPU_CP1::absD(uint32_t opcode)
{
    // Store the absolute value of a double
    getDouble((opcode >> 6) & 0x1F) = fabs(getDouble((opcode >> 11) & 0x1F));
}

void CPU_CP1::movS(uint32_t opcode)
{
    // Copy a float to another register
    getFloat((opcode >> 6) & 0x1F) = getFloat((opcode >> 11) & 0x1F);
}

void CPU_CP1::movD(uint32_t opcode)
{
    // Copy a double to another register
    getDouble((opcode >> 6) & 0x1F) = getDouble((opcode >> 11) & 0x1F);
}

void CPU_CP1::negS(uint32_t opcode)
{
    // Store the negative value of a float
    getFloat((opcode >> 6) & 0x1F) = -getFloat((opcode >> 11) & 0x1F);
}

void CPU_CP1::negD(uint32_t opcode)
{
    // Store the negative value of a double
    getDouble((opcode >> 6) & 0x1F) = -getDouble((opcode >> 11) & 0x1F);
}

void CPU_CP1::roundWS(uint32_t opcode)
{
    // Convert a float to a 32-bit integer with forced rounding to nearest
    int backup = fegetround();
    fesetround(FE_TONEAREST);
    int32_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::roundWD(uint32_t opcode)
{
    // Convert a double to a 32-bit integer with forced rounding to nearest
    int backup = fegetround();
    fesetround(FE_TONEAREST);
    int32_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::roundLS(uint32_t opcode)
{
    // Convert a float to a 64-bit integer with forced rounding to nearest
    int backup = fegetround();
    fesetround(FE_TONEAREST);
    int64_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::roundLD(uint32_t opcode)
{
    // Convert a double to a 64-bit integer with forced rounding to nearest
    int backup = fegetround();
    fesetround(FE_TONEAREST);
    int64_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::truncWS(uint32_t opcode)
{
    // Convert a float to a 32-bit integer with forced rounding towards zero
    int backup = fegetround();
    fesetround(FE_TOWARDZERO);
    int32_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::truncWD(uint32_t opcode)
{
    // Convert a double to a 32-bit integer with forced rounding towards zero
    int backup = fegetround();
    fesetround(FE_TOWARDZERO);
    int32_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::truncLS(uint32_t opcode)
{
    // Convert a float to a 64-bit integer with forced rounding towards zero
    int backup = fegetround();
    fesetround(FE_TOWARDZERO);
    int64_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::truncLD(uint32_t opcode)
{
    // Convert a double to a 64-bit integer with forced rounding towards zero
    int backup = fegetround();
    fesetround(FE_TOWARDZERO);
    int64_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::ceilWS(uint32_t opcode)
{
    // Convert a float to a 32-bit integer with forced rounding upwards
    int backup = fegetround();
    fesetround(FE_UPWARD);
    int32_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::ceilWD(uint32_t opcode)
{
    // Convert a double to a 32-bit integer with forced rounding upwards
    int backup = fegetround();
    fesetround(FE_UPWARD);
    int32_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::ceilLS(uint32_t opcode)
{
    // Convert a float to a 64-bit integer with forced rounding upwards
    int backup = fegetround();
    fesetround(FE_UPWARD);
    int64_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::ceilLD(uint32_t opcode)
{
    // Convert a double to a 64-bit integer with forced rounding upwards
    int backup = fegetround();
    fesetround(FE_UPWARD);
    int64_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::floorWS(uint32_t opcode)
{
    // Convert a float to a 32-bit integer with forced rounding downwards
    int backup = fegetround();
    fesetround(FE_DOWNWARD);
    int32_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::floorWD(uint32_t opcode)
{
    // Convert a double to a 32-bit integer with forced rounding downwards
    int backup = fegetround();
    fesetround(FE_DOWNWARD);
    int32_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::floorLS(uint32_t opcode)
{
    // Convert a float to a 64-bit integer with forced rounding downwards
    int backup = fegetround();
    fesetround(FE_DOWNWARD);
    int64_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::floorLD(uint32_t opcode)
{
    // Convert a double to a 64-bit integer with forced rounding downwards
    int backup = fegetround();
    fesetround(FE_DOWNWARD);
    int64_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
    fesetround(backup);
}

void CPU_CP1::cvtSD(uint32_t opcode)
{
    // Convert a double to a float and store the result
    float value = getDouble((opcode >> 11) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::cvtSW(uint32_t opcode)
{
    // Convert a 32-bit integer to a float and store the result
    float value = (int32_t)read(CP1_32BIT, (opcode >> 11) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::cvtSL(uint32_t opcode)
{
    // Convert a 64-bit integer to a float and store the result
    float value = (int64_t)read(CP1_64BIT, (opcode >> 11) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::cvtDS(uint32_t opcode)
{
    // Convert a float to a double and store the result
    double value = getFloat((opcode >> 11) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::cvtDW(uint32_t opcode)
{
    // Convert a 32-bit integer to a double and store the result
    double value = (int32_t)read(CP1_32BIT, (opcode >> 11) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::cvtDL(uint32_t opcode)
{
    // Convert a 64-bit integer to a double and store the result
    double value = (int64_t)read(CP1_64BIT, (opcode >> 11) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CPU_CP1::cvtWS(uint32_t opcode)
{
    // Convert a float to a 32-bit integer and store the result
    int32_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
}

void CPU_CP1::cvtWD(uint32_t opcode)
{
    // Convert a double to a 32-bit integer and store the result
    int32_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_32BIT, (opcode >> 6) & 0x1F, value);
}

void CPU_CP1::cvtLS(uint32_t opcode)
{
    // Convert a float to a 64-bit integer and store the result
    int64_t value = nearbyint(getFloat((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
}

void CPU_CP1::cvtLD(uint32_t opcode)
{
    // Convert a double to a 64-bit integer and store the result
    int64_t value = nearbyint(getDouble((opcode >> 11) & 0x1F));
    write(CP1_64BIT, (opcode >> 6) & 0x1F, value);
}

void CPU_CP1::cf(uint32_t opcode)
{
    // Set the CP1 condition bit to false
    bool cond = false;
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::cunS(uint32_t opcode)
{
    // Set the CP1 condition bit if one of two floats is NaN
    bool cond = (std::isnan(getFloat((opcode >> 11) & 0x1F)) || std::isnan(getFloat((opcode >> 16) & 0x1F)));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::cunD(uint32_t opcode)
{
    // Set the CP1 condition bit if one of two doubles is NaN
    bool cond = (std::isnan(getDouble((opcode >> 11) & 0x1F)) || std::isnan(getDouble((opcode >> 16) & 0x1F)));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::ceqS(uint32_t opcode)
{
    // Set the CP1 condition bit if two floats are equal
    bool cond = (getFloat((opcode >> 11) & 0x1F) == getFloat((opcode >> 16) & 0x1F));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::ceqD(uint32_t opcode)
{
    // Set the CP1 condition bit if two doubles are equal
    bool cond = (getDouble((opcode >> 11) & 0x1F) == getDouble((opcode >> 16) & 0x1F));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::cueqS(uint32_t opcode)
{
    // Set the CP1 condition bit if two floats are equal or one of them is NaN
    float &a = getFloat((opcode >> 11) & 0x1F);
    float &b = getFloat((opcode >> 16) & 0x1F);
    bool cond = (std::isnan(a) || std::isnan(b) || a == b);
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::cueqD(uint32_t opcode)
{
    // Set the CP1 condition bit if two floats are equal or one of them is NaN
    double &a = getDouble((opcode >> 11) & 0x1F);
    double &b = getDouble((opcode >> 16) & 0x1F);
    bool cond = (std::isnan(a) || std::isnan(b) || a == b);
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::coltS(uint32_t opcode)
{
    // Set the CP1 condition bit if one float is less than another
    bool cond = (getFloat((opcode >> 11) & 0x1F) < getFloat((opcode >> 16) & 0x1F));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::coltD(uint32_t opcode)
{
    // Set the CP1 condition bit if one double is less than another
    bool cond = (getDouble((opcode >> 11) & 0x1F) < getDouble((opcode >> 16) & 0x1F));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::cultS(uint32_t opcode)
{
    // Set the CP1 condition bit if one float is less than another or one of them is NaN
    float &a = getFloat((opcode >> 11) & 0x1F);
    float &b = getFloat((opcode >> 16) & 0x1F);
    bool cond = (std::isnan(a) || std::isnan(b) || a < b);
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::cultD(uint32_t opcode)
{
    // Set the CP1 condition bit if one double is less than another or one of them is NaN
    double &a = getDouble((opcode >> 11) & 0x1F);
    double &b = getDouble((opcode >> 16) & 0x1F);
    bool cond = (std::isnan(a) || std::isnan(b) || a < b);
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::coleS(uint32_t opcode)
{
    // Set the CP1 condition bit if one float is less or equal to another
    bool cond = (getFloat((opcode >> 11) & 0x1F) <= getFloat((opcode >> 16) & 0x1F));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::coleD(uint32_t opcode)
{
    // Set the CP1 condition bit if one double is less or equal to another
    bool cond = (getDouble((opcode >> 11) & 0x1F) <= getDouble((opcode >> 16) & 0x1F));
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::culeS(uint32_t opcode)
{
    // Set the CP1 condition bit if one float is less or equal to another or one of them is NaN
    float &a = getFloat((opcode >> 11) & 0x1F);
    float &b = getFloat((opcode >> 16) & 0x1F);
    bool cond = (std::isnan(a) || std::isnan(b) || a <= b);
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::culeD(uint32_t opcode)
{
    // Set the CP1 condition bit if one double is less or equal to another or one of them is NaN
    double &a = getDouble((opcode >> 11) & 0x1F);
    double &b = getDouble((opcode >> 16) & 0x1F);
    bool cond = (std::isnan(a) || std::isnan(b) || a <= b);
    status = (status & ~(1 << 23)) | (cond << 23);
}

void CPU_CP1::unk(uint32_t opcode)
{
    // Warn about unknown instructions
    LOG_CRIT("Unknown FPU opcode: 0x%08X @ 0x%X\n", opcode, CPU::programCounter - 4);
}
