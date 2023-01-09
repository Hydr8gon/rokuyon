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
    _Framebuffer fb;
    bool fb_realloc;
    std::atomic<bool> ready;

    uint32_t control;
    uint32_t origin;
    uint32_t width;
    uint32_t yScale;

    void drawFrame();
}

_Framebuffer *VI::getFramebuffer()
{
    // Wait until a new frame is ready
    if (!ready.load())
        return nullptr;

    // Release the original framebuffer and return the copied one
    ready.store(false);
    return &fb;
}

void VI::reset()
{
    // Reset the VI to its initial state
    control = 0;
    origin = 0;
    width = 0;
    yScale = 0;

    fb_realloc = false;

    // Schedule the first frame to be drawn
    Core::schedule(drawFrame, (93750000 / 60) * 2);
}

uint32_t VI::read(uint32_t address)
{
    // Read from an I/O register if one exists at the given address
    switch (address)
    {
        case 0x4400000: // VI_CONTROL
            return control;

        default:
            LOG_WARN("Unknown VI register read: 0x%X\n", address);
            return 0;
    }
}

void VI::write(uint32_t address, uint32_t value)
{
    uint32_t new_width;
    uint32_t new_yScale;

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
            new_width = (value & 0xFFF);
            fb_realloc = new_width != width;
            width = new_width;
            return;

        case 0x4400010: // VI_V_CURRENT
            // Acknowledge a VI interrupt instead of writing a value
            MI::clearInterrupt(3);
            return;

        case 0x4400034: // VI_Y_SCALE
            // Set the framebuffer Y-scale
            // TODO: actually use offset value
            new_yScale = (value & 0xFFF0FFF);
            fb_realloc = new_yScale != yScale;
            yScale = new_yScale;
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

    size_t num_pix;
    size_t size;

    if (fb_realloc)
    {
        // Set the frame width and height based on registers
        fb.width = width;
        fb.height = ((yScale & 0xFFF) * 240) >> 10;

        // Allocate a framebuffer of the correct size
        if (fb.data) delete[] fb.data;
        num_pix = (size_t)(fb.width * fb.height);
        size = num_pix * sizeof(uint32_t);
        fb.data = new uint32_t[size];

        fb_realloc = false;
    }
    else
    {
        num_pix = (size_t)(fb.width * fb.height);
        size = num_pix * sizeof(uint32_t);
    }

    // Read the framebuffer from N64 memory
    switch (control & 0x3) // Type
    {
        case 0x3: // 32-bit
            // Translate pixels from RGB_8888 to ARGB8888
            for (size_t i = 0; i < num_pix; i++)
            {
                uint32_t color = Memory::read<uint32_t>(origin + (uint32_t)(i << 2));
                uint8_t r = (color >> 24) & 0xFF;
                uint8_t g = (color >> 16) & 0xFF;
                uint8_t b = (color >>  8) & 0xFF;
                fb.data[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
            break;

        case 0x2: // 16-bit
            // Translate pixels from RGB_5551 to ARGB8888
            for (size_t i = 0; i < num_pix; i++)
            {
                uint16_t color = Memory::read<uint16_t>(origin + (uint32_t)(i << 1));
                uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
                uint8_t g = ((color >>  6) & 0x1F) * 255 / 31;
                uint8_t b = ((color >>  1) & 0x1F) * 255 / 31;
                fb.data[i] = (0xFF << 24) | (b << 16) | (g << 8) | r;
            }
            break;

        default:
            // Don't show anything
            memset(fb.data, 0, size);
            break;
    }

    // Finish the frame and request a VI interrupt
    // TODO: request interrupt at the proper time
    ready.store(true);
    MI::setInterrupt(3);

    // Schedule the next frame to be drawn
    Core::schedule(drawFrame, (93750000 / 60) * 2);
    Core::countFrame();
}
