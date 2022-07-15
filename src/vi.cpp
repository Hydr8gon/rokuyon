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

#include <atomic>
#include <cstddef>
#include <cstring>
#include <thread>

#include "vi.h"
#include "core.h"
#include "log.h"
#include "memory.h"
#include "mi.h"

namespace VI
{
    Framebuffer fbs[2];
    std::atomic<bool> ready;

    uint32_t control;
    uint32_t origin;
    uint32_t width;
    uint32_t yScale;
}

Framebuffer *VI::getFramebuffer()
{
    // Wait until a new frame is ready
    if (!ready.load())
        return nullptr;

    // Get the width and height of the frame
    fbs[1].width  = fbs[0].width;
    fbs[1].height = fbs[0].height;

    // Make a copy of the framebuffer so the original can be overwritten
    if (fbs[1].data) delete[] fbs[1].data;
    size_t size = fbs[1].width * fbs[1].height * sizeof(uint32_t);
    fbs[1].data = new uint32_t[size];
    memcpy(fbs[1].data, fbs[0].data, size);

    // Release the original framebuffer and return the copied one
    ready.store(false);
    return &fbs[1];
}

void VI::reset()
{
    // Reset the VI to its initial state
    control = 0;
    origin = 0;
    width = 0;
    yScale = 0;
}

uint32_t VI::read(uint32_t address)
{
    // Read from an I/O register if one exists at the given address
    switch (address)
    {
        default:
            LOG_WARN("Unknown VI register read: 0x%X\n", address);
            return 0;
    }
}

void VI::write(uint32_t address, uint32_t value)
{
    // Write to an I/O register if one exists at the given address
    switch (address)
    {
        case 0x4400000: // VI_CONTROL
            // Set the VI control register
            // TODO: actually use bits other than type
            control = (value & 0x1FBFF);
            return;

        case 0x4400004: // VI_ORIGIN
            // Set the framebuffer address
            origin = 0x80000000 | (value & 0xFFFFFF);
            return;

        case 0x4400008: // VI_WIDTH
            // Set the framebuffer width in pixels
            width = (value & 0xFFF);
            return;

        case 0x4400010: // VI_V_CURRENT
            // Acknowledge a VI interrupt instead of writing a value
            MI::clearInterrupt(3);
            return;

        case 0x4400034: // VI_Y_SCALE
            // Set the framebuffer Y-scale
            // TODO: actually use offset value
            yScale = (value & 0xFFF0FFF);
            return;

        default:
            LOG_WARN("Unknown VI register write: 0x%X\n", address);
            return;
    }
}

void VI::drawFrame()
{
    // Wait until the last frame has been copied
    while (Core::running && ready.load())
        std::this_thread::yield();

    // Set the frame width and height based on registers
    fbs[0].width = width;
    fbs[0].height = ((yScale & 0xFFF) * 240) >> 10;

    // Allocate a framebuffer of the correct size
    if (fbs[0].data) delete[] fbs[0].data;
    size_t size = fbs[0].width * fbs[0].height;
    fbs[0].data = new uint32_t[size * sizeof(uint32_t)];

    // Read the framebuffer from N64 memory
    switch (control & 0x3) // Type
    {
        case 0x3: // 32-bit
            // Translate pixels from RGB_8888 to ARGB8888
            for (size_t i = 0; i < size; i++)
            {
                uint32_t color = Memory::read<uint32_t>(origin + (i << 2));
                uint8_t r = (color >> 24) & 0xFF;
                uint8_t g = (color >> 16) & 0xFF;
                uint8_t b = (color >>  8) & 0xFF;
                fbs[0].data[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
            break;

        case 0x2: // 16-bit
            // Translate pixels from RGB_5551 to ARGB8888
            for (size_t i = 0; i < size; i++)
            {
                uint16_t color = Memory::read<uint16_t>(origin + (i << 1));
                uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
                uint8_t g = ((color >>  6) & 0x1F) * 255 / 31;
                uint8_t b = ((color >>  1) & 0x1F) * 255 / 31;
                fbs[0].data[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
            break;

        default:
            // Don't show anything
            memset(fbs[0].data, 0, size);
            break;
    }

    // Finish the frame and request a VI interrupt
    // TODO: request interrupt at the proper time
    ready.store(true);
    MI::setInterrupt(3);
}
