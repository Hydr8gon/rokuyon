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

#ifndef CORE_H
#define CORE_H

#include <cstdint>
#include <string>

namespace Core
{
    extern bool running;
    extern bool cpuRunning;
    extern bool rspRunning;
    extern uint32_t globalCycles;
    extern int fps;

    extern uint8_t *rom;
    extern uint8_t *save;
    extern uint32_t romSize;
    extern uint32_t saveSize;

    bool bootRom(const std::string &path);
    void resizeSave(uint32_t newSize);
    void start();
    void stop();

    void countFrame();
    void writeSave(uint32_t address, uint8_t value);
    void schedule(void (*function)(), uint32_t cycles);
}

#endif // CORE_H
