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
#include "input_dialog.h"
#include "save_dialog.h"
#include "../core.h"
#include "../pif.h"
#include "../settings.h"

enum FrameEvent
{
    LOAD_ROM = 1,
    CHANGE_SAVE,
    QUIT,
    PAUSE,
    RESTART,
    STOP,
    INPUT_BINDINGS,
    FPS_LIMITER,
    UPDATE_JOY
};

wxBEGIN_EVENT_TABLE(ryFrame, wxFrame)
EVT_MENU(LOAD_ROM, ryFrame::loadRom)
EVT_MENU(CHANGE_SAVE, ryFrame::changeSave)
EVT_MENU(QUIT, ryFrame::quit)
EVT_MENU(PAUSE, ryFrame::pause)
EVT_MENU(RESTART, ryFrame::restart)
EVT_MENU(STOP, ryFrame::stop)
EVT_MENU(INPUT_BINDINGS, ryFrame::inputSettings)
EVT_MENU(FPS_LIMITER, ryFrame::toggleFpsLimit)
EVT_TIMER(UPDATE_JOY, ryFrame::updateJoystick)
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

    // Set up the settings menu
    wxMenu *settingsMenu = new wxMenu();
    settingsMenu->Append(INPUT_BINDINGS, "&Input Bindings");
    settingsMenu->AppendCheckItem(FPS_LIMITER, "&FPS Limiter");
    settingsMenu->Check(FPS_LIMITER, Settings::fpsLimiter);

    // Set up the menu bar
    wxMenuBar *menuBar = new wxMenuBar();
    menuBar->Append(fileMenu, "&File");
    menuBar->Append(systemMenu, "&System");
    menuBar->Append(settingsMenu, "&Settings");
    SetMenuBar(menuBar);

    // Set up and show the window
    DragAcceptFiles(true);
    SetClientSize(wxSize(480, 360));
    SetMinClientSize(wxSize(480, 360));
    Centre();
    Show(true);

    // Set up a canvas for drawing the framebuffer
    canvas = new ryCanvas(this);
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(canvas, 1, wxEXPAND);
    SetSizer(sizer);

    // Prepare a joystick if one is connected
    joystick = new wxJoystick();
    if (joystick->IsOk())
    {
        // Save the initial axis values so inputs can be detected as offsets instead of raw values
        // This avoids issues with axes that have non-zero values in their resting positions
        for (int i = 0; i < joystick->GetNumberAxes(); i++)
            axisBases.push_back(joystick->GetPosition(i));

        // Start a timer to update joystick input, since wxJoystickEvents are unreliable
        timer = new wxTimer(this, UPDATE_JOY);
        timer->Start(10);
    }
    else
    {
        // Don't use a joystick if one isn't connected
        delete joystick;
        joystick = nullptr;
        timer = nullptr;
    }

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

void ryFrame::pressKey(int key)
{
    if (key < 8)
    {
        // Press a key before the 2 unused keys
        PIF::pressKey(key);
    }
    else if (key < 14)
    {
        // Press a key after the 2 unused keys
        PIF::pressKey(key + 2);
    }
    else if (key < 19)
    {
        // Press a custom key-stick key
        stickPressed[key - 14] = true;
        updateKeyStick();
    }
}

void ryFrame::releaseKey(int key)
{
    if (key < 8)
    {
        // Release a key before the 2 unused keys
        PIF::releaseKey(key);
    }
    else if (key < 14)
    {
        // Release a key after the 2 unused keys
        PIF::releaseKey(key + 2);
    }
    else if (key < 19)
    {
        // Release a custom key-stick key
        stickPressed[key - 14] = false;
        updateKeyStick();
    }
}

