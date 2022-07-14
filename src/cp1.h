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

#ifndef CP1_H
#define CP1_H

#include <cstdint>

enum CP1Type
{
    CP1_32BIT = 0,
    CP1_64BIT,
    CP1_CTRL
};

namespace CP1
{
    extern void (*sglInstrs[])(uint32_t);
    extern void (*dblInstrs[])(uint32_t);
    extern void (*wrdInstrs[])(uint32_t);
    extern void (*lwdInstrs[])(uint32_t);

    void reset();
    uint64_t read(CP1Type type, int index);
    void write(CP1Type type, int index, uint64_t value);
    void setRegMode(bool full);
}

#endif // CP1_H
