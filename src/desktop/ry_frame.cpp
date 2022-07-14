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

#include "ry_frame.h"
#include "ry_canvas.h"
#include "../core.h"

enum FrameEvent
{
    LOAD_ROM = 1,
    QUIT
};

wxBEGIN_EVENT_TABLE(ryFrame, wxFrame)
EVT_MENU(LOAD_ROM, ryFrame::loadRom)
EVT_MENU(QUIT,     ryFrame::quit)
EVT_CLOSE(ryFrame::close)
wxEND_EVENT_TABLE()

ryFrame::ryFrame(): wxFrame(nullptr, wxID_ANY, "rokuyon")
{
    // Set up the File menu
    wxMenu *fileMenu = new wxMenu();
    fileMenu->Append(LOAD_ROM, "&Load ROM");
    fileMenu->AppendSeparator();
    fileMenu->Append(QUIT, "&Quit");

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    SetMenuBar(menuBar);

    // Set up and show the window
    SetClientSize(wxSize(320, 240));
    SetMinClientSize(wxSize(320, 240));
    Centre();
    Show(true);

    // Set up a canvas for drawing the framebuffer
    canvas = new ryCanvas(this);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(canvas, 1, wxEXPAND);
    SetSizer(sizer);
}

void ryFrame::loadRom(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog romSelect(this, "Select ROM File", "", "", "N64 ROM files (*.z64)|*.z64", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    // Try to boot the selected ROM and handle errors
    if (romSelect.ShowModal() != wxID_CANCEL)
    {
        switch (Core::bootRom((const char*)romSelect.GetPath().mb_str(wxConvUTF8)))
        {
            case 1: // PIF ROM load failed
                wxMessageDialog(this, "Make sure a valid PIF dump named pif_rom.bin is next to the executable.",
                    "Error Loading PIF ROM", wxICON_NONE).ShowModal();
                break;

            case 2: // Cart ROM load failed
                wxMessageDialog(this, "Make sure the ROM file is accessible and try again.",
                    "Error Loading Cart ROM", wxICON_NONE).ShowModal();
                break;
        }
    }
}

void ryFrame::quit(wxCommandEvent &event)
{
    // Close the program
    Close(true);
}

void ryFrame::close(wxCloseEvent &event)
{
    // Stop emulation before exiting
    Core::stop();
    canvas->finish();
    event.Skip(true);
}