void ryFrame::updateKeyStick()
{
    int stickX = 0;
    int stickY = 0;

    // Apply the base stick movement from pressed keys
    if (stickPressed[0]) stickY += 80;
    if (stickPressed[1]) stickY -= 80;
    if (stickPressed[2]) stickX -= 80;
    if (stickPressed[3]) stickX += 80;

    // Scale diagonals to create a round boundary
    if (stickX && stickY)
    {
        stickX = stickX * 60 / 80;
        stickY = stickY * 60 / 80;
    }

    // Half the coordinates when the stick modifier is applied
    if (stickPressed[4])
    {
        stickX /= 2;
        stickY /= 2;
    }

    // Update the stick coordinates
    PIF::setStick(stickX, stickY);
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

void ryFrame::inputSettings(wxCommandEvent &event)
{
    // Pause joystick updates and show the input settings dialog
    if (timer) timer->Stop();
    InputDialog inputDialog(joystick);
    inputDialog.ShowModal();
    if (timer) timer->Start(10);
}

void ryFrame::toggleFpsLimit(wxCommandEvent &event)
{
    // Toggle the FPS limiter setting
    Settings::fpsLimiter = !Settings::fpsLimiter;
    Settings::save();
}

void ryFrame::updateJoystick(wxTimerEvent &event)
{
    int stickX = 0;
    int stickY = 0;

    // Check the status of mapped joystick inputs
    for (int i = 0; i < MAX_KEYS; i++)
    {
        if (ryApp::keyBinds[i] >= 3000 && joystick->GetNumberAxes() > ryApp::keyBinds[i] - 3000) // Axis -
        {
            int j = ryApp::keyBinds[i] - 3000;
            if (joystick->GetPosition(j) < axisBases[j])
            {
                switch (i)
                {
                    case 14: // Stick Up
                        // Scale the axis position and apply it to the stick in the up direction
                        stickY += joystick->GetPosition(j) * 80 / joystick->GetYMin();
                        continue;

                    case 15: // Stick Down
                        // Scale the axis position and apply it to the stick in the down direction
                        stickY -= joystick->GetPosition(j) * 80 / joystick->GetYMax();
                        continue;

                    case 16: // Stick Left
                        // Scale the axis position and apply it to the stick in the left direction
                        stickX -= joystick->GetPosition(j) * 80 / joystick->GetXMin();
                        continue;

                    case 17: // Stick Right
                        // Scale the axis position and apply it to the stick in the right direction
                        stickX += joystick->GetPosition(j) * 80 / joystick->GetXMax();
                        continue;

                    default:
                        // Trigger a key press or release based on the axis position
                        if (joystick->GetPosition(j) < axisBases[j] - joystick->GetXMax() / 2)
                            pressKey(i);
                        else
                            releaseKey(i);
                        continue;
                }
            }
        }
        else if (ryApp::keyBinds[i] >= 2000 && joystick->GetNumberAxes() > ryApp::keyBinds[i] - 2000) // Axis +
        {
            int j = ryApp::keyBinds[i] - 2000;
            if (joystick->GetPosition(j) > axisBases[j])
            {
                switch (i)
                {
                    case 14: // Stick Up
                        // Scale the axis position and apply it to the stick in the up direction
                        stickY += joystick->GetPosition(j) * 80 / joystick->GetYMin();
                        continue;

                    case 15: // Stick Down
                        // Scale the axis position and apply it to the stick in the down direction
                        stickY -= joystick->GetPosition(j) * 80 / joystick->GetYMax();
                        continue;

                    case 16: // Stick Left
                        // Scale the axis position and apply it to the stick in the left direction
                        stickX -= joystick->GetPosition(j) * 80 / joystick->GetXMin();
                        continue;

                    case 17: // Stick Right
                        // Scale the axis position and apply it to the stick in the right direction
                        stickX += joystick->GetPosition(j) * 80 / joystick->GetXMax();
                        continue;

                    default:
                        // Trigger a key press or release based on the axis position
                        if (joystick->GetPosition(j) > axisBases[j] + joystick->GetXMax() / 2)
                            pressKey(i);
                        else
                            releaseKey(i);
                        continue;
                }
            }
        }
        else if (ryApp::keyBinds[i] >= 1000 && joystick->GetNumberButtons() > ryApp::keyBinds[i] - 1000) // Button
        {
            // Trigger a key press or release based on the button status
            if (joystick->GetButtonState(ryApp::keyBinds[i] - 1000))
                pressKey(i);
            else
                releaseKey(i);
        }
    }

    // Half the coordinates when the stick modifier is applied
    if (stickPressed[4])
    {
        stickX /= 2;
        stickY /= 2;
    }

    // Update the stick coordinates if key-stick is inactive
    if (!(stickPressed[0] | stickPressed[1] | stickPressed[2] | stickPressed[3]))
        PIF::setStick(stickX, stickY);
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
