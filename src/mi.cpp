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

#include "mi.h"
#include "cp0.h"
#include "log.h"

namespace MI
{
    uint32_t interrupt;
    uint32_t mask;
}

void MI::reset()
{
    // Reset the MI to its initial state
    interrupt = 0;
    mask = 0;
}

uint32_t MI::read(uint32_t address)
{
    // Read from an I/O register if one exists at the given address
    switch (address)
    {
        case 0x4300008: // MI_INTERRUPT
            // Get the interrupt flags
            return interrupt;

        case 0x430000C: // MI_MASK
            // Get the interrupt mask
            return mask;

        default:
            LOG_WARN("Unknown MI register read: 0x%X\n", address);
            return 0;
    }
}

void MI::write(uint32_t address, uint32_t value)
{
    // Write to an I/O register if one exists at the given address
    switch (address)
    {
        case 0x430000C: // MI_MASK
            // For each set bit, set or clear a mask bit appropriately
            for (int i = 0; i < 12; i += 2)
            {
                if (value & (1 << (i + 1)))
                    mask |= (1 << (i / 2));
                else if (value & (1 << i))
                    mask &= ~(1 << (i / 2));
            }

            CP0::checkInterrupts();
            return;

        default:
            LOG_WARN("Unknown MI register write: 0x%X\n", address);
            return;
    }
}

void MI::setInterrupt(int bit)
{
    // Request an interrupt by setting its bit
    interrupt |= (1 << bit);
    CP0::checkInterrupts();
}

void MI::clearInterrupt(int bit)
{
    // Acknowledge an interrupt by clearing its bit
    interrupt &= ~(1 << bit);
    CP0::checkInterrupts();
}
