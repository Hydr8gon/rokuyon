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

#include "cp0.h"
#include "log.h"
#include "mi.h"
#include "vr4300.h"

namespace CP0
{
    uint32_t status;
    uint32_t cause;
    uint32_t exCounter;
}

void CP0::reset()
{
    // Reset the CP0 to its initial state
    status = 0;
    cause = 0;
    exCounter = 0;
}

uint32_t CP0::read(int index)
{
    // Read from a CP0 register if one exists at the given address
    switch (index)
    {
        case 12: // Status
            // Get the status register
            return status;

        case 13: // Cause
            // Get the cause register
            return cause;

        case 14: // EPC
            // Get the exception program counter
            return exCounter;
    }

    LOG_WARN("Read from unknown CP0 register: %d\n", index);
    return 0;
}

void CP0::write(int index, uint32_t value)
{
    // Write to a CP0 register if one exists at the given address
    switch (index)
    {
        case 12: // Status
            // Set the status register
            // TODO: actually use bits other than 0xFF03
            status = value & 0xFF57FFFF;
            checkInterrupts();
            return;

        case 13: // Cause
            // Set the software interrupt flags
            cause = (cause & ~0x300) | (value & 0x300);
            checkInterrupts();
            return;

        case 14: // EPC
            // Set the exception program counter
            exCounter = value;
            return;
    }

    LOG_WARN("Write to unknown CP0 register: %d\n", index);
}

void CP0::checkInterrupts()
{
    // Set the external interrupt bit if any MI interrupt is set
    cause = (cause & ~0x400) | ((bool)(MI::interrupt & MI::mask) << 10);

    // Trigger an interrupt if able and an enabled bit is set
    if (((status & 0x3) == 0x1) && (status & cause & 0xFF00))
        VR4300::exception();
}

