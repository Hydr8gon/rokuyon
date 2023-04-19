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

#include "cpu_cp0.h"
#include "core.h"
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
    uint32_t context;
    uint32_t pageMask;
    uint32_t badVAddr;
    uint32_t count;
    uint32_t entryHi;
    uint32_t compare;
    uint32_t status;
    uint32_t cause;
    uint32_t epc;
    uint32_t errorEpc;

    bool irqPending;
    uint32_t startCycles;
    uint32_t endCycles;

    void scheduleCount();
    void updateCount();
    void interrupt();

    void tlbr(uint32_t opcode);
    void tlbwi(uint32_t opcode);
    void tlbp(uint32_t opcode);
    void eret(uint32_t opcode);
    void unk(uint32_t opcode);
}

// CP0 instruction lookup table, using opcode bits 0-5
void (*CPU_CP0::cp0Instrs[0x40])(uint32_t) =
{
    unk,  tlbr, tlbwi, unk, unk, unk, unk, unk, // 0x00-0x07
    tlbp, unk,  unk,   unk, unk, unk, unk, unk, // 0x08-0x0F
    unk,  unk,  unk,   unk, unk, unk, unk, unk, // 0x10-0x17
    eret, unk,  unk,   unk, unk, unk, unk, unk, // 0x18-0x1F
    unk,  unk,  unk,   unk, unk, unk, unk, unk, // 0x20-0x27
    unk,  unk,  unk,   unk, unk, unk, unk, unk, // 0x28-0x2F
    unk,  unk,  unk,   unk, unk, unk, unk, unk, // 0x30-0x37
    unk,  unk,  unk,   unk, unk, unk, unk, unk  // 0x38-0x3F
};

void CPU_CP0::reset()
{
    // Reset the CPU CP0 to its initial state
    _index = 0;
    entryLo0 = 0;
    entryLo1 = 0;
    context = 0;
    pageMask = 0;
    badVAddr = 0;
    count = 0;
    entryHi = 0;
    compare = 0;
    status = 0x400004;
    cause = 0;
    epc = 0;
    errorEpc = 0;
    irqPending = false;
    endCycles = -1;
    scheduleCount();
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

        case 4: // Context
            // Get the context register
            return context;

        case 5: // PageMask
            // Get the page mask register
            return pageMask;

        case 8: // BadVAddr
            // Get the bad virtual address register
            return badVAddr;

        case 9: // Count
            // Get the count register, as it would be at the current cycle
            return count + ((Core::globalCycles - startCycles) >> 2);

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

        case 4: // Context
            // Set the context register
            context = value & 0xFFFFFFF0;
            return;

        case 5: // PageMask
            // Set the page mask register
            pageMask = value & 0x1FFE000;
            return;

        case 9: // Count
            // Set the count register and reschedule its next update
            count = value;
            scheduleCount();
            return;

        case 10: // EntryHi
            // Set the high entry register
            entryHi = value & 0xFFFFE0FF;
            return;

        case 11: // Compare
            // Set the compare register and acknowledge a timer interrupt
            compare = value;
            cause &= ~0x8000;
            
            // Update the count register and reschedule its next update
            count += ((Core::globalCycles - startCycles) >> 2);
            scheduleCount();
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

void CPU_CP0::resetCycles()
{
    // Adjust the cycle counts for a cycle reset
    startCycles -= Core::globalCycles;
    endCycles -= Core::globalCycles;
}

void CPU_CP0::scheduleCount()
{
    // Assuming count is updated, schedule its next update
    // This is done as close to match as possible, with a limit to prevent cycle overflow
    startCycles = Core::globalCycles;
    uint32_t cycles = startCycles + std::min<uint32_t>((compare - count) << 2, 0x40000000);
    cycles += (startCycles == cycles) << 2;

    // Only reschedule if the update is sooner than what's already scheduled
    // This helps prevent overloading the scheduler when registers are used excessively
    if (endCycles > cycles)
    {
        Core::schedule(updateCount, cycles - startCycles);
        endCycles = cycles;
    }
}

void CPU_CP0::updateCount()
{
    // Ignore the update if it was rescheduled
    if (Core::globalCycles != endCycles)
        return;

    // Update count and request a timer interrupt if it matches compare
    if ((count += ((endCycles - startCycles) >> 2)) == compare)
    {
        cause |= 0x8000;
        checkInterrupts();
    }

    // Schedule the next update unconditionally
    endCycles = -1;
    scheduleCount();
}

void CPU_CP0::checkInterrupts()
{
    // Set the external interrupt bit if any MI interrupt is set
    cause = (cause & ~0x400) | ((bool)(MI::interrupt & MI::mask) << 10);

    // Schedule an interrupt if able and an enabled bit is set
    if (((status & 0x3) == 0x1) && (status & cause & 0xFF00) && !irqPending)
    {
        Core::schedule(interrupt, 2); // 1 CPU cycle
        irqPending = true;
    }
}

void CPU_CP0::interrupt()
{
    // Trigger an interrupt that has been scheduled
    CPU_CP0::exception(0);
    irqPending = false;
}

void CPU_CP0::exception(uint8_t type)
{
    // Update registers for an exception and jump to the handler
    // TODO: handle nested exceptions
    status |= 0x2; // EXL
    cause = (cause & ~0x8000007C) | ((type << 2) & 0x7C);
    epc = CPU::programCounter - (type ? 4 : 0);
    CPU::programCounter = ((status & (1 << 22)) ? 0xBFC00200 : 0x80000000) - 4;
    CPU::nextOpcode = 0;

    // Adjust the exception vector based on the type
    if ((type & ~1) != 2) // Not TLB miss
        CPU::programCounter += 0x180;

    // Return to the preceding branch if the exception occured in a delay slot
    if (CPU::delaySlot != -1)
    {
        epc = CPU::delaySlot - 4;
        cause |= (1 << 31); // BD
    }

    // Unhalt the CPU if it was idling
    Core::cpuRunning = true;
}

void CPU_CP0::setTlbAddress(uint32_t address)
{
    // Set the address that caused a TLB exception
    badVAddr = address;
    entryHi = address & 0xFFFFE000;
    context = (context & ~0x7FFFF0) | ((address >> 9) & 0x7FFFF0);
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

void CPU_CP0::tlbr(uint32_t opcode)
{
    // Get the TLB entry at the current index
    Memory::getEntry(_index, entryLo0, entryLo1, entryHi, pageMask);
}

void CPU_CP0::tlbwi(uint32_t opcode)
{
    // Set the TLB entry at the current index
    Memory::setEntry(_index, entryLo0, entryLo1, entryHi, pageMask);
}

void CPU_CP0::tlbp(uint32_t opcode)
{
    // Search the TLB entries for one that matches the current high register
    for (int i = 0; i < 32; i++)
    {
        // Get a TLB entry
        uint32_t _entryLo0, _entryLo1, _entryHi, _pageMask;
        Memory::getEntry(i, _entryLo0, _entryLo1, _entryHi, _pageMask);

        // Set the index to the TLB entry if it matches
        if (entryHi == _entryHi)
        {
            _index = i;
            return;
        }
    }

    // Set the index high bit if no match was found
    _index = (1 << 31);
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
