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

#include "ry_app.h"
#include "../core.h"
#include "../ai.h"

enum AppEvent
{
    UPDATE = 1
};

wxBEGIN_EVENT_TABLE(ryApp, wxApp)
EVT_TIMER(UPDATE, ryApp::update)
wxEND_EVENT_TABLE()

bool ryApp::OnInit()
{
    // Create the app's frame, passing along a filename from the command line
    SetAppName("rokuyon");
    frame = new ryFrame((argc > 1) ? argv[1].ToStdString() : "");

    // Set up the update timer
    timer = new wxTimer(this, UPDATE);
    timer->Start(6);

    // Set up the audio stream
    // TBD: I was unable to mate the PortAudio nuget package with the build.
#ifndef _MSC_VER
    Pa_Initialize();
    Pa_OpenDefaultStream(&stream, 0, 2, paInt16, 48000, 1024, audioCallback, nullptr);
    Pa_StartStream(stream);
#endif

    return true;
}

int ryApp::OnExit()
{
    // Stop some things before exiting
    // TBD: I was unable to mate the PortAudio nuget package with the build.
#ifndef _MSC_VER
    Pa_StopStream(stream);
#endif
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
