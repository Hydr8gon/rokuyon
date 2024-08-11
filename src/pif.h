/*
    Copyright 2022-2024 Hydr8gon

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

#ifndef PIF_H
#define PIF_H

#include <cstdint>
#include <cstdio>

namespace PIF
{
    extern uint8_t memory[0x800];

    void reset();
    void runCommand();

    void pressKey(int key);
    void releaseKey(int key);
    void setStick(int x, int y);
}

#endif // PIF_H
