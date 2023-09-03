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

#ifndef INPUT_DIALOG_H
#define INPUT_DIALOG_H

#include "ry_app.h"

class InputDialog: public wxDialog
{
    public:
        InputDialog(wxJoystick *joystick);
        ~InputDialog();

    private:
        wxJoystick *joystick;
        wxTimer *timer;
        wxButton *keys[MAX_KEYS];

        int keyBinds[MAX_KEYS];
        std::vector<int> axisBases;
        wxButton *current = nullptr;
        int keyIndex = 0;

        std::string keyToString(int key);
        void resetLabels();

        void remapA(wxCommandEvent &event);
        void remapB(wxCommandEvent &event);
        void remapZ(wxCommandEvent &event);
        void remapStart(wxCommandEvent &event);
        void remapDUp(wxCommandEvent &event);
        void remapDDown(wxCommandEvent &event);
        void remapDLeft(wxCommandEvent &event);
        void remapDRight(wxCommandEvent &event);
        void remapL(wxCommandEvent &event);
        void remapR(wxCommandEvent &event);
        void remapCUp(wxCommandEvent &event);
        void remapCDown(wxCommandEvent &event);
        void remapCLeft(wxCommandEvent &event);
        void remapCRight(wxCommandEvent &event);
        void remapSUp(wxCommandEvent &event);
        void remapSDown(wxCommandEvent &event);
        void remapSLeft(wxCommandEvent &event);
        void remapSRight(wxCommandEvent &event);
        void remapSMod(wxCommandEvent &event);
        void remapFullScreen(wxCommandEvent &event);

        void clearMap(wxCommandEvent &event);
        void updateJoystick(wxTimerEvent &event);
        void confirm(wxCommandEvent &event);
        void pressKey(wxKeyEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // INPUT_DIALOG_H
