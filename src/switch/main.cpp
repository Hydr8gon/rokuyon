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

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <malloc.h>
#include <switch.h>
#include <thread>

#include "switch_ui.h"
#include "../ai.h"
#include "../core.h"
#include "../pif.h"
#include "../settings.h"
#include "../vi.h"

AudioOutBuffer audioBuffers[2];
AudioOutBuffer *audioReleasedBuffer;
int16_t *audioData[2];
uint32_t count;

std::string path;
std::thread *audioThread;
bool showFps;

const uint32_t keyMap[] =
{
    (HidNpadButton_A | HidNpadButton_B), (HidNpadButton_X | HidNpadButton_Y), // A, B
    (HidNpadButton_ZL | HidNpadButton_ZR), HidNpadButton_Plus, // Z, Start
    HidNpadButton_Up, HidNpadButton_Down, HidNpadButton_Left, HidNpadButton_Right, // D-pad
    0, 0, HidNpadButton_L, HidNpadButton_R, // L, R
    HidNpadButton_StickRUp, HidNpadButton_StickRDown, // C-up, C-down
    HidNpadButton_StickRLeft, HidNpadButton_StickRRight, // C-left, C-right
    (HidNpadButton_StickL | HidNpadButton_StickR), HidNpadButton_Minus // FPS, Pause
};

void outputAudio()
{
    while (Core::running)
    {
        // Load audio samples from the core when a buffer is empty
        audoutWaitPlayFinish(&audioReleasedBuffer, &count, UINT64_MAX);
        AI::fillBuffer((uint32_t*)audioReleasedBuffer->buffer);
        audoutAppendAudioOutBuffer(audioReleasedBuffer);
    }
}

bool startCore(bool reset)
{
    if (!audioThread)
    {
        // Try to boot a ROM at the current path, but display an error if failed
        if (reset && !Core::bootRom(path))
        {
            std::vector<std::string> message = { "Make sure the ROM file is accessible and try again." };
            SwitchUI::message("Error Loading ROM", message);
            return false;
        }

        // Start the emulator core
        Core::start();
        audioThread = new std::thread(outputAudio);
    }

    return true;
}

void stopCore()
{
    if (audioThread)
    {
        // Stop the emulator core
        Core::stop();
        audioThread->join();
        delete audioThread;
        audioThread = nullptr;
    }
}

void settingsMenu()
{
    const std::vector<std::string> toggle = { "Off", "On" };
    size_t index = 0;

    while (true)
    {
        // Make a list of settings and current values
        std::vector<ListItem> settings =
        {
            ListItem("FPS Limiter", toggle[Settings::fpsLimiter]),
            ListItem("Threaded RDP", toggle[Settings::threadedRdp]),
            ListItem("Texture Filter", toggle[Settings::texFilter])
        };

        // Create the settings menu
        Selection menu = SwitchUI::menu("Settings", &settings, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & HidNpadButton_A)
        {
            // Change the chosen setting to its next value
            switch (index)
            {
                case 0: Settings::fpsLimiter = !Settings::fpsLimiter; break;
                case 1: Settings::threadedRdp = !Settings::threadedRdp; break;
                case 2: Settings::texFilter = !Settings::texFilter; break;
            }
        }
        else
        {
            // Close the settings menu
            Settings::save();
            return;
        }
    }
}

