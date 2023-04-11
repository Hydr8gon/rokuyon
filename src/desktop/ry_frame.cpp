/*
    Copyright 2022-2023 Hydr8gon

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
#include "save_dialog.h"
#include "../core.h"

enum FrameEvent
{
    LOAD_ROM = 1,
    CHANGE_SAVE,
    QUIT,
    PAUSE,
    RESTART,
    STOP
};

wxBEGIN_EVENT_TABLE(ryFrame, wxFrame)
EVT_MENU(LOAD_ROM, ryFrame::loadRom)
EVT_MENU(CHANGE_SAVE, ryFrame::changeSave)
EVT_MENU(QUIT, ryFrame::quit)
EVT_MENU(PAUSE, ryFrame::pause)
EVT_MENU(RESTART, ryFrame::restart)
EVT_MENU(STOP, ryFrame::stop)
EVT_DROP_FILES(ryFrame::dropFiles)
EVT_CLOSE(ryFrame::close)
wxEND_EVENT_TABLE()

ryFrame::ryFrame(std::string path): wxFrame(nullptr, wxID_ANY, "rokuyon")
{
    // Set up the file menu
    fileMenu = new wxMenu();
    fileMenu->Append(LOAD_ROM, "&Load ROM");
    fileMenu->Append(CHANGE_SAVE, "&Change Save Type");
    fileMenu->AppendSeparator();
    fileMenu->Append(QUIT, "&Quit");

    // Set up the system menu
    systemMenu = new wxMenu();
    systemMenu->Append(PAUSE, "&Resume");
    systemMenu->Append(RESTART, "&Restart");
    systemMenu->Append(STOP, "&Stop");
    updateMenu();

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(systemMenu, "&System");
    SetMenuBar(menuBar);

    // Set up and show the window
    DragAcceptFiles(true);
    SetClientSize(wxSize(320, 240));
    SetMinClientSize(wxSize(320, 240));
    Centre();
    Show(true);

    // Set up a canvas for drawing the framebuffer
    canvas = new ryCanvas(this);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(canvas, 1, wxEXPAND);
    SetSizer(sizer);

    // Boot a ROM right away if a filename was given through the command line
    if (path != "")
        bootRom(path);
}

void ryFrame::bootRom(std::string path)
{
    // Remember the path so the ROM can be restarted
    lastPath = path;

    // Try to boot the specified ROM, and display an error if failed
    if (!Core::bootRom(path))
    {
        wxMessageDialog(this, "Make sure the ROM file is accessible and try again.",
            "Error Loading ROM", wxICON_NONE).ShowModal();
        return;
    }

    // Reset the system menu
    paused = false;
    updateMenu();
}

void ryFrame::updateMenu()
{
    if (Core::running)
    {
        // Enable some menu items when the core is running
        systemMenu->SetLabel(PAUSE, "&Pause");
        systemMenu->Enable(PAUSE, true);
        systemMenu->Enable(RESTART, true);
        systemMenu->Enable(STOP, true);
        fileMenu->Enable(CHANGE_SAVE, true);
    }
    else
    {
        // Disable some menu items when the core isn't running
        systemMenu->SetLabel(PAUSE, "&Resume");
        if (!paused)
        {
            systemMenu->Enable(PAUSE, false);
            systemMenu->Enable(RESTART, false);
            systemMenu->Enable(STOP, false);
            fileMenu->Enable(CHANGE_SAVE, false);
        }
    }
}

void ryFrame::Refresh()
{
    wxFrame::Refresh();

    // Override the refresh function to also update the FPS counter
    wxString label = "rokuyon";
    if (Core::running)
        label += wxString::Format(" - %d FPS", Core::fps);
    SetLabel(label);
}

void ryFrame::loadRom(wxCommandEvent &event)
{
    // Show the file browser
    wxFileDialog romSelect(this, "Select ROM File", "", "", "N64 ROM files (*.z64)|*.z64", wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    // Boot a ROM if a file was selected
    if (romSelect.ShowModal() != wxID_CANCEL)
        bootRom((const char*)romSelect.GetPath().mb_str(wxConvUTF8));
}

void ryFrame::changeSave(wxCommandEvent &event)
{
    // Show the save type dialog
    SaveDialog saveDialog(lastPath);
    saveDialog.ShowModal();
}

void ryFrame::quit(wxCommandEvent &event)
{
    // Close the program
    Close(true);
}

void ryFrame::pause(wxCommandEvent &event)
{
    // Temporarily stop or start the emulator
    if ((paused = !paused))
        Core::stop();
    else
        Core::start();
    updateMenu();
}

void ryFrame::restart(wxCommandEvent &event)
{
    // Boot the most recently loaded ROM again
    ryFrame::bootRom(lastPath);
}

void ryFrame::stop(wxCommandEvent &event)
{
    // Stop the emulator and reset the system menu
    Core::stop();
    paused = false;
    updateMenu();
}

void ryFrame::dropFiles(wxDropFilesEvent &event)
{
    // Boot a ROM if a single file is dropped onto the frame
    if (event.GetNumberOfFiles() == 1)
    {
        wxString path = event.GetFiles()[0];
        if (wxFileExists(path))
            bootRom((const char*)path.mb_str(wxConvUTF8));
    }
}

void ryFrame::close(wxCloseEvent &event)
{
    // Stop emulation before exiting
    Core::stop();
    canvas->finish();
    event.Skip(true);
}
