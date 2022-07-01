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

enum AppEvent
{
    UPDATE = 1
};

wxBEGIN_EVENT_TABLE(ryApp, wxApp)
EVT_TIMER(UPDATE, ryApp::update)
wxEND_EVENT_TABLE()

bool ryApp::OnInit()
{
    // Create and show the app's frame
    SetAppName("rokuyon");
    frame = new ryFrame();

    // Set up the update timer
    timer = new wxTimer(this, UPDATE);
    timer->Start(6);

    return true;
}

int ryApp::OnExit()
{
    timer->Stop();
    return wxApp::OnExit();
}

void ryApp::update(wxTimerEvent &event)
{
    // Continuously refresh the frame
    frame->Refresh();
}
