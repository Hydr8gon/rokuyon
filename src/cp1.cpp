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

#include "cp1.h"
#include "log.h"
#include "vr4300.h"

namespace CP1
{
    bool fullMode;
    uint64_t registers[32];
    uint32_t status;

    float &getFloat(int index);
    double &getDouble(int index);

    void addS(uint32_t opcode);
    void addD(uint32_t opcode);
    void unk(uint32_t opcode);
}

// Single-precision FPU instruction lookup table, using opcode bits 0-5
void (*CP1::sglInstrs[0x40])(uint32_t) =
{
    addS, unk, unk, unk, unk, unk, unk, unk, // 0x00-0x07
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x08-0x0F
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x10-0x17
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x18-0x1F
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x20-0x27
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x28-0x2F
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x30-0x37
    unk,  unk, unk, unk, unk, unk, unk, unk  // 0x38-0x3F
};

// Double-precision FPU instruction lookup table, using opcode bits 0-5
void (*CP1::dblInstrs[0x40])(uint32_t) =
{
    addD, unk, unk, unk, unk, unk, unk, unk, // 0x00-0x07
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x08-0x0F
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x10-0x17
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x18-0x1F
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x20-0x27
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x28-0x2F
    unk,  unk, unk, unk, unk, unk, unk, unk, // 0x30-0x37
    unk,  unk, unk, unk, unk, unk, unk, unk  // 0x38-0x3F
};

// 32-bit fixed-point FPU instruction lookup table, using opcode bits 0-5
void (*CP1::wrdInstrs[0x40])(uint32_t) =
{
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x00-0x07
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x08-0x0F
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x10-0x17
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x18-0x1F
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x20-0x27
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x28-0x2F
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x30-0x37
    unk, unk, unk, unk, unk, unk, unk, unk  // 0x38-0x3F
};

// 64-bit fixed-point FPU instruction lookup table, using opcode bits 0-5
void (*CP1::lwdInstrs[0x40])(uint32_t) =
{
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x00-0x07
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x08-0x0F
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x10-0x17
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x18-0x1F
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x20-0x27
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x28-0x2F
    unk, unk, unk, unk, unk, unk, unk, unk, // 0x30-0x37
    unk, unk, unk, unk, unk, unk, unk, unk  // 0x38-0x3F
};

void CP1::reset()
{
    // Reset the CP1 to its initial state
    fullMode = false;
    memset(registers, 0, sizeof(registers));
    status = 0;
}

uint64_t CP1::read(CP1Type type, int index)
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
            // Read from a CP1 control register if one exists at the given address
            switch (index)
            {
                case 31: // Status
                    // Get the status register
                    return status;

                default:
                    LOG_WARN("Read from unknown CP1 control register: %d\n", index);
                    return 0;
            }
    }
}

void CP1::write(CP1Type type, int index, uint64_t value)
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
            // Write to a CP1 control register if one exists at the given address
            switch (index)
            {
                case 31: // Status
                    // Set the status register
                    status = value & 0x183FFFF;

                    // Keep track of unimplemented bits that should do something
                    if (uint32_t bits = (value & 0x1000F83))
                        LOG_WARN("Unimplemented CP1 status bits set: 0x%X\n", bits);
                    return;

                default:
                    LOG_WARN("Write to unknown CP1 control register: %d\n", index);
                    return;
            }
    }
}

void CP1::setRegMode(bool full)
{
    // Set the register mode to either full or half
    fullMode = full;
}

inline float &CP1::getFloat(int index)
{
    // Get a 32-bit register as a float, mapped based on the current mode
    // TODO: make this endian-safe
    return fullMode ? *(float*)&registers[index] :
        *((float*)&registers[index & ~1] + (index & 1));
}

inline double &CP1::getDouble(int index)
{
    // Get a 64-bit register as a double
    // This should be endian-safe as long as double and uint64_t have the same size
    return *(double*)&registers[index];
}

void CP1::addS(uint32_t opcode)
{
    // Add a float to a float and store the result
    float value = getFloat((opcode >> 11) & 0x1F) + getFloat((opcode >> 16) & 0x1F);
    getFloat((opcode >> 6) & 0x1F) = value;
}

void CP1::addD(uint32_t opcode)
{
    // Add a double to a double and store the result
    double value = getDouble((opcode >> 11) & 0x1F) + getDouble((opcode >> 16) & 0x1F);
    getDouble((opcode >> 6) & 0x1F) = value;
}

void CP1::unk(uint32_t opcode)
{
    // Warn about unknown instructions
    LOG_CRIT("Unknown FPU opcode: 0x%08X @ 0x%X\n", opcode, VR4300::programCounter - 4);
}