void fileBrowser()
{
    size_t index = 0;
    path = "sdmc:/";

    // Load the appropriate icons for the current theme
    uint32_t *file   = SwitchUI::bmpToTexture(SwitchUI::isDarkTheme() ? "romfs:/file-dark.bmp"   : "romfs:/file-light.bmp");
    uint32_t *folder = SwitchUI::bmpToTexture(SwitchUI::isDarkTheme() ? "romfs:/folder-dark.bmp" : "romfs:/folder-light.bmp");

    while (true)
    {
        std::vector<ListItem> files;
        DIR *dir = opendir(path.c_str());
        dirent *entry;

        // Add all folders and ROMs at the current path to a list with icons
        while ((entry = readdir(dir)))
        {
            std::string name = entry->d_name;
            if (entry->d_type == DT_DIR)
                files.push_back(ListItem(name, "", folder, 64));
            else if (name.find(".z64", name.length() - 4) != std::string::npos)
                files.push_back(ListItem(name, "", file, 64));
        }

        closedir(dir);
        sort(files.begin(), files.end());

        // Create the file browser menu
        Selection menu = SwitchUI::menu("rokuyon", &files, index, "Settings", "Exit");
        index = menu.index;

        // Handle menu input
        if (menu.pressed & HidNpadButton_A)
        {
            if (!files.empty())
            {
                // Navigate to the selected path
                path += "/" + files[menu.index].name;
                index = 0;

                if (files[menu.index].icon == file)
                {
                    // Close the browser If a ROM is loaded successfully
                    if (startCore(true))
                        break;

                    // Remove the ROM from the path and continue browsing
                    path = path.substr(0, path.rfind("/"));
                }
            }
        }
        else if (menu.pressed & HidNpadButton_B)
        {
            if (path != "sdmc:/")
            {
                // Navigate to the previous directory
                path = path.substr(0, path.rfind("/"));
                index = 0;
            }
        }
        else if (menu.pressed & HidNpadButton_X)
        {
            // Open the settings menu
            settingsMenu();
        }
        else
        {
            // Close the file browser
            break;
        }
    }

    // Free the theme icons
    delete[] file;
    delete[] folder;
}

bool saveTypeMenu()
{
    size_t index = 0;
    std::vector<ListItem> items =
    {
        ListItem("None"),
        ListItem("EEPROM 0.5KB"),
        ListItem("EEPROM 2KB"),
        ListItem("SRAM 32KB"),
        ListItem("FLASH 128KB")
    };

    // Select the current save type by default
    switch (Core::saveSize)
    {
        case 0x00200: index = 1; break; // EEPROM 0.5KB
        case 0x00800: index = 2; break; // EEPROM 8KB
        case 0x08000: index = 3; break; // SRAM 32KB
        case 0x20000: index = 4; break; // FLASH 128KB
    }

    // Create the save type menu
    Selection menu = SwitchUI::menu("Change Save Type", &items, index);
    index = menu.index;

    // Handle menu input
    if (menu.pressed & HidNpadButton_A)
    {
        // Ask for confirmation before doing anything because accidents could be bad!
        std::vector<std::string> message = { "Are you sure? This may result in data loss!" };
        if (!SwitchUI::message("Changing Save Type", message, true))
            return false;

        // On confirmation, change the save type
        switch (index)
        {
            case 0: Core::resizeSave(0x00000); break; // None
            case 1: Core::resizeSave(0x00200); break; // EEPROM 0.5KB
            case 2: Core::resizeSave(0x00800); break; // EEPROM 8KB
            case 3: Core::resizeSave(0x08000); break; // SRAM 32KB
            case 4: Core::resizeSave(0x20000); break; // FLASH 128KB
        }

        // Restart the emulator
        Core::bootRom(path);
        return true;
    }

    return false;
}

void pauseMenu()
{
    size_t index = 0;
    std::vector<ListItem> items =
    {
        ListItem("Resume"),
        ListItem("Restart"),
        ListItem("Change Save Type"),
        ListItem("Settings"),
        ListItem("File Browser")
    };

    // Pause the emulator
    stopCore();

    while (true)
    {
        // Create the pause menu
        Selection menu = SwitchUI::menu("rokuyon", &items, index);
        index = menu.index;

        // Handle menu input
        if (menu.pressed & HidNpadButton_A)
        {
            switch (index)
            {
                case 0: // Resume
                    // Return to the emulator
                    startCore(false);
                    return;

                case 2: // Change Save Type
                    // Open the save type menu and restart if the save changed
                    if (!saveTypeMenu())
                        break;

                case 1: // Restart
                    // Restart and return to the emulator
                    if (!startCore(true))
                        fileBrowser();
                    return;

                case 3: // Settings
                    // Open the settings menu
                    settingsMenu();
                    break;

                case 4: // File Browser
                    // Open the file browser
                    fileBrowser();
                    return;
            }
        }
        else if (menu.pressed & HidNpadButton_B)
        {
            // Return to the emulator
            startCore(false);
            return;
        }
        else
        {
            // Close the pause menu
            return;
        }
    }
}

