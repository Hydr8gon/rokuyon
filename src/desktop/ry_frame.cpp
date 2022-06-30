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
#include "../core.h"

enum FrameEvent
{
    LOAD_ROM = 1,
    QUIT
};

wxBEGIN_EVENT_TABLE(ryFrame, wxFrame)
EVT_MENU(LOAD_ROM, ryFrame::loadRom)
EVT_MENU(QUIT,     ryFrame::quit)
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

    Show(true);
}

void ryFrame::loadRom(wxCommandEvent &event)
{
    // Show the file browser and try to boot the selected ROM
    wxFileDialog romSelect(this, "Select ROM File", "", "", "N64 ROM files (*.z64)|*.z64", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (romSelect.ShowModal() != wxID_CANCEL && !Core::bootRom((const char*)romSelect.GetPath().mb_str(wxConvUTF8)))
        wxMessageDialog(this, "Make sure the ROM file is accessible and try again.", "Error Loading ROM", wxICON_NONE).ShowModal();
}

void ryFrame::quit(wxCommandEvent &event)
{
    // Close the program
    Close(true);
}
