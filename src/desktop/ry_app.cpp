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

#include <wx/filename.h>
#include <wx/stdpaths.h>

#include "ry_app.h"
#include "../ai.h"
#include "../core.h"
#include "../settings.h"

enum AppEvent
{
    UPDATE = 1
};

wxBEGIN_EVENT_TABLE(ryApp, wxApp)
EVT_TIMER(UPDATE, ryApp::update)
wxEND_EVENT_TABLE()

int ryApp::keyBinds[] =
{
    'L', 'K', 'J', 'G', // A, B, Z, Start
    WXK_UP, WXK_DOWN, WXK_LEFT, WXK_RIGHT, // D-pad
    'Q', 'P', // L, R
    '8', 'I', 'U', 'O', // C-buttons
    'W', 'S', 'A', 'D', WXK_SHIFT, // Joystick
    WXK_ESCAPE // Full screen
};

bool ryApp::OnInit()
{
    // Define the input binding setting names
    static const char *names[MAX_KEYS] =
    {
        "keyA", "keyB", "keyZ", "keyStart",
        "keyDUp", "keyDDown", "keyDLeft", "keyDRight",
        "keyL", "keyR",
        "keyCUp", "keyCDown", "keyCLeft", "keyCRight",
        "keySUp", "keySDown", "keySLeft", "keySRight",
        "keySMod", "keyFullScreen"
    };

    // Register the input binding settings
    for (int i = 0; i < MAX_KEYS; i++)
        Settings::add(names[i], &keyBinds[i], false);

    // Try to load settings from the current directory first
    if (!Settings::load())
    {
        // Get the system-specific application settings directory
        std::string settingsDir;
        wxStandardPaths &paths = wxStandardPaths::Get();
#if defined(WINDOWS) || defined(MACOS) || !wxCHECK_VERSION(3, 1, 0)
        settingsDir = paths.GetUserDataDir().mb_str(wxConvUTF8);
#else
        paths.SetFileLayout(wxStandardPaths::FileLayout_XDG);
        settingsDir = paths.GetUserConfigDir().mb_str(wxConvUTF8);
        settingsDir += "/rokuyon";
#endif

        // Try to load settings from the system directory, creating it if it doesn't exist
        if (!Settings::load(settingsDir + "/rokuyon.ini"))
        {
            wxFileName dir = wxFileName::DirName(settingsDir);
            if (!dir.DirExists()) dir.Mkdir();
            Settings::save();
        }
    }

    // Create the app's frame, passing along a filename from the command line
    SetAppName("rokuyon");
    frame = new ryFrame((argc > 1) ? argv[1].ToStdString() : "");

    // Set up the update timer
    timer = new wxTimer(this, UPDATE);
    timer->Start(6);

    // Set up the audio stream
    Pa_Initialize();
    Pa_OpenDefaultStream(&stream, 0, 2, paInt16, 48000, 1024, audioCallback, nullptr);
    Pa_StartStream(stream);

    return true;
}

int ryApp::OnExit()
{
    // Stop some things before exiting
    Pa_StopStream(stream);
    timer->Stop();
    return wxApp::OnExit();
}

void ryApp::update(wxTimerEvent &event)
{
    // Continuously refresh the frame
    frame->Refresh();
}

int ryApp::audioCallback(const void *in, void *out, unsigned long count,
    const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data)
{
    // Get samples from the audio interface
    AI::fillBuffer((uint32_t*)out);
    return paContinue;
}
