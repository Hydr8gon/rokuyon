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

#include <cstring>

#include "pif.h"
#include "log.h"
#include "memory.h"

namespace PIF
{
    uint8_t memory[0x800]; // 2KB-64B PIF ROM + 64B PIF RAM
    uint8_t command;
    uint16_t buttons;

    extern void (*pifCommands[])(int);

    void joybusProtocol(int bit);
    void verifyChecksum(int bit);
    void clearMemory(int bit);
    void unknownCmd(int bit);
}

// Small command lookup table for PIF command bits
void (*PIF::pifCommands[7])(int) =
{
    joybusProtocol, unknownCmd,     unknownCmd, unknownCmd, // 0-3
    unknownCmd,     verifyChecksum, clearMemory             // 4-6
};

void PIF::reset(FILE *pifFile)
{
    // Load the PIF ROM into memory
    fread(memory, sizeof(uint8_t), 0x7C0, pifFile);
    fclose(pifFile);

    // Reset the PIF to its initial state
    clearMemory(0);
    command = 0;
    buttons = 0;

    // Set the CIC value used for checksums during boot
    // TODO: check ROM bootcode and set appropriately
    Memory::write<uint32_t>(0xBFC007E4, 0x00043F3F);

    // Set the memory size to 4MB
    // TODO: I think IPL3 is supposed to set this, but stubbing RI_SELECT_REG to 1 skips it
    Memory::write<uint32_t>(0xA0000318, 0x00400000);
}

void PIF::runCommand()
{
    // Update the current command if new command bits were set
    if (uint8_t value = memory[0x7FF] & 0x7F)
        command = value;

    // Execute commands for any set bits, and clear the bits
    for (int i = 0; i < 7; i++)
    {
        if (command & (1 << i))
        {
            (*pifCommands[i])(i);
            memory[0x7FF] &= ~(1 << i);
        }
    }
}

void PIF::pressKey(int key)
{
    // Mark a button as pressed
    if (key < 16)
        buttons |= (1 << (15 - key));
}

void PIF::releaseKey(int key)
{
    // Mark a button as released
    if (key < 16)
        buttons &= ~(1 << (15 - key));
}

void PIF::joybusProtocol(int bit)
{
    // Handle joybus commands in PIF RAM for 4 controllers
    for (int i = 0; i < 4 * 8; i += 8)
    {
        switch (uint8_t cmd = memory[0x7C3 + i])
        {
            case 0xFF: // Reset
            case 0x00: // Info
                // Report a standard controller with no pak
                memory[0x7C4 + i] = 0x05; // ID high
                memory[0x7C5 + i] = 0x00; // ID low
                memory[0x7C6 + i] = 0x02; // Status
                break;

            case 0x01: // Controller state
                // Report which buttons are pressed for controller 1
                memory[0x7C4 + i] = (i == 0) ? (buttons >> 8) : 0;
                memory[0x7C5 + i] = (i == 0) ? (buttons >> 0) : 0;
                memory[0x7C6 + i] = 0; // Stick X
                memory[0x7C7 + i] = 0; // Stick Y
                break;

            default:
                LOG_WARN("Unknown controller %d command: 0x%02X\n", i / 8, cmd);
                return;
        }
    }
}

void PIF::verifyChecksum(int bit)
{
    // On hardware, this command verifies if a checksum matches one given by the CIC
    // It doesn't matter for emulation, so just set the result bit
    memory[0x7FF] |= 0x80;
}

void PIF::clearMemory(int bit)
{
    // Clear the 64 bytes of PIF RAM
    memset(&memory[0x7C0], 0, 0x40);
}

void PIF::unknownCmd(int bit)
{
    // Warn about unknown commands
    LOG_WARN("Unknown PIF command bit: %d\n", bit);
}
