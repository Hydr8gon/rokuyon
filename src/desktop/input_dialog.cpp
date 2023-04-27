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

#include "input_dialog.h"
#include "../settings.h"

enum InputEvent
{
    REMAP_A = 1,
    REMAP_B,
    REMAP_Z,
    REMAP_START,
    REMAP_DUP,
    REMAP_DDOWN,
    REMAP_DLEFT,
    REMAP_DRIGHT,
    REMAP_L,
    REMAP_R,
    REMAP_CUP,
    REMAP_CDOWN,
    REMAP_CLEFT,
    REMAP_CRIGHT,
    REMAP_SUP,
    REMAP_SDOWN,
    REMAP_SLEFT,
    REMAP_SRIGHT,
    REMAP_SMOD,
    CLEAR_MAP,
    UPDATE_JOY
};

wxBEGIN_EVENT_TABLE(InputDialog, wxDialog)
EVT_BUTTON(REMAP_A, InputDialog::remapA)
EVT_BUTTON(REMAP_B, InputDialog::remapB)
EVT_BUTTON(REMAP_Z, InputDialog::remapZ)
EVT_BUTTON(REMAP_START, InputDialog::remapStart)
EVT_BUTTON(REMAP_DUP, InputDialog::remapDUp)
EVT_BUTTON(REMAP_DDOWN, InputDialog::remapDDown)
EVT_BUTTON(REMAP_DLEFT, InputDialog::remapDLeft)
EVT_BUTTON(REMAP_DRIGHT, InputDialog::remapDRight)
EVT_BUTTON(REMAP_L, InputDialog::remapL)
EVT_BUTTON(REMAP_R, InputDialog::remapR)
EVT_BUTTON(REMAP_CUP, InputDialog::remapCUp)
EVT_BUTTON(REMAP_CDOWN, InputDialog::remapCDown)
EVT_BUTTON(REMAP_CLEFT, InputDialog::remapCLeft)
EVT_BUTTON(REMAP_CRIGHT, InputDialog::remapCRight)
EVT_BUTTON(REMAP_SUP, InputDialog::remapSUp)
EVT_BUTTON(REMAP_SDOWN, InputDialog::remapSDown)
EVT_BUTTON(REMAP_SLEFT, InputDialog::remapSLeft)
EVT_BUTTON(REMAP_SRIGHT, InputDialog::remapSRight)
EVT_BUTTON(REMAP_SMOD, InputDialog::remapSMod)
EVT_BUTTON(CLEAR_MAP, InputDialog::clearMap)
EVT_TIMER(UPDATE_JOY, InputDialog::updateJoystick)
EVT_BUTTON(wxID_OK, InputDialog::confirm)
EVT_CHAR_HOOK(InputDialog::pressKey)
wxEND_EVENT_TABLE()

