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

#include "cpu_cp0.h"
#include "cpu.h"
#include "cpu_cp1.h"
#include "log.h"
#include "mi.h"

namespace CPU_CP0
{
    uint32_t count;
    uint32_t compare;
    uint32_t status;
    uint32_t cause;
    uint32_t epc;
    uint32_t errorEpc;
}

void CPU_CP0::reset()
{
    // Reset the CPU CP0 to its initial state
    count = 0;
    compare = 0;
    status = 0x400004;
    cause = 0;
    epc = 0;
    errorEpc = 0;
}

uint32_t CPU_CP0::read(int index)
{
    // Read from a CPU CP0 register if one exists at the given index
    switch (index)
    {
        case 9: // Count
            // Get the count register
            return count;

        case 11: // Compare
            // Get the compare register
            return compare;

        case 12: // Status
            // Get the status register
            return status;

        case 13: // Cause
            // Get the cause register
            return cause;

        case 14: // EPC
            // Get the exception program counter
            return epc;

        case 30: // ErrorEPC
            // Get the error exception program counter
            return errorEpc;

        default:
            LOG_WARN("Read from unknown CPU CP0 register: %d\n", index);
            return 0;
    }
}

void CPU_CP0::write(int index, uint32_t value)
{
    // Write to a CPU CP0 register if one exists at the given index
    switch (index)
    {
        case 9: // Count
            // Set the count register
            count = value;
            return;

        case 11: // Compare
            // Set the compare register and acknowledge a timer interrupt
            compare = value;
            cause &= ~0x8000;
            return;

        case 12: // Status
            // Set the status register and apply the FR bit to the CP1
            status = value & 0xFF57FFFF;
            checkInterrupts();
            CPU_CP1::setRegMode(status & (1 << 26));

            // Keep track of unimplemented bits that should do something
            if (uint32_t bits = (value & 0xB0000E0))
                LOG_WARN("Unimplemented CPU CP0 status bits set: 0x%X\n", bits);
            return;

        case 13: // Cause
            // Set the software interrupt flags
            cause = (cause & ~0x300) | (value & 0x300);
            checkInterrupts();
            return;

        case 14: // EPC
            // Set the exception program counter
            epc = value;
            return;

        case 30: // ErrorEPC
            // Set the error exception program counter
            errorEpc = value;
            return;

        default:
            LOG_WARN("Write to unknown CPU CP0 register: %d\n", index);
            return;
    }
}

void CPU_CP0::updateCount()
{
    // Increment count and request a timer interrupt when it matches compare
    if (++count == compare)
    {
        cause |= 0x8000;
        checkInterrupts();
    }
}

void CPU_CP0::checkInterrupts()
{
    // Set the external interrupt bit if any MI interrupt is set
    cause = (cause & ~0x400) | ((bool)(MI::interrupt & MI::mask) << 10);

    // Trigger an interrupt if able and an enabled bit is set
    if (((status & 0x3) == 0x1) && (status & cause & 0xFF00))
        CPU::exception(0);
}

uint32_t CPU_CP0::exception(uint32_t programCounter, uint8_t type)
{
    // Update registers for an exception and return the vector address
    status |= 0x2; // EXL
    cause = (cause & ~0x7C) | ((type << 2) & 0x7C);
    epc = programCounter - (type ? 4 : 0);
    return (status & (1 << 22)) ? 0xBFC00200 : 0x80000000;
}

bool CPU_CP0::cpUsable(uint8_t cp)
{
    // Check if a coprocessor is usable (CP0 is always usable in kernel mode)
    if (!(status & (1 << (28 + cp))) && (cp > 0 || (!(status & 0x6) && (status & 0x18))))
    {
        // Set the coprocessor number bits
        cause = (cause & ~(0x3 << 28)) | ((cp & 0x3) << 28);
        return false;
    }

    return true;
}
