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

#ifndef VI_H
#define VI_H

#include <cstdint>

struct Framebuffer
{
    uint32_t *data;
    uint32_t width;
    uint32_t height;
};

namespace VI
{
    Framebuffer *getFramebuffer();

    void reset();
    void drawFrame();

    void writeControl(uint32_t value);
    void writeOrigin(uint32_t value);
    void writeWidth(uint32_t value);
    void writeYScale(uint32_t value);
}

#endif // VI_H
