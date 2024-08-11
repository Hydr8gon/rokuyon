/*
    Copyright 2022-2024 Hydr8gon

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

#ifndef SAVE_DIALOG_H
#define SAVE_DIALOG_H

#include <cstdint>
#include <wx/wx.h>

class SaveDialog: public wxDialog
{
    public:
        SaveDialog(std::string &lastPath);

    private:
        std::string &lastPath;
        uint32_t selection = 0;

        uint32_t selectToSize(uint32_t select);
        uint32_t sizeToSelect(uint32_t size);

        void select0(wxCommandEvent &event);
        void select1(wxCommandEvent &event);
        void select2(wxCommandEvent &event);
        void select3(wxCommandEvent &event);
        void select4(wxCommandEvent &event);
        void confirm(wxCommandEvent &event);

        wxDECLARE_EVENT_TABLE();
};

#endif // SAVE_DIALOG_H
