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

#ifndef RSP_CP2_H
#define RSP_CP2_H

#include <cstdint>

namespace RSP_CP2
{
    extern void (*vecInstrs[])(uint32_t);

    void reset();
    int16_t read(bool control, int index, int byte);
    void write(bool control, int index, int byte, int16_t value);
}

#endif // RSP_CP2_H
