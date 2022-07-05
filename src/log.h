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

#ifndef LOG_H
#define LOG_H

#include <cstdio>

// If enabled, print critical logs in red
#if LOG_LEVEL > 0
    #define LOG_CRIT(...) printf("\x1b[31m" __VA_ARGS__)
#else
    #define LOG_CRIT(...) (0)
#endif

// If enabled, print warning logs in yellow
#if LOG_LEVEL > 1
    #define LOG_WARN(...) printf("\x1b[33m" __VA_ARGS__)
#else
    #define LOG_WARN(...) (0)
#endif

// If enabled, print info logs normally
#if LOG_LEVEL > 2
    #define LOG_INFO(...) printf("\x1b[0m" __VA_ARGS__)
#else
    #define LOG_INFO(...) (0)
#endif

#endif // LOG_H
