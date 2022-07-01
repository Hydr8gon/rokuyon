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
#include "../vi.h"

wxBEGIN_EVENT_TABLE(ryCanvas, wxGLCanvas)
EVT_PAINT(ryCanvas::draw)
EVT_SIZE(ryCanvas::resize)
wxEND_EVENT_TABLE()

ryCanvas::ryCanvas(wxFrame *frame): wxGLCanvas(frame, wxID_ANY, nullptr), frame(frame)
{
    // Prepare the OpenGL context
    context = new wxGLContext(this);
}

void ryCanvas::draw(wxPaintEvent &event)
{
    if (!Core::running) return;
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

    // Get the framebuffer as a texture
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 320, 240, 0, GL_RGBA, GL_UNSIGNED_BYTE, VI::getFramebuffer());

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