std::string InputDialog::keyToString(int key)
{
    // Handle joystick keys based on the special offsets assigned to them
    if (key >= 3000)
        return "Axis " + std::to_string(key - 3000) + " -";
    else if (key >= 2000)
        return "Axis " + std::to_string(key - 2000) + " +";
    else if (key >= 1000)
        return "Button " + std::to_string(key - 1000);

    // Convert special keys to words representing their respective keys
    switch (key)
    {
        case 0:                    return "None";
        case WXK_BACK:             return "Backspace";
        case WXK_TAB:              return "Tab";
        case WXK_RETURN:           return "Return";
        case WXK_ESCAPE:           return "Escape";
        case WXK_SPACE:            return "Space";
        case WXK_DELETE:           return "Delete";
        case WXK_START:            return "Start";
        case WXK_LBUTTON:          return "Left Button";
        case WXK_RBUTTON:          return "Right Button";
        case WXK_CANCEL:           return "Cancel";
        case WXK_MBUTTON:          return "Middle Button";
        case WXK_CLEAR:            return "Clear";
        case WXK_SHIFT:            return "Shift";
        case WXK_ALT:              return "Alt";
        case WXK_RAW_CONTROL:      return "Control";
        case WXK_MENU:             return "Menu";
        case WXK_PAUSE:            return "Pause";
        case WXK_CAPITAL:          return "Caps Lock";
        case WXK_END:              return "End";
        case WXK_HOME:             return "Home";
        case WXK_LEFT:             return "Left";
        case WXK_UP:               return "Up";
        case WXK_RIGHT:            return "Right";
        case WXK_DOWN:             return "Down";
        case WXK_SELECT:           return "Select";
        case WXK_PRINT:            return "Print";
        case WXK_EXECUTE:          return "Execute";
        case WXK_SNAPSHOT:         return "Snapshot";
        case WXK_INSERT:           return "Insert";
        case WXK_HELP:             return "Help";
        case WXK_NUMPAD0:          return "Numpad 0";
        case WXK_NUMPAD1:          return "Numpad 1";
        case WXK_NUMPAD2:          return "Numpad 2";
        case WXK_NUMPAD3:          return "Numpad 3";
        case WXK_NUMPAD4:          return "Numpad 4";
        case WXK_NUMPAD5:          return "Numpad 5";
        case WXK_NUMPAD6:          return "Numpad 6";
        case WXK_NUMPAD7:          return "Numpad 7";
        case WXK_NUMPAD8:          return "Numpad 8";
        case WXK_NUMPAD9:          return "Numpad 9";
        case WXK_MULTIPLY:         return "Multiply";
        case WXK_ADD:              return "Add";
        case WXK_SEPARATOR:        return "Separator";
        case WXK_SUBTRACT:         return "Subtract";
        case WXK_DECIMAL:          return "Decimal";
        case WXK_DIVIDE:           return "Divide";
        case WXK_F1:               return "F1";
        case WXK_F2:               return "F2";
        case WXK_F3:               return "F3";
        case WXK_F4:               return "F4";
        case WXK_F5:               return "F5";
        case WXK_F6:               return "F6";
        case WXK_F7:               return "F7";
        case WXK_F8:               return "F8";
        case WXK_F9:               return "F9";
        case WXK_F10:              return "F10";
        case WXK_F11:              return "F11";
        case WXK_F12:              return "F12";
        case WXK_F13:              return "F13";
        case WXK_F14:              return "F14";
        case WXK_F15:              return "F15";
        case WXK_F16:              return "F16";
        case WXK_F17:              return "F17";
        case WXK_F18:              return "F18";
        case WXK_F19:              return "F19";
        case WXK_F20:              return "F20";
        case WXK_F21:              return "F21";
        case WXK_F22:              return "F22";
        case WXK_F23:              return "F23";
        case WXK_F24:              return "F24";
        case WXK_NUMLOCK:          return "Numlock";
        case WXK_SCROLL:           return "Scroll";
        case WXK_PAGEUP:           return "Page Up";
        case WXK_PAGEDOWN:         return "Page Down";
        case WXK_NUMPAD_SPACE:     return "Numpad Space";
        case WXK_NUMPAD_TAB:       return "Numpad Tab";
        case WXK_NUMPAD_ENTER:     return "Numpad Enter";
        case WXK_NUMPAD_F1:        return "Numpad F1";
        case WXK_NUMPAD_F2:        return "Numpad F2";
        case WXK_NUMPAD_F3:        return "Numpad F3";
        case WXK_NUMPAD_F4:        return "Numpad F4";
        case WXK_NUMPAD_HOME:      return "Numpad Home";
        case WXK_NUMPAD_LEFT:      return "Numpad Left";
        case WXK_NUMPAD_UP:        return "Numpad Up";
        case WXK_NUMPAD_RIGHT:     return "Numpad Right";
        case WXK_NUMPAD_DOWN:      return "Numpad Down";
        case WXK_NUMPAD_PAGEUP:    return "Numpad Page Up";
        case WXK_NUMPAD_PAGEDOWN:  return "Numpad Page Down";
        case WXK_NUMPAD_END:       return "Numpad End";
        case WXK_NUMPAD_BEGIN:     return "Numpad Begin";
        case WXK_NUMPAD_INSERT:    return "Numpad Insert";
        case WXK_NUMPAD_DELETE:    return "Numpad Delete";
        case WXK_NUMPAD_EQUAL:     return "Numpad Equal";
        case WXK_NUMPAD_MULTIPLY:  return "Numpad Multiply";
        case WXK_NUMPAD_ADD:       return "Numpad Add";
        case WXK_NUMPAD_SEPARATOR: return "Numpad Separator";
        case WXK_NUMPAD_SUBTRACT:  return "Numpad Subtract";
        case WXK_NUMPAD_DECIMAL:   return "Numpad Decimal";
        case WXK_NUMPAD_DIVIDE:    return "Numpad Divide";
    }

    // Directly use the key character for regular keys
    std::string regular;
    regular = (char)key;
    return regular;
}