int main()
{
    // Initialize the UI and lock exiting until cleanup
    appletLockExit();
    SwitchUI::initialize();

    // Load settings or create them if they don't exist
    if (!Settings::load())
        Settings::save();

    // Initialize audio output
    audoutInitialize();
    audoutStartAudioOut();

    // Initialize the audio buffers
    for (int i = 0; i < 2; i++)
    {
        size_t size = 1024 * 2 * sizeof(int16_t);
        audioData[i] = (int16_t*)memalign(0x1000, size);
        memset(audioData[i], 0, size);
        audioBuffers[i].next = nullptr;
        audioBuffers[i].buffer = audioData[i];
        audioBuffers[i].buffer_size = size;
        audioBuffers[i].data_size = size;
        audioBuffers[i].data_offset = 0;
        audoutAppendAudioOutBuffer(&audioBuffers[i]);
    }

    // Overclock the Switch CPU
    clkrstInitialize();
    ClkrstSession cpuSession;
    clkrstOpenSession(&cpuSession, PcvModuleId_CpuBus, 0);
    clkrstSetClockRate(&cpuSession, 1785000000);

    // Open the file browser
    fileBrowser();

    while (appletMainLoop() && Core::running)
    {
        // Maintain the CPU overclock if it was reset from ex. leaving the app
        uint32_t rate;
        clkrstGetClockRate(&cpuSession, &rate);
        if (rate != 1785000000)
            clkrstSetClockRate(&cpuSession, 1785000000);

        // Scan for controller input
        padUpdate(SwitchUI::getPad());
        uint32_t pressed = padGetButtonsDown(SwitchUI::getPad());
        uint32_t released = padGetButtonsUp(SwitchUI::getPad());
        HidAnalogStickState stick = padGetStickPos(SwitchUI::getPad(), 0);

        // Send key input to the core
        for (int i = 0; i < 16; i++)
        {
            if (pressed & keyMap[i])
                PIF::pressKey(i);
            else if (released & keyMap[i])
                PIF::releaseKey(i);
        }

        // Send joystick input to the core
        PIF::setStick(stick.x >> 8, stick.y >> 8);

        // Draw a new frame if one is ready
        if (_Framebuffer *fb = VI::getFramebuffer())
        {
            SwitchUI::clear(Color(0, 0, 0));
            SwitchUI::drawImage(fb->data, fb->width, fb->height, 160, 0, 960, 720, true, 0);
            if (showFps) SwitchUI::drawString(std::to_string(Core::fps) + " FPS", 5, 0, 48, Color(255, 255, 255));
            SwitchUI::update();
            delete fb;
        }

        // Toggle showing FPS or open the pause menu if hotkeys are pressed
        if (pressed & keyMap[16])
            showFps = !showFps;
        else if (pressed & keyMap[17])
            pauseMenu();
    }

    // Ensure the core is stopped
    stopCore();

    // Disable the CPU overclock
    clkrstSetClockRate(&cpuSession, 1020000000);
    clkrstExit();

    // Stop audio output
    audoutStopAudioOut();
    audoutExit();

    // Free the audio buffers
    delete[] audioData[0];
    delete[] audioData[1];

    // Clean up the UI and unlock exiting
    SwitchUI::deinitialize();
    appletUnlockExit();
    return 0;
}
