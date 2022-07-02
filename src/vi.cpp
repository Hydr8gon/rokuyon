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

#include <cstddef>

#include "vi.h"
#include "memory.h"

static uint32_t origin;

uint32_t *VI::getFramebuffer()
{
    // Read the framebuffer from memory
    static uint32_t framebuffer[320 * 240];
    for (size_t i = 0; i < 320 * 240; i++)
        framebuffer[i] = Memory::read<uint32_t>(origin + (i << 2));
    return framebuffer;
}

void VI::reset()
{
    // Reset the VI to its initial state
    origin = 0x80000000;
}

void VI::writeOrigin(uint32_t value)
{
    // Set the framebuffer address
    origin = 0x80000000 | (value & 0xFFFFFF);
}