InputDialog::InputDialog(wxJoystick *joystick):
    wxDialog(nullptr, wxID_ANY, "Input Bindings"), joystick(joystick)
{
    // Get the height of a button in pixels as a reference scale for the rest of the UI
    wxButton *dummy = new wxButton(this, wxID_ANY, "");
    size_t scale = dummy->GetSize().y;
    delete dummy;

    // Load the current key bindings
    memcpy(keyBinds, ryApp::keyBinds, sizeof(keyBinds));

    // Define labels for the bindings
    static const std::string labels[] =
    {
        "A Button", "B Button", "Z Button", "Start Button",
        "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
        "L Button", "R Button",
        "C-Pad Up", "C-Pad Down", "C-Pad Left", "C-Pad Right",
        "Stick Up", "Stick Down", "Stick Left", "Stick Right",
        "Stick Mod"
    };

    // Set up individual buttons for each binding
    wxBoxSizer *keySizers[MAX_KEYS];
    for (int i = 0; i < MAX_KEYS; i++)
    {
        keySizers[i] = new wxBoxSizer(wxHORIZONTAL);
        keySizers[i]->Add(new wxStaticText(this, wxID_ANY, labels[i] + ":"), 1, wxALIGN_CENTRE | wxRIGHT, scale / 16);
        keys[i] = new wxButton(this, REMAP_A + i, keyToString(keyBinds[i]), wxDefaultPosition, wxSize(scale * 4, scale));
        keySizers[i]->Add(keys[i], 0, wxLEFT, scale / 16);
    }

    // Add buttons to the first column of the layout
    wxBoxSizer *column1 = new wxBoxSizer(wxVERTICAL);
    column1->Add(keySizers[14], 1, wxEXPAND | wxALL, scale / 8);
    column1->Add(keySizers[15], 1, wxEXPAND | wxALL, scale / 8);
    column1->Add(keySizers[16], 1, wxEXPAND | wxALL, scale / 8);
    column1->Add(keySizers[17], 1, wxEXPAND | wxALL, scale / 8);
    column1->Add(keySizers[18], 1, wxEXPAND | wxALL, scale / 8);

    // Add buttons to the second column of the layout
    wxBoxSizer *column2 = new wxBoxSizer(wxVERTICAL);
    column2->Add(keySizers[0], 1, wxEXPAND | wxALL, scale / 8);
    column2->Add(keySizers[1], 1, wxEXPAND | wxALL, scale / 8);
    column2->Add(keySizers[2], 1, wxEXPAND | wxALL, scale / 8);
    column2->Add(keySizers[3], 1, wxEXPAND | wxALL, scale / 8);
    column2->Add(keySizers[8], 1, wxEXPAND | wxALL, scale / 8);

    // Add buttons to the third column of the layout
    wxBoxSizer *column3 = new wxBoxSizer(wxVERTICAL);
    column3->Add(keySizers[10], 1, wxEXPAND | wxALL, scale / 8);
    column3->Add(keySizers[11], 1, wxEXPAND | wxALL, scale / 8);
    column3->Add(keySizers[12], 1, wxEXPAND | wxALL, scale / 8);
    column3->Add(keySizers[13], 1, wxEXPAND | wxALL, scale / 8);
    column3->Add(keySizers[9], 1, wxEXPAND | wxALL, scale / 8);

    // Add buttons to the fourth column of the layout
    wxBoxSizer *column4 = new wxBoxSizer(wxVERTICAL);
    column4->Add(keySizers[4], 1, wxEXPAND | wxALL, scale / 8);
    column4->Add(keySizers[5], 1, wxEXPAND | wxALL, scale / 8);
    column4->Add(keySizers[6], 1, wxEXPAND | wxALL, scale / 8);
    column4->Add(keySizers[7], 1, wxEXPAND | wxALL, scale / 8);
    column4->Add(new wxStaticText(this, wxID_ANY, ""), 1, wxEXPAND | wxALL, scale / 8);

    // Combine the button tab contents and add a final border around it
    wxBoxSizer *buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(column1, 1, wxEXPAND | wxALL, scale / 8);
    buttonSizer->Add(column2, 1, wxEXPAND | wxALL, scale / 8);
    buttonSizer->Add(column3, 1, wxEXPAND | wxALL, scale / 8);
    buttonSizer->Add(column4, 1, wxEXPAND | wxALL, scale / 8);

    // Set up the navigation buttons
    wxBoxSizer *naviSizer = new wxBoxSizer(wxHORIZONTAL);
    naviSizer->Add(new wxStaticText(this, wxID_ANY, ""), 1);
    naviSizer->Add(new wxButton(this, CLEAR_MAP,   "Clear"), 0, wxRIGHT, scale / 16);
    naviSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxLEFT | wxRIGHT, scale / 16);
    naviSizer->Add(new wxButton(this, wxID_OK,     "Confirm"), 0, wxLEFT, scale / 16);

    // Populate the dialog
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(buttonSizer, 1, wxEXPAND);
    sizer->Add(naviSizer, 0, wxEXPAND | wxALL, scale / 8);
    SetSizerAndFit(sizer);

    // Lock the window to the default size
    SetMinSize(GetSize());
    SetMaxSize(GetSize());

    // Set up joystick input if a joystick is connected
    if (joystick)
    {
        // Save the initial axis values so inputs can be detected as offsets instead of raw values
        // This avoids issues with axes that have non-zero values in their resting positions
        for (int i = 0; i < joystick->GetNumberAxes(); i++)
            axisBases.push_back(joystick->GetPosition(i));

        // Start a timer to update joystick input, since wxJoystickEvents are unreliable
        timer = new wxTimer(this, UPDATE_JOY);
        timer->Start(10);
    }
}

