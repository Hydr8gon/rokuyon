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

#ifndef RY_FRAME_H
#define RY_FRAME_H

#include <wx/wx.h>

class ryCanvas;

class ryFrame: public wxFrame
{
    public:
        ryFrame(std::string path);

        void Refresh();
        bool isPaused() { return paused; }

    private:
        ryCanvas *canvas;
        wxMenu *fileMenu;
        wxMenu *systemMenu;

        std::string lastPath;
        bool paused;

        void bootRom(std::string path);
        void updateMenu();

        void loadRom(wxCommandEvent &event);
        void changeSave(wxCommandEvent &event);
        void quit(wxCommandEvent &event);
        void pause(wxCommandEvent &event);
        void restart(wxCommandEvent &event);
        void stop(wxCommandEvent &event);
        void dropFiles(wxDropFilesEvent &event);
        void close(wxCloseEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // RY_FRAME_H
