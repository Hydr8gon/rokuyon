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

#include "cpu_cp0.h"
#include "cpu.h"
#include "cpu_cp1.h"
#include "log.h"
#include "memory.h"
#include "mi.h"

namespace CPU_CP0
{
    uint32_t _index;
    uint32_t entryLo0;
    uint32_t entryLo1;
    uint32_t pageMask;
    uint32_t count;
    uint32_t entryHi;
    uint32_t compare;
    uint32_t status;
    uint32_t cause;
    uint32_t epc;
    uint32_t errorEpc;

    void tlbwi(uint32_t opcode);
    void eret(uint32_t opcode);
    void unk(uint32_t opcode);
}

// CP0 instruction lookup table, using opcode bits 0-5
void (*CPU_CP0::cp0Instrs[0x40])(uint32_t) =
{
    unk,  unk, tlbwi, unk, unk, unk, unk, unk, // 0x00-0x07
    unk,  unk, unk,   unk, unk, unk, unk, unk, // 0x08-0x0F
    unk,  unk, unk,   unk, unk, unk, unk, unk, // 0x10-0x17
    eret, unk, unk,   unk, unk, unk, unk, unk, // 0x18-0x1F
    unk,  unk, unk,   unk, unk, unk, unk, unk, // 0x20-0x27
    unk,  unk, unk,   unk, unk, unk, unk, unk, // 0x28-0x2F
    unk,  unk, unk,   unk, unk, unk, unk, unk, // 0x30-0x37
    unk,  unk, unk,   unk, unk, unk, unk, unk  // 0x38-0x3F
};

void CPU_CP0::reset()
{
    // Reset the CPU CP0 to its initial state
    _index = 0;
    entryLo0 = 0;
    entryLo1 = 0;
    pageMask = 0;
    count = 0;
    entryHi = 0;
    compare = 0;
    status = 0x400004;
    cause = 0;
    epc = 0;
    errorEpc = 0;
}

uint32_t CPU_CP0::read(int index)
{
    // Read from a CPU CP0 register if one exists at the given index
    switch (index)
    {
        case 0: // Index
            // Get the index register
            return _index;

        case 2: // EntryLo0
            // Get the low entry 0 register
            return entryLo0;

        case 3: // EntryLo1
            // Get the low entry 1 register
            return entryLo1;

        case 5: // PageMask
            // Get the page mask register
            return pageMask;

        case 9: // Count
            // Get the count register
            return count;

        case 10: // EntryHi
            // Get the high entry register
            return entryHi;

        case 11: // Compare
            // Get the compare register
            return compare;

        case 12: // Status
            // Get the status register
            return status;

        case 13: // Cause
            // Get the cause register
            return cause;

        case 14: // EPC
            // Get the exception program counter
            return epc;

        case 30: // ErrorEPC
            // Get the error exception program counter
            return errorEpc;

        default:
            LOG_WARN("Read from unknown CPU CP0 register: %d\n", index);
            return 0;
    }
}

void CPU_CP0::write(int index, uint32_t value)
{
    // Write to a CPU CP0 register if one exists at the given index
    switch (index)
    {
        case 0: // Index
            // Set the index register
            _index = value & 0x3F;
            return;

        case 2: // EntryLo0
            // Set the low entry 0 register
            entryLo0 = value & 0x3FFFFFF;
            return;

        case 3: // EntryLo1
            // Set the low entry 1 register
            entryLo1 = value & 0x3FFFFFF;
            return;

        case 5: // PageMask
            // Set the page mask register
            pageMask = value & 0x1FFE000;
            return;

        case 9: // Count
            // Set the count register
            count = value;
            return;

        case 10: // EntryHi
            // Set the high entry register
            entryHi = value & 0xFFFFE0FF;
            return;

        case 11: // Compare
            // Set the compare register and acknowledge a timer interrupt
            compare = value;
            cause &= ~0x8000;
            return;

        case 12: // Status
            // Set the status register and apply the FR bit to the CP1
            status = value & 0xFF57FFFF;
            checkInterrupts();
            CPU_CP1::setRegMode(status & (1 << 26));

            // Keep track of unimplemented bits that should do something
            if (uint32_t bits = (value & 0xB0000E0))
                LOG_WARN("Unimplemented CPU CP0 status bits set: 0x%X\n", bits);
            return;

        case 13: // Cause
            // Set the software interrupt flags
            cause = (cause & ~0x300) | (value & 0x300);
            checkInterrupts();
            return;

        case 14: // EPC
            // Set the exception program counter
            epc = value;
            return;

        case 30: // ErrorEPC
            // Set the error exception program counter
            errorEpc = value;
            return;

        default:
            LOG_WARN("Write to unknown CPU CP0 register: %d\n", index);
            return;
    }
}

void CPU_CP0::updateCount()
{
    // Increment count and request a timer interrupt when it matches compare
    if (++count == compare)
    {
        cause |= 0x8000;
        checkInterrupts();
    }
}

void CPU_CP0::checkInterrupts()
{
    // Set the external interrupt bit if any MI interrupt is set
    cause = (cause & ~0x400) | ((bool)(MI::interrupt & MI::mask) << 10);

    // Trigger an interrupt if able and an enabled bit is set
    if (((status & 0x3) == 0x1) && (status & cause & 0xFF00))
        exception(0);
}

void CPU_CP0::exception(uint8_t type)
{
    // If an exception occurs at a delay slot, execute that first
    // TODO: handle delay slot exceptions properly
    if (CPU::nextOpcode != Memory::read<uint32_t>(CPU::programCounter))
        CPU::runOpcode();

    // Update registers for an exception and jump to the handler
    status |= 0x2; // EXL
    cause = (cause & ~0x7C) | ((type << 2) & 0x7C);
    epc = CPU::programCounter - (type ? 4 : 0);
    CPU::programCounter = ((status & (1 << 22)) ? 0xBFC00200 : 0x80000000) + 0x180 - 4;
    CPU::nextOpcode = 0;
}

bool CPU_CP0::cpUsable(uint8_t cp)
{
    // Check if a coprocessor is usable (CP0 is always usable in kernel mode)
    if (!(status & (1 << (28 + cp))) && (cp > 0 || (!(status & 0x6) && (status & 0x18))))
    {
        // Set the coprocessor number bits
        cause = (cause & ~(0x3 << 28)) | ((cp & 0x3) << 28);
        return false;
    }

    return true;
}

void CPU_CP0::tlbwi(uint32_t opcode)
{
    // Set the TLB entry at the current index
    Memory::setEntry(_index, entryLo0, entryLo1, entryHi, pageMask);
}

void CPU_CP0::eret(uint32_t opcode)
{
    // Return from an error exception or exception and clear the ERL or EXL bit
    CPU::programCounter = CPU_CP0::read((status & 0x4) ? 30 : 14) - 4;
    CPU::nextOpcode = 0;
    status &= ~((status & 0x4) ? 0x4 : 0x2);
}

void CPU_CP0::unk(uint32_t opcode)
{
    // Warn about unknown instructions
    LOG_CRIT("Unknown CP0 opcode: 0x%08X @ 0x%X\n", opcode, CPU::programCounter - 4);
}
