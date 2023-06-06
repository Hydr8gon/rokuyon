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

#include "save_dialog.h"
#include "../core.h"

enum SaveEvent
{
    SELECT_0 = 1,
    SELECT_1,
    SELECT_2,
    SELECT_3,
    SELECT_4
};

wxBEGIN_EVENT_TABLE(SaveDialog, wxDialog)
EVT_RADIOBUTTON(SELECT_0, SaveDialog::select0)
EVT_RADIOBUTTON(SELECT_1, SaveDialog::select1)
EVT_RADIOBUTTON(SELECT_2, SaveDialog::select2)
EVT_RADIOBUTTON(SELECT_3, SaveDialog::select3)
EVT_RADIOBUTTON(SELECT_4, SaveDialog::select4)
EVT_BUTTON(wxID_OK, SaveDialog::confirm)
wxEND_EVENT_TABLE()

uint32_t SaveDialog::selectToSize(uint32_t select)
{
    // Convert a save selection to a size
    switch (select)
    {
        case 1:  return 0x00200; // EEPROM 0.5KB
        case 2:  return 0x00800; // EEPROM 8KB
        case 3:  return 0x08000; // SRAM 32KB
        case 4:  return 0x20000; // FLASH 128KB
        default: return 0x00000; // None
    }
}

uint32_t SaveDialog::sizeToSelect(uint32_t size)
{
    // Convert a save size to a selection
    switch (size)
    {
        case 0x00200: return 1; // EEPROM 0.5KB
        case 0x00800: return 2; // EEPROM 8KB
        case 0x08000: return 3; // SRAM 32KB
        case 0x20000: return 4; // FLASH 128KB
        default:      return 0; // None
    }
}

SaveDialog::SaveDialog(std::string &lastPath): lastPath(lastPath),
    wxDialog(nullptr, wxID_ANY, "Change Save Type")
{
    // Get the height of a button in pixels as a reference scale for the rest of the UI
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    size_t scale = dummy->GetSize().y;
    delete dummy;

    // Create left and right columns for the radio buttons
    wxBoxSizer *leftRadio = new wxBoxSizer(wxVERTICAL);
    wxBoxSizer *rightRadio = new wxBoxSizer(wxVERTICAL);
    wxRadioButton *buttons[5];

    // Set up radio buttons for the save types
    leftRadio->Add(buttons[0] = new wxRadioButton(this, SELECT_0, "None"), 1);
    leftRadio->Add(buttons[1] = new wxRadioButton(this, SELECT_1, "EEPROM 0.5KB"), 1);
    leftRadio->Add(buttons[2] = new wxRadioButton(this, SELECT_2, "EEPROM 2KB"), 1);
    rightRadio->Add(buttons[3] = new wxRadioButton(this, SELECT_3, "SRAM 32KB"), 1);
    rightRadio->Add(buttons[4] = new wxRadioButton(this, SELECT_4, "FLASH 128KB"), 1);
    rightRadio->Add(new wxStaticText(this, wxID_ANY, ""), 1);

    // Select the current save type by default
    selection = sizeToSelect(Core::saveSize);
    buttons[selection]->SetValue(true);

    // Combine all of the radio buttons
    wxBoxSizer *radioSizer = new wxBoxSizer(wxHORIZONTAL);
    radioSizer->Add(leftRadio, 1, wxEXPAND | wxRIGHT, scale / 8);
    radioSizer->Add(rightRadio, 1, wxEXPAND | wxLEFT, scale / 8);

    // Set up the cancel and confirm buttons
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    buttonSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxRIGHT, scale / 16);
    buttonSizer->Add(new wxButton(this, wxID_OK, "Confirm"), 0, wxLEFT, scale / 16);

    // Combine all of the contents
    wxBoxSizer *contents = new wxBoxSizer(wxVERTICAL);
    contents->Add(radioSizer, 1, wxEXPAND | wxALL, scale / 8);
    contents->Add(buttonSizer, 0, wxEXPAND | wxALL, scale / 8);

    // Add a final border around everything
    wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(contents, 1, wxEXPAND | wxALL, scale / 8);
    SetSizer(sizer);

    // Size the window to fit the contents and prevent resizing
    sizer->Fit(this);
    SetMinSize(GetSize());
    SetMaxSize(GetSize());
}

void SaveDialog::select0(wxCommandEvent &event)
{
    // Select save type 0
    selection = 0;
}

void SaveDialog::select1(wxCommandEvent &event)
{
    // Select save type 1
    selection = 1;
}

void SaveDialog::select2(wxCommandEvent &event)
{
    // Select save type 2
    selection = 2;
}

void SaveDialog::select3(wxCommandEvent &event)
{
    // Select save type 3
    selection = 3;
}

void SaveDialog::select4(wxCommandEvent &event)
{
    // Select save type 4
    selection = 4;
}

void SaveDialog::confirm(wxCommandEvent &event)
{
    // Ask for confirmation before doing anything because accidents could be bad!
    wxMessageDialog dialog(this, "Are you sure? This may result in data loss!",
        "Changing Save Type", wxYES_NO | wxICON_NONE);

    // On confirmation, change the save type and restart the emulator
    if (dialog.ShowModal() == wxID_YES)
    {
        Core::stop();
        Core::resizeSave(selectToSize(selection));
        Core::bootRom(lastPath);
        event.Skip(true);
    }
}
