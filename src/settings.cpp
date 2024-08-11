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

#include <vector>

#include "settings.h"

struct Setting
{
    Setting(std::string name, void *value, bool isString):
        name(name), value(value), isString(isString) {}

    std::string name;
    void *value;
    bool isString;
};

namespace Settings
{
    std::string filename;
    int fpsLimiter = 1;
    int expansionPak = 1;
    int threadedRdp = 0;
    int texFilter = 1;

    std::vector<Setting> settings =
    {
        Setting("fpsLimiter", &fpsLimiter, false),
        Setting("expansionPak", &expansionPak, false),
        Setting("threadedRdp", &threadedRdp, false),
        Setting("texFilter", &texFilter, false)
    };
}

void Settings::add(std::string name, void *value, bool isString)
{
    // Add an additional platform setting to be loaded from the settings file
    settings.push_back(Setting(name, value, isString));
}

bool Settings::load(std::string filename)
{
    // Attempt to open the settings file; otherwise default values will be used
    Settings::filename = filename;
    FILE *file = fopen(filename.c_str(), "r");
    if (!file) return false;

    char data[1024];

    // Read each line in the settings file and load values from them
    while (fgets(data, 1024, file) != nullptr)
    {
        std::string line = data;
        int split = line.find("=");
        std::string name = line.substr(0, split);

        for (size_t i = 0; i < settings.size(); i++)
        {
            if (name == settings[i].name)
            {
                std::string value = line.substr(split + 1, line.size() - split - 2);
                if (settings[i].isString)
                    *(std::string*)settings[i].value = value;
                else if (value[0] >= '0' && value[0] <= '9')
                    *(int*)settings[i].value = stoi(value);
                break;
            }
        }
    }

    fclose(file);
    return true;
}

bool Settings::save()
{
    // Attempt to open the settings file
    FILE *file = fopen(filename.c_str(), "w");
    if (!file) return false;

    // Write each value to a line in the settings file
    for (size_t i = 0; i < settings.size(); i++)
    {
        std::string value = settings[i].isString ?
            *(std::string*)settings[i].value : std::to_string(*(int*)settings[i].value);
        fprintf(file, "%s=%s\n", settings[i].name.c_str(), value.c_str());
    }

    fclose(file);
    return true;
}
