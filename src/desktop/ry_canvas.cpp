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

#include "ry_canvas.h"
#include "../core.h"
#include "../pif.h"
#include "../vi.h"

const char ryCanvas::keyMap[16] =
{
    'L', 'K', 'J', 'G', 'W', 'S', 'A', 'D',
     0 ,  0 , 'Q', 'P', '8', '2', '4', '6'
};

wxBEGIN_EVENT_TABLE(ryCanvas, wxGLCanvas)
EVT_PAINT(ryCanvas::draw)
EVT_SIZE(ryCanvas::resize)
EVT_KEY_DOWN(ryCanvas::pressKey)
EVT_KEY_UP(ryCanvas::releaseKey)
wxEND_EVENT_TABLE()

ryCanvas::ryCanvas(wxFrame *frame): wxGLCanvas(frame, wxID_ANY, nullptr), frame(frame)
{
    // Prepare the OpenGL context
    context = new wxGLContext(this);

    // Set focus so that key presses will be registered
    SetFocus();
}

void ryCanvas::finish()
{
    // Tell the canvas to stop rendering
    finished = true;
}

void ryCanvas::draw(wxPaintEvent &event)
{
    // Stop rendering so the program can close
    if (finished)
        return;

    SetCurrent(*context);
    static bool setup = false;

    if (!setup)
    {
        // Prepare a texture for the framebuffer
        GLuint texture;
        glEnable(GL_TEXTURE_2D);
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // Finish initial setup
        frame->SendSizeEvent();
        setup = true;
    }

    // Clear the screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (Core::running)
    {
        // At the swap interval, get the framebuffer as a texture
        if (++frameCount >= swapInterval)
        {
            if (Framebuffer *fb = VI::getFramebuffer())
            {
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb->width,
                    fb->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, fb->data);
                frameCount = 0;
            }
        }

        // Submit the polygon vertices
        glBegin(GL_QUADS);
        glTexCoord2i(1, 1);
        glVertex2i(x + width, y + height);
        glTexCoord2i(0, 1);
        glVertex2i(x, y + height);
        glTexCoord2i(0, 0);
        glVertex2i(x, y);
        glTexCoord2i(1, 0);
        glVertex2i(x + width, y);
        glEnd();
    }

    // Track the refresh rate and update the swap interval every second
    // Speed is limited by drawing, so this tries to keep it at 60 Hz
    refreshRate++;
    std::chrono::duration<double> rateTime = std::chrono::steady_clock::now() - lastRateTime;
    if (rateTime.count() >= 1.0f)
    {
        swapInterval = (refreshRate + 5) / 60; // Margin of 5
        refreshRate = 0;
        lastRateTime = std::chrono::steady_clock::now();
    }

    // Finish the frame
    glFinish();
    SwapBuffers();
}

void ryCanvas::resize(wxSizeEvent &event)
{
    // Update the canvas dimensions
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    wxSize size = GetSize();
    glOrtho(0, size.x, size.y, 0, -1, 1);
    glViewport(0, 0, size.x, size.y);

    // Set the layout to be centered and as large as possible
    if (((float)size.x / size.y) > (320.0f / 240)) // Wide
    {
        width = 320 * size.y / 240;
        height = size.y;
        x = (size.x - width) / 2;
        y = 0;
    }
    else // Tall
    {
        width = size.x;
        height = 240 * size.x / 320;
        x = 0;
        y = (size.y - height) / 2;
    }
}

void ryCanvas::pressKey(wxKeyEvent &event)
{
    // Trigger a key press if a mapped key was pressed
    for (int i = 0; i < 16; i++)
    {
        if (event.GetKeyCode() == keyMap[i])
            PIF::pressKey(i);
    }
}

void ryCanvas::releaseKey(wxKeyEvent &event)
{
    // Trigger a key release if a mapped key was released
    for (int i = 0; i < 16; i++)
    {
        if (event.GetKeyCode() == keyMap[i])
            PIF::releaseKey(i);
    }
}
