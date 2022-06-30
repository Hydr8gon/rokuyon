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

#include <wx/wx.h>

class ryApp: public wxApp
{
    private:
        bool OnInit();
};

bool ryApp::OnInit()
{
    // Make a useless window because there's nothing else to show :)
    SetAppName("rokuyon");
    wxFrame *frame = new wxFrame(nullptr, wxID_ANY, "rokuyon");
    frame->Show(true);
    return true;
}

// Let wxWidgets handle the main function
wxIMPLEMENT_APP(ryApp);
