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

#ifndef RY_APP_H
#define RY_APP_H

#include <portaudio.h>

#include "ry_frame.h"

#define MAX_KEYS 20

class ryApp: public wxApp
{
    public:
        static int keyBinds[MAX_KEYS];

    private:
        ryFrame *frame;
        wxTimer *timer;
        PaStream *stream;

        bool OnInit();
        int  OnExit();

        void update(wxTimerEvent &event);

        static int audioCallback(const void *in, void *out, unsigned long count,
            const PaStreamCallbackTimeInfo *info, PaStreamCallbackFlags flags, void *data);

        wxDECLARE_EVENT_TABLE();
};

#endif // RY_APP_H
