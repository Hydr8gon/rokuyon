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

#ifndef CPU_CP0_H
#define CPU_CP0_H

#include <cstdint>

namespace CPU_CP0
{
    extern void (*cp0Instrs[])(uint32_t);

    void reset();
    uint32_t read(int index);
    void write(int index, uint32_t value);

    void resetCycles();
    void checkInterrupts();
    void exception(uint8_t type);
    void setTlbAddress(uint32_t address);
    bool cpUsable(uint8_t cp);
}

#endif // CPU_CP0_H