InputDialog::~InputDialog()
{
    // Clean up the joystick timer
    if (joystick)
        delete timer;
}

void InputDialog::resetLabels()
{
    // Reset the button labels
    for (int i = 0; i < MAX_KEYS; i++)
        keys[i]->SetLabel(keyToString(keyBinds[i]));
    current = nullptr;
}

// Prepare an input binding for remapping
#define REMAP_FUNC(name, index) \
    void InputDialog::name(wxCommandEvent &event) \
    { \
        resetLabels(); \
        keys[index]->SetLabel("Press a key"); \
        current = keys[index]; \
        keyIndex = index; \
    }

REMAP_FUNC(remapA, 0)
REMAP_FUNC(remapB, 1)
REMAP_FUNC(remapZ, 2)
REMAP_FUNC(remapStart, 3)
REMAP_FUNC(remapDUp, 4)
REMAP_FUNC(remapDDown, 5)
REMAP_FUNC(remapDLeft, 6)
REMAP_FUNC(remapDRight, 7)
REMAP_FUNC(remapL, 8)
REMAP_FUNC(remapR, 9)
REMAP_FUNC(remapCUp, 10)
REMAP_FUNC(remapCDown, 11)
REMAP_FUNC(remapCLeft, 12)
REMAP_FUNC(remapCRight, 13)
REMAP_FUNC(remapSUp, 14)
REMAP_FUNC(remapSDown, 15)
REMAP_FUNC(remapSLeft, 16)
REMAP_FUNC(remapSRight, 17)
REMAP_FUNC(remapSMod, 18)

void InputDialog::clearMap(wxCommandEvent &event)
{
    if (current)
    {
        // If a button is selected, clear only its mapping
        keyBinds[keyIndex] = 0;
        current->SetLabel(keyToString(keyBinds[keyIndex]));
        current = nullptr;
    }
    else
    {
        // If no button is selected, clear all mappings
        for (int i = 0; i < MAX_KEYS; i++)
            keyBinds[i] = 0;
        resetLabels();
    }
}

void InputDialog::updateJoystick(wxTimerEvent &event)
{
    if (!current) return;

    // Map the current button to a joystick button if one is pressed
    for (int i = 0; i < joystick->GetNumberButtons(); i++)
    {
        if (joystick->GetButtonState(i))
        {
            keyBinds[keyIndex] = 1000 + i;
            current->SetLabel(keyToString(keyBinds[keyIndex]));
            current = nullptr;
            return;
        }
    }

    // Map the current button to a joystick axis if one is pushed far enough
    for (int i = 0; i < joystick->GetNumberAxes(); i++)
    {
        if (joystick->GetPosition(i) - axisBases[i] > joystick->GetXMax() / 2) // Positive axis
        {
            keyBinds[keyIndex] = 2000 + i;
            current->SetLabel(keyToString(keyBinds[keyIndex]));
            current = nullptr;
            return;
        }
        else if (joystick->GetPosition(i) - axisBases[i] < joystick->GetXMin() / 2) // Negative axis
        {
            keyBinds[keyIndex] = 3000 + i;
            current->SetLabel(keyToString(keyBinds[keyIndex]));
            current = nullptr;
            return;
        }
    }
}

void InputDialog::confirm(wxCommandEvent &event)
{
    // Update and save the key bindings
    memcpy(ryApp::keyBinds, keyBinds, sizeof(keyBinds));
    Settings::save();
    event.Skip(true);
}

void InputDialog::pressKey(wxKeyEvent &event)
{
    // Map the selected button to the pressed key
    if (current)
    {
        keyBinds[keyIndex] = event.GetKeyCode();
        current->SetLabel(keyToString(keyBinds[keyIndex]));
        current = nullptr;
    }
}
