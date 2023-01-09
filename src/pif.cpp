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
#include "core.h"
#include "cpu.h"
#include "log.h"
#include "memory.h"

namespace PIF
{
    uint8_t memory[0x800]; // 2KB-64B PIF ROM + 64B PIF RAM
    uint16_t eepromMask;
    uint8_t eepromId;

    uint8_t command;
    uint16_t buttons;
    int8_t stickX;
    int8_t stickY;

    extern void (*pifCommands[])(int);

    uint32_t crc32(uint8_t *data, size_t size);
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

uint32_t PIF::crc32(uint8_t *data, size_t size)
{
    uint32_t r = 0xFFFFFFFF;

    // Calculate a CRC32 value for the given data
    for (size_t i = 0; i < size; i++)
    {
        r ^= data[i];
        for (int j = 0; j < 8; j++)
        {
            uint32_t t = ~((r & 1) - 1);
            r = (r >> 1) ^ (0xEDB88320 & t);
        }
    }

    return ~r;
}

void PIF::reset()
{
    // Reset the PIF to its initial state
    clearMemory(0);
    command = 0;
    buttons = 0;
    stickX = 0;
    stickY = 0;

    // Set a mask and ID for 0.5KB/2KB EEPROM, or disable EEPROM
    switch (Core::saveSize)
    {
        case 0x200: eepromMask = 0x1FF; eepromId = 0x80; break;
        case 0x800: eepromMask = 0x7FF; eepromId = 0xC0; break;
        default:    eepromMask = 0x000; eepromId = 0x00; break;
    }

    // Set the CIC seed based on which bootcode is detected
    // This value is used during boot to calculate a checksum
    switch (uint32_t value = crc32(&Core::rom[0x40], 0x1000 - 0x40))
    {
        case 0x6170A4A1: // 6101
            LOG_INFO("Detected CIC chip 6101\n");
            Memory::write<uint8_t>(0xBFC007E6, 0x3F);
            break;

        case 0x90BB6CB5: // 6102
            LOG_INFO("Detected CIC chip 6102\n");
            Memory::write<uint8_t>(0xBFC007E6, 0x3F);
            break;

        case 0x0B050EE0: // 6103
            LOG_INFO("Detected CIC chip 6103\n");
            Memory::write<uint8_t>(0xBFC007E6, 0x78);
            break;

        case 0x98BC2C86: // 6105
            LOG_INFO("Detected CIC chip 6105\n");
            Memory::write<uint8_t>(0xBFC007E6, 0x91);
            break;

        case 0xACC8580A: // 6106
            LOG_INFO("Detected CIC chip 6106\n");
            Memory::write<uint8_t>(0xBFC007E6, 0x85);
            break;

        default:
            LOG_WARN("Unknown IPL3 CRC32 value: 0x%08X\n", value);
            break;
    }

    if (FILE *pifFile = fopen("pif_rom.bin", "rb"))
    {
        // Load the PIF ROM into memory if it exists
        fread(memory, sizeof(uint8_t), 0x7C0, pifFile);
        fclose(pifFile);
    }
    else
    {
        // Set CPU registers as if the PIF ROM was executed
        // Values from https://github.com/mikeryan/n64dev/blob/master/src/boot/pif.S
        *CPU::registersW[1]  = 0x0000000000000000;
        *CPU::registersW[2]  = 0xFFFFFFFFD1731BE9;
        *CPU::registersW[3]  = 0xFFFFFFFFD1731BE9;
        *CPU::registersW[4]  = 0x0000000000001BE9;
        *CPU::registersW[5]  = 0xFFFFFFFFF45231E5;
        *CPU::registersW[6]  = 0xFFFFFFFFA4001F0C;
        *CPU::registersW[7]  = 0xFFFFFFFFA4001F08;
        *CPU::registersW[8]  = 0x00000000000000C0;
        *CPU::registersW[9]  = 0x0000000000000000;
        *CPU::registersW[10] = 0x0000000000000040;
        *CPU::registersW[11] = 0xFFFFFFFFA4000040;
        *CPU::registersW[12] = 0xFFFFFFFFD1330BC3;
        *CPU::registersW[13] = 0xFFFFFFFFD1330BC3;
        *CPU::registersW[14] = 0x0000000025613A26;
        *CPU::registersW[15] = 0x000000002EA04317;
        *CPU::registersW[16] = 0x0000000000000000;
        *CPU::registersW[17] = 0x0000000000000000;
        *CPU::registersW[18] = 0x0000000000000000;
        *CPU::registersW[19] = 0x0000000000000000;
        *CPU::registersW[20] = 0x0000000000000001;
        *CPU::registersW[21] = 0x0000000000000000;
        *CPU::registersW[22] = Memory::read<uint8_t>(0xBFC007E6);
        *CPU::registersW[23] = 0x0000000000000006;
        *CPU::registersW[24] = 0x0000000000000000;
        *CPU::registersW[25] = 0xFFFFFFFFD73F2993;
        *CPU::registersW[26] = 0x0000000000000000;
        *CPU::registersW[27] = 0x0000000000000000;
        *CPU::registersW[28] = 0x0000000000000000;
        *CPU::registersW[29] = 0xFFFFFFFFA4001FF0;
        *CPU::registersW[30] = 0x0000000000000000;
        *CPU::registersW[31] = 0xFFFFFFFFA4001554;

        // Copy the IPL3 from ROM to DMEM and jump to the start address
        for (uint32_t i = 0; i < 0x1000; i++)
            Memory::write(0xA4000000 + i, Core::rom[i]);
        CPU::programCounter = 0xA4000040 - 4;
    }

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

void PIF::setStick(int x, int y)
{
    // Update the stick position
    stickX = x;
    stickY = y;
}

void PIF::joybusProtocol(int bit)
{
    uint8_t channel = 0;

    // Loop through PIF RAM and process joybus commands
    for (uint16_t i = 0x7C0; i < 0x7FF; i++)
    {
        int8_t txSize = memory[i];

        if (txSize > 0)
        {
            uint8_t rxSize = memory[i + 1];

            switch (uint8_t cmd = memory[i + 2])
            {
                case 0xFF: // Reset
                case 0x00: // Info
                    if (channel < 4)
                    {
                        // Report a standard controller with no pak
                        memory[i + 3] = 0x05; // ID high
                        memory[i + 4] = 0x00; // ID low
                        memory[i + 5] = 0x02; // Status
                    }
                    else if (channel < 6)
                    {
                        // Report the current EEPROM type, if any
                        memory[i + 3] = 0x00; // ID high
                        memory[i + 4] = eepromId; // ID low
                        memory[i + 5] = 0x00; // Status
                    }
                    else
                    {
                        // Report nothing
                        memory[i + 3] = 0x00; // ID high
                        memory[i + 4] = 0x00; // ID low
                        memory[i + 5] = 0x00; // Status
                    }
                    break;

                case 0x01: // Controller state
                    // Report the state of controller 1 if the channel is 0
                    memory[i + 3] = channel ? 0 : (buttons >> 8);
                    memory[i + 4] = channel ? 0 : (buttons >> 0);
                    memory[i + 5] = channel ? 0 : stickX;
                    memory[i + 6] = channel ? 0 : stickY;
                    break;

                case 0x04: // Read EEPROM block
                    if ((channel & ~1) == 4) // Channels 4 and 5
                    {
                        // Read 8 bytes from an EEPROM page to PIF memory
                        uint16_t address = (memory[i + 3] * 8) & eepromMask;
                        for (int j = 0; j < 8; j++)
                            memory[i + 4 + j] = eepromMask ? Core::save[address + j] : 0;
                    }
                    break;

                case 0x05: // Write EEPROM block
                    if ((channel & ~1) == 4 && eepromMask) // Channels 4 and 5
                    {
                        // Write 8 bytes from PIF memory to an EEPROM page
                        uint16_t address = (memory[i + 3] * 8) & eepromMask;
                        for (int j = 0; j < 8; j++)
                            Core::writeSave(address + j, memory[i + 4 + j]);
                    }
                    break;

                default:
                    LOG_WARN("Unknown joybus command: 0x%02X\n", cmd);
                    break;
            }

            // Skip the transferred bytes and move to the next channel
            i += txSize + rxSize + 1;
            channel++;
        }
        else if (txSize == 0)
        {
            // Move to the next channel without transferring anything
            channel++;
        }
        else if (txSize == (int8_t)0xFE)
        {
            // Finish executing commands early
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
