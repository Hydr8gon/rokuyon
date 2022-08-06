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

#include "cpu.h"
#include "cpu_cp0.h"
#include "cpu_cp1.h"
#include "log.h"
#include "memory.h"

namespace CPU
{
    uint64_t registersR[33];
    uint64_t *registersW[32];
    uint64_t hi, lo;
    uint32_t programCounter;
    uint32_t nextOpcode;
    bool countToggle;

    extern void (*immInstrs[])(uint32_t);
    extern void (*regInstrs[])(uint32_t);
    extern void (*extInstrs[])(uint32_t);

    void j(uint32_t opcode);
    void jal(uint32_t opcode);
    void beq(uint32_t opcode);
    void bne(uint32_t opcode);
    void blez(uint32_t opcode);
    void bgtz(uint32_t opcode);
    void addi(uint32_t opcode);
    void addiu(uint32_t opcode);
    void slti(uint32_t opcode);
    void sltiu(uint32_t opcode);
    void andi(uint32_t opcode);
    void ori(uint32_t opcode);
    void xori(uint32_t opcode);
    void lui(uint32_t opcode);
    void beql(uint32_t opcode);
    void bnel(uint32_t opcode);
    void blezl(uint32_t opcode);
    void bgtzl(uint32_t opcode);
    void daddi(uint32_t opcode);
    void daddiu(uint32_t opcode);
    void ldl(uint32_t opcode);
    void ldr(uint32_t opcode);
    void lb(uint32_t opcode);
    void lh(uint32_t opcode);
    void lwl(uint32_t opcode);
    void lw(uint32_t opcode);
    void lbu(uint32_t opcode);
    void lhu(uint32_t opcode);
    void lwr(uint32_t opcode);
    void lwu(uint32_t opcode);
    void sb(uint32_t opcode);
    void sh(uint32_t opcode);
    void swl(uint32_t opcode);
    void sw(uint32_t opcode);
    void sdl(uint32_t opcode);
    void sdr(uint32_t opcode);
    void swr(uint32_t opcode);
    void cache(uint32_t opcode);
    void lwc1(uint32_t opcode);
    void ldc1(uint32_t opcode);
    void ld(uint32_t opcode);
    void swc1(uint32_t opcode);
    void sdc1(uint32_t opcode);
    void sd(uint32_t opcode);

    void sll(uint32_t opcode);
    void srl(uint32_t opcode);
    void sra(uint32_t opcode);
    void sllv(uint32_t opcode);
    void srlv(uint32_t opcode);
    void srav(uint32_t opcode);
    void jr(uint32_t opcode);
    void jalr(uint32_t opcode);
    void syscall(uint32_t opcode);
    void break_(uint32_t opcode);
    void mfhi(uint32_t opcode);
    void mthi(uint32_t opcode);
    void mflo(uint32_t opcode);
    void mtlo(uint32_t opcode);
    void dsllv(uint32_t opcode);
    void dsrlv(uint32_t opcode);
    void dsrav(uint32_t opcode);
    void mult(uint32_t opcode);
    void multu(uint32_t opcode);
    void div(uint32_t opcode);
    void divu(uint32_t opcode);
    void dmult(uint32_t opcode);
    void dmultu(uint32_t opcode);
    void ddiv(uint32_t opcode);
    void ddivu(uint32_t opcode);
    void add(uint32_t opcode);
    void addu(uint32_t opcode);
    void sub(uint32_t opcode);
    void subu(uint32_t opcode);
    void and_(uint32_t opcode);
    void or_(uint32_t opcode);
    void xor_(uint32_t opcode);
    void nor(uint32_t opcode);
    void slt(uint32_t opcode);
    void sltu(uint32_t opcode);
    void dadd(uint32_t opcode);
    void daddu(uint32_t opcode);
    void dsub(uint32_t opcode);
    void dsubu(uint32_t opcode);
    void dsll(uint32_t opcode);
    void dsrl(uint32_t opcode);
    void dsra(uint32_t opcode);
    void dsll32(uint32_t opcode);
    void dsrl32(uint32_t opcode);
    void dsra32(uint32_t opcode);

    void bltz(uint32_t opcode);
    void bgez(uint32_t opcode);
    void bltzl(uint32_t opcode);
    void bgezl(uint32_t opcode);
    void bltzal(uint32_t opcode);
    void bgezal(uint32_t opcode);
    void bltzall(uint32_t opcode);
    void bgezall(uint32_t opcode);

    void eret(uint32_t opcode);
    void mfc0(uint32_t opcode);
    void mtc0(uint32_t opcode);
    void mfc1(uint32_t opcode);
    void dmfc1(uint32_t opcode);
    void cfc1(uint32_t opcode);
    void mtc1(uint32_t opcode);
    void dmtc1(uint32_t opcode);
    void ctc1(uint32_t opcode);
    void bc1f(uint32_t opcode);
    void bc1t(uint32_t opcode);
    void bc1fl(uint32_t opcode);
    void bc1tl(uint32_t opcode);

    void cop0(uint32_t opcode);
    void cop1(uint32_t opcode);
    void unk(uint32_t opcode);
}

// Immediate-type CPU instruction lookup table, using opcode bits 26-31
void (*CPU::immInstrs[0x40])(uint32_t) =
{
    nullptr, nullptr, j,    jal,   beq,  bne,  blez,  bgtz,  // 0x00-0x07
    addi,    addiu,   slti, sltiu, andi, ori,  xori,  lui,   // 0x08-0x0F
    cop0,    cop1,    unk,  unk,   beql, bnel, blezl, bgtzl, // 0x10-0x17
    daddi,   daddiu,  ldl,  ldr,   unk,  unk,  unk,   unk,   // 0x18-0x1F
    lb,      lh,      lwl,  lw,    lbu,  lhu,  lwr,   lwu,   // 0x20-0x27
    sb,      sh,      swl,  sw,    sdl,  sdr,  swr,   cache, // 0x28-0x2F
    unk,     lwc1,    unk,  unk,   unk,  ldc1, unk,   ld,    // 0x30-0x37
    unk,     swc1,    unk,  unk,   unk,  sdc1, unk,   sd     // 0x38-0x3F
};

// Register-type CPU instruction lookup table, using opcode bits 0-5
void (*CPU::regInstrs[0x40])(uint32_t) =
{
    sll,  unk,   srl,  sra,  sllv,    unk,    srlv,   srav,  // 0x00-0x07
    jr,   jalr,  unk,  unk,  syscall, break_, unk,    unk,   // 0x08-0x0F
    mfhi, mthi,  mflo, mtlo, dsllv,   unk,    dsrlv,  dsrav, // 0x10-0x17
    mult, multu, div,  divu, dmult,   dmultu, ddiv,   ddivu, // 0x18-0x1F
    add,  addu,  sub,  subu, and_,    or_,    xor_,   nor,   // 0x20-0x27
    unk,  unk,   slt,  sltu, dadd,    daddu,  dsub,   dsubu, // 0x28-0x2F
    unk,  unk,   unk,  unk,  unk,     unk,    unk,    unk,   // 0x30-0x37
    dsll, unk,   dsrl, dsra, dsll32,  unk,    dsrl32, dsra32 // 0x38-0x3F
};

// Extra-type CPU instruction lookup table, using opcode bits 16-20
void (*CPU::extInstrs[0x20])(uint32_t) =
{
    bltz,   bgez,   bltzl,   bgezl,   unk, unk, unk, unk, // 0x00-0x07
    unk,    unk,    unk,     unk,     unk, unk, unk, unk, // 0x08-0x0F
    bltzal, bgezal, bltzall, bgezall, unk, unk, unk, unk, // 0x10-0x17
    unk,    unk,    unk,     unk,     unk, unk, unk, unk  // 0x18-0x1F
};

void CPU::reset()
{
    // Map the writable registers so that writes to r0 are redirected
    registersW[0] = &registersR[32];
    for (int i = 1; i < 32; i++)
        registersW[i] = &registersR[i];

    // Reset the CPU to its initial state
    memset(registersR, 0, sizeof(registersR));
    hi = lo = 0;
    programCounter = 0xBFC00000;
    nextOpcode = Memory::read<uint32_t>(programCounter);
    countToggle = false;
}

void CPU::runOpcode()
{
    // Update the CP0 count register at half the speed of the CPU
    if (countToggle = !countToggle)
        CPU_CP0::updateCount();

    // Move an opcode through the pipeline
    uint32_t opcode = nextOpcode;
    nextOpcode = Memory::read<uint32_t>(programCounter += 4);

    // Look up and execute an instruction
    switch (opcode >> 26)
    {
        default: return (*immInstrs[opcode >> 26])(opcode);
        case 0:  return (*regInstrs[opcode & 0x3F])(opcode);
        case 1:  return (*extInstrs[(opcode >> 16) & 0x1F])(opcode);
    }
}

void CPU::exception(uint8_t type)
{
    // If an exception happens at a delay slot, execute that first
    // TODO: handle delay slot exceptions properly
    if (nextOpcode != Memory::read<uint32_t>(programCounter))
        runOpcode();

    // Trigger an exception and jump to the handler
    programCounter = CPU_CP0::exception(programCounter, type) + 0x180 - 4;
    nextOpcode = 0;
}

void CPU::j(uint32_t opcode)
{
    // Jump to an immediate value
    programCounter = ((programCounter & 0xF0000000) | ((opcode & 0x3FFFFFF) << 2)) - 4;
}

void CPU::jal(uint32_t opcode)
{
    // Save the return address and jump to an immediate value
    *registersW[31] = programCounter + 4;
    programCounter = ((programCounter & 0xF0000000) | ((opcode & 0x3FFFFFF) << 2)) - 4;
}

void CPU::beq(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers are equal
    if (registersR[(opcode >> 21) & 0x1F] == registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::bne(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers aren't equal
    if (registersR[(opcode >> 21) & 0x1F] != registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::blez(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less or equal to zero
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] <= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::bgtz(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater than zero
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] > 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::addi(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the lower result
    // On overflow, trigger an integer overflow exception
    int32_t op1 = registersR[(opcode >> 21) & 0x1F];
    int32_t op2 = (int16_t)opcode;
    int32_t value = op1 + op2;
    if (!((op1 ^ op2) & (1 << 31)) && ((op1 ^ value) & (1 << 31)))
        return exception(12);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::addiu(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the lower result
    int32_t value = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::slti(uint32_t opcode)
{
    // Check if a signed register is less than a signed 16-bit immediate, and store the result
    bool value = (int64_t)registersR[(opcode >> 21) & 0x1F] < (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::sltiu(uint32_t opcode)
{
    // Check if a register is less than a signed 16-bit immediate, and store the result
    bool value = registersR[(opcode >> 21) & 0x1F] < (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::andi(uint32_t opcode)
{
    // Bitwise and a register with a 16-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] & (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::ori(uint32_t opcode)
{
    // Bitwise or a register with a 16-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] | (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::xori(uint32_t opcode)
{
    // Bitwise exclusive or a register with a 16-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] ^ (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::lui(uint32_t opcode)
{
    // Load a 16-bit immediate into the upper 16 bits of a register
    *registersW[(opcode >> 16) & 0x1F] = (int16_t)opcode << 16;
}

void CPU::beql(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers are equal
    // Otherwise, discard the delay slot opcode
    if (registersR[(opcode >> 21) & 0x1F] == registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::bnel(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers aren't equal
    // Otherwise, discard the delay slot opcode
    if (registersR[(opcode >> 21) & 0x1F] != registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::blezl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less or equal to zero
    // Otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] <= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::bgtzl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater than zero
    // Otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] > 0)
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::daddi(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the result
    // On overflow, trigger an integer overflow exception
    int64_t op1 = registersR[(opcode >> 21) & 0x1F];
    int64_t op2 = (int16_t)opcode;
    int64_t value = op1 + op2;
    if (!((op1 ^ op2) & (1LL << 63)) && ((op1 ^ value) & (1LL << 63)))
        return exception(12);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::daddiu(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void CPU::ldl(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the most significant byte of a misaligned 64-bit word
    // The aligned 64-bit word is shifted left so its bytes misalign to match the address
    // The shifted value is read to a register, but empty bytes are left unchanged
    // This allows LDL and LDR to be used in succession to read misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint64_t value = Memory::read<uint64_t>(address) << ((address & 7) * 8);
    uint64_t *reg = registersW[(opcode >> 16) & 0x1F];
    *reg = (*reg & ~((uint64_t)-1 << ((address & 7) * 8))) | value;
}

void CPU::ldr(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the most significant byte of a misaligned 64-bit word
    // The aligned 64-bit word is shifted right so its bytes misalign to match the address
    // The shifted value is read to a register, but empty bytes are left unchanged
    // This allows LDL and LDR to be used in succession to read misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint64_t value = Memory::read<uint64_t>(address) >> ((7 - (address & 7)) * 8);
    uint64_t *reg = registersW[(opcode >> 16) & 0x1F];
    *reg = (*reg & ~((uint64_t)-1 >> ((7 - (address & 7)) * 8))) | value;
}

void CPU::lb(uint32_t opcode)
{
    // Load a signed byte from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = (int8_t)Memory::read<uint8_t>(address);
}

void CPU::lh(uint32_t opcode)
{
    // Load a signed half-word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = (int16_t)Memory::read<uint16_t>(address);
}

void CPU::lwl(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the most significant byte of a misaligned 32-bit word
    // The aligned 32-bit word is shifted left so its bytes misalign to match the address
    // The shifted value is read to a register, but empty bytes are left unchanged
    // This allows LWL and LWR to be used in succession to read misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint32_t value = Memory::read<uint32_t>(address) << ((address & 3) * 8);
    uint64_t *reg = registersW[(opcode >> 16) & 0x1F];
    *reg = (int32_t)(((uint32_t)*reg & ~((uint32_t)-1 << ((address & 7) * 8))) | value);
}

void CPU::lw(uint32_t opcode)
{
    // Load a signed word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = (int32_t)Memory::read<uint32_t>(address);
}

void CPU::lbu(uint32_t opcode)
{
    // Load a byte from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint8_t>(address);
}

void CPU::lhu(uint32_t opcode)
{
    // Load a half-word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint16_t>(address);
}

void CPU::lwr(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the most significant byte of a misaligned 32-bit word
    // The aligned 32-bit word is shifted right so its bytes misalign to match the address
    // The shifted value is read to a register, but empty bytes are left unchanged
    // This allows LWL and LWR to be used in succession to read misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint32_t value = Memory::read<uint32_t>(address) >> ((3 - (address & 3)) * 8);
    uint64_t *reg = registersW[(opcode >> 16) & 0x1F];
    *reg = (int32_t)(((uint32_t)*reg & ~((uint32_t)-1 >> ((3 - (address & 3)) * 8))) | value);
}

void CPU::lwu(uint32_t opcode)
{
    // Load a word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint32_t>(address);
}

void CPU::sb(uint32_t opcode)
{
    // Store a byte to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    Memory::write<uint8_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::sh(uint32_t opcode)
{
    // Store a half-word to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    Memory::write<uint16_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::swl(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the most significant byte of a misaligned 32-bit word
    // A register value is shifted right so its bytes misalign to match the address
    // The shifted value is written to the aligned memory address, but empty bytes are left unchanged
    // This allows SWL and SWR to be used in succession to write misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint32_t rVal = (uint32_t)registersR[(opcode >> 16) & 0x1F] >> ((address & 3) * 8);
    uint32_t mVal = Memory::read<uint32_t>(address) & ~((uint32_t)-1 >> ((address & 3) * 8));
    Memory::write<uint32_t>(address, rVal | mVal);
}

void CPU::sw(uint32_t opcode)
{
    // Store a word to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    Memory::write<uint32_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::sdl(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the most significant byte of a misaligned 64-bit word
    // A register value is shifted right so its bytes misalign to match the address
    // The shifted value is written to the aligned memory address, but empty bytes are left unchanged
    // This allows SDL and SDR to be used in succession to write misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint64_t rVal = registersR[(opcode >> 16) & 0x1F] >> ((address & 7) * 8);
    uint64_t mVal = Memory::read<uint64_t>(address) & ~((uint64_t)-1 >> ((address & 7) * 8));
    Memory::write<uint64_t>(address, rVal | mVal);
}

void CPU::sdr(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the least significant byte of a misaligned 64-bit word
    // A register value is shifted left so its bytes misalign to match the address
    // The shifted value is written to the aligned memory address, but empty bytes are left unchanged
    // This allows SDL and SDR to be used in succession to write misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint64_t rVal = registersR[(opcode >> 16) & 0x1F] << ((7 - (address & 7)) * 8);
    uint64_t mVal = Memory::read<uint64_t>(address) & ~((uint64_t)-1 << ((7 - (address & 7)) * 8));
    Memory::write<uint64_t>(address, rVal | mVal);
}

void CPU::swr(uint32_t opcode)
{
    // This instruction is confusing, but I'll try my best to explain
    // The address points to the least significant byte of a misaligned 32-bit word
    // A register value is shifted left so its bytes misalign to match the address
    // The shifted value is written to the aligned memory address, but empty bytes are left unchanged
    // This allows SWL and SWR to be used in succession to write misaligned words
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    uint32_t rVal = (uint32_t)registersR[(opcode >> 16) & 0x1F] << ((3 - (address & 3)) * 8);
    uint32_t mVal = Memory::read<uint32_t>(address) & ~((uint32_t)-1 << ((3 - (address & 3)) * 8));
    Memory::write<uint32_t>(address, rVal | mVal);
}

void CPU::cache(uint32_t opcode)
{
    // The cache isn't emulated, so just warn about an unknown operation
    LOG_WARN("Unknown cache operation: 0x%02X\n", (opcode >> 16) & 0x1F);
}

void CPU::lwc1(uint32_t opcode)
{
    // Load a word from memory at base plus offset to a CP1 register
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    CPU_CP1::write(CP1_32BIT, (opcode >> 16) & 0x1F, Memory::read<uint32_t>(address));
}

void CPU::ldc1(uint32_t opcode)
{
    // Load a double-word from memory at base plus offset to a CP1 register
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    CPU_CP1::write(CP1_64BIT, (opcode >> 16) & 0x1F, Memory::read<uint64_t>(address));
}

void CPU::ld(uint32_t opcode)
{
    // Load a double-word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint64_t>(address);
}

void CPU::swc1(uint32_t opcode)
{
    // Store a word to memory at base plus offset from a CP1 register
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    Memory::write<uint32_t>(address, CPU_CP1::read(CP1_32BIT, (opcode >> 16) & 0x1F));
}

void CPU::sdc1(uint32_t opcode)
{
    // Store a double-word to memory at base plus offset from a CP1 register
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    Memory::write<uint64_t>(address, CPU_CP1::read(CP1_64BIT, (opcode >> 16) & 0x1F));
}

void CPU::sd(uint32_t opcode)
{
    // Store a double-word to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    Memory::write<uint64_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::sll(uint32_t opcode)
{
    // Shift a register left by a 5-bit immediate and store the lower result
    int32_t value = registersR[(opcode >> 16) & 0x1F] << ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::srl(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the lower result
    int32_t value = (uint32_t)registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::sra(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the lower signed result
    int32_t value = (int64_t)registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::sllv(uint32_t opcode)
{
    // Shift a register left by a register and store the lower result
    int32_t value = registersR[(opcode >> 16) & 0x1F] << (registersR[(opcode >> 21) & 0x1F] & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::srlv(uint32_t opcode)
{
    // Shift a register right by a register and store the lower result
    int32_t value = (uint32_t)registersR[(opcode >> 16) & 0x1F] >> (registersR[(opcode >> 21) & 0x1F] & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::srav(uint32_t opcode)
{
    // Shift a register right by a register and store the lower signed result
    int32_t value = (int64_t)registersR[(opcode >> 16) & 0x1F] >> (registersR[(opcode >> 21) & 0x1F] & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::jr(uint32_t opcode)
{
    // Jump to an address stored in a register
    programCounter = registersR[(opcode >> 21) & 0x1F] - 4;
}

void CPU::jalr(uint32_t opcode)
{
    // Save the return address and jump to an address stored in a register
    *registersW[(opcode >> 11) & 0x1F] = programCounter + 4;
    programCounter = registersR[(opcode >> 21) & 0x1F] - 4;
}

void CPU::syscall(uint32_t opcode)
{
    // Trigger a system call exception
    exception(8);
}

void CPU::break_(uint32_t opcode)
{
    // Trigger a breakpoint exception
    exception(9);
}

void CPU::mfhi(uint32_t opcode)
{
    // Copy the high word of the mult/div result to a register
    *registersW[(opcode >> 11) & 0x1F] = hi;
}

void CPU::mthi(uint32_t opcode)
{
    // Copy a register to the high word of the mult/div result
    hi = registersR[(opcode >> 21) & 0x1F];
}

void CPU::mflo(uint32_t opcode)
{
    // Copy the low word of the mult/div result to a register
    *registersW[(opcode >> 11) & 0x1F] = lo;
}

void CPU::mtlo(uint32_t opcode)
{
    // Copy a register to the low word of the mult/div result
    lo = registersR[(opcode >> 21) & 0x1F];
}

void CPU::dsllv(uint32_t opcode)
{
    // Shift a register left by a register and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] << (registersR[(opcode >> 21) & 0x1F] & 0x3F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsrlv(uint32_t opcode)
{
    // Shift a register right by a register and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] >> (registersR[(opcode >> 21) & 0x1F] & 0x3F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsrav(uint32_t opcode)
{
    // Shift a register right by a register and store the signed result
    uint64_t value = (int64_t)registersR[(opcode >> 16) & 0x1F] >> (registersR[(opcode >> 21) & 0x1F] & 0x3F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::mult(uint32_t opcode)
{
    // Multiply two 32-bit signed registers and set the 64-bit result
    int64_t value = (int64_t)(int32_t)registersR[(opcode >> 16) & 0x1F] *
        (int32_t)registersR[(opcode >> 21) & 0x1F];
    hi = value >> 32;
    lo = (uint32_t)value;
}

void CPU::multu(uint32_t opcode)
{
    // Multiply two 32-bit unsigned registers and set the 64-bit result
    uint64_t value = (uint64_t)(uint32_t)registersR[(opcode >> 16) & 0x1F] *
        (uint32_t)registersR[(opcode >> 21) & 0x1F];
    hi = value >> 32;
    lo = (uint32_t)value;
}

void CPU::div(uint32_t opcode)
{
    // Divide two 32-bit signed registers and set the 32-bit result and remainder
    // TODO: handle edge cases
    int32_t divisor = registersR[(opcode >> 16) & 0x1F];
    int32_t value = registersR[(opcode >> 21) & 0x1F];
    if (divisor && !(divisor == -1 && value == (1 << 31)))
    {
        hi = value % divisor;
        lo = value / divisor;
    }
}

void CPU::divu(uint32_t opcode)
{
    // Divide two 32-bit unsigned registers and set the 32-bit result and remainder
    // TODO: handle edge cases
    if (uint32_t divisor = registersR[(opcode >> 16) & 0x1F])
    {
        uint32_t value = registersR[(opcode >> 21) & 0x1F];
        hi = value % divisor;
        lo = value / divisor;
    }
}

void CPU::dmult(uint32_t opcode)
{
    // Multiply two signed 64-bit registers and set the 128-bit result
    __uint128_t value = (__int128_t)(int64_t)registersR[(opcode >> 16) & 0x1F] *
        (int64_t)registersR[(opcode >> 21) & 0x1F];
    hi = value >> 64;
    lo = value;
}

void CPU::dmultu(uint32_t opcode)
{
    // Multiply two unsigned 64-bit registers and set the 128-bit result
    __uint128_t value = (__uint128_t)registersR[(opcode >> 16) & 0x1F] *
        registersR[(opcode >> 21) & 0x1F];
    hi = value >> 64;
    lo = value;
}

void CPU::ddiv(uint32_t opcode)
{
    // Divide two 64-bit signed registers and set the 64-bit result and remainder
    // TODO: handle edge cases
    int64_t divisor = registersR[(opcode >> 16) & 0x1F];
    int64_t value = registersR[(opcode >> 21) & 0x1F];
    if (divisor && !(divisor == -1 && value == (1L << 63)))
    {
        hi = value % divisor;
        lo = value / divisor;
    }
}

void CPU::ddivu(uint32_t opcode)
{
    // Divide two 64-bit unsigned registers and set the 64-bit result and remainder
    // TODO: handle edge cases
    if (uint64_t divisor = registersR[(opcode >> 16) & 0x1F])
    {
        uint64_t value = registersR[(opcode >> 21) & 0x1F];
        hi = value % divisor;
        lo = value / divisor;
    }
}

void CPU::add(uint32_t opcode)
{
    // Add a register to a register and store the lower result
    // On overflow, trigger an integer overflow exception
    int32_t op1 = registersR[(opcode >> 21) & 0x1F];
    int32_t op2 = registersR[(opcode >> 16) & 0x1F];
    int32_t value = op1 + op2;
    if (!((op1 ^ op2) & (1 << 31)) && ((op1 ^ value) & (1 << 31)))
        return exception(12);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::addu(uint32_t opcode)
{
    // Add a register to a register and store the result
    int32_t value = registersR[(opcode >> 21) & 0x1F] + registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::sub(uint32_t opcode)
{
    // Subtract a register from a register and store the lower result
    // On overflow, trigger an integer overflow exception
    int32_t op1 = registersR[(opcode >> 21) & 0x1F];
    int32_t op2 = registersR[(opcode >> 16) & 0x1F];
    int32_t value = op1 - op2;
    if (((op1 ^ op2) & (1 << 31)) && ((op1 ^ value) & (1 << 31)))
        return exception(12);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::subu(uint32_t opcode)
{
    // Subtract a register from a register and store the result
    int32_t value = registersR[(opcode >> 21) & 0x1F] - registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::and_(uint32_t opcode)
{
    // Bitwise and a register with a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] & registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::or_(uint32_t opcode)
{
    // Bitwise or a register with a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] | registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::xor_(uint32_t opcode)
{
    // Bitwise exclusive or a register with a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] ^ registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::nor(uint32_t opcode)
{
    // Bitwise or a register with a register and store the negated result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] | registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = ~value;
}

void CPU::slt(uint32_t opcode)
{
    // Check if a signed register is less than another signed register, and store the result
    bool value = (int64_t)registersR[(opcode >> 21) & 0x1F] < (int64_t)registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::sltu(uint32_t opcode)
{
    // Check if a register is less than another register, and store the result
    bool value = registersR[(opcode >> 21) & 0x1F] < registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dadd(uint32_t opcode)
{
    // Add a register to a register and store the result
    // On overflow, trigger an integer overflow exception
    int64_t op1 = registersR[(opcode >> 21) & 0x1F];
    int64_t op2 = registersR[(opcode >> 16) & 0x1F];
    int64_t value = op1 + op2;
    if (!((op1 ^ op2) & (1LL << 63)) && ((op1 ^ value) & (1LL << 63)))
        return exception(12);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::daddu(uint32_t opcode)
{
    // Add a register to a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] + registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsub(uint32_t opcode)
{
    // Subtract a register from a register and store the result
    // On overflow, trigger an integer overflow exception
    int64_t op1 = registersR[(opcode >> 21) & 0x1F];
    int64_t op2 = registersR[(opcode >> 16) & 0x1F];
    int64_t value = op1 - op2;
    if (((op1 ^ op2) & (1LL << 63)) && ((op1 ^ value) & (1LL << 63)))
        return exception(12);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsubu(uint32_t opcode)
{
    // Subtract a register from a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] - registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsll(uint32_t opcode)
{
    // Shift a register left by a 5-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] << ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsrl(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsra(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the signed result
    uint64_t value = (int64_t)registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsll32(uint32_t opcode)
{
    // Shift a register left by a 5-bit immediate plus 32 and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] << (32 + ((opcode >> 6) & 0x1F));
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsrl32(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate plus 32 and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] >> (32 + ((opcode >> 6) & 0x1F));
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::dsra32(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate plus 32 and store the signed result
    uint64_t value = (int64_t)registersR[(opcode >> 16) & 0x1F] >> (32 + ((opcode >> 6) & 0x1F));
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void CPU::bltz(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less than zero
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] < 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::bgez(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater or equal to zero
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] >= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::bltzl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less than zero
    // Otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] < 0)
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::bgezl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater or equal to zero
    // Otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] >= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::bltzal(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less than zero
    // Also, save the return address
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] < 0)
    {
        *registersW[31] = programCounter + 4;
        programCounter += ((int16_t)opcode << 2) - 4;
    }
}

void CPU::bgezal(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater or equal to zero
    // Also, save the return address
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] >= 0)
    {
        *registersW[31] = programCounter + 4;
        programCounter += ((int16_t)opcode << 2) - 4;
    }
}

void CPU::bltzall(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less than zero
    // Also, save the return address; otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] < 0)
    {
        *registersW[31] = programCounter + 4;
        programCounter += ((int16_t)opcode << 2) - 4;
    }
    else
    {
        nextOpcode = 0;
    }
}

void CPU::bgezall(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater or equal to zero
    // Also, save the return address; otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] >= 0)
    {
        *registersW[31] = programCounter + 4;
        programCounter += ((int16_t)opcode << 2) - 4;
    }
    else
    {
        nextOpcode = 0;
    }
}

void CPU::eret(uint32_t opcode)
{
    // Return from an error exception or exception and clear the ERL or EXL bit
    uint32_t status = CPU_CP0::read(12);
    programCounter = CPU_CP0::read((status & 0x4) ? 30 : 14) - 4;
    nextOpcode = 0;
    CPU_CP0::write(12, status & ~((status & 0x4) ? 0x4 : 0x2));
}

void CPU::mfc0(uint32_t opcode)
{
    // Copy a 32-bit CP0 register value to a CPU register
    *registersW[(opcode >> 16) & 0x1F] = CPU_CP0::read((opcode >> 11) & 0x1F);
}

void CPU::mtc0(uint32_t opcode)
{
    // Copy a 32-bit CPU register value to a CP0 register
    CPU_CP0::write((opcode >> 11) & 0x1F, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::mfc1(uint32_t opcode)
{
    // Copy a 32-bit CP1 register value to a CPU register
    *registersW[(opcode >> 16) & 0x1F] = CPU_CP1::read(CP1_32BIT, (opcode >> 11) & 0x1F);
}

void CPU::dmfc1(uint32_t opcode)
{
    // Copy a 64-bit CP1 register value to a CPU register
    *registersW[(opcode >> 16) & 0x1F] = CPU_CP1::read(CP1_64BIT, (opcode >> 11) & 0x1F);
}

void CPU::cfc1(uint32_t opcode)
{
    // Copy a 32-bit CP1 control register value to a CPU register
    *registersW[(opcode >> 16) & 0x1F] = CPU_CP1::read(CP1_CTRL, (opcode >> 11) & 0x1F);
}

void CPU::mtc1(uint32_t opcode)
{
    // Copy a 32-bit CPU register value to a CP1 register
    CPU_CP1::write(CP1_32BIT, (opcode >> 11) & 0x1F, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::dmtc1(uint32_t opcode)
{
    // Copy a 64-bit CPU register value to a CP1 register
    CPU_CP1::write(CP1_64BIT, (opcode >> 11) & 0x1F, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::ctc1(uint32_t opcode)
{
    // Copy a 32-bit CPU register value to a CP1 control register
    CPU_CP1::write(CP1_CTRL, (opcode >> 11) & 0x1F, registersR[(opcode >> 16) & 0x1F]);
}

void CPU::bc1f(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if the CP1 condition bit is 0
    if (!(CPU_CP1::read(CP1_CTRL, 31) & (1 << 23)))
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::bc1t(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if the CP1 condition bit is 1
    if (CPU_CP1::read(CP1_CTRL, 31) & (1 << 23))
        programCounter += ((int16_t)opcode << 2) - 4;
}

void CPU::bc1fl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if the CP1 condition bit is 0
    // Otherwise, discard the delay slot opcode
    if (!(CPU_CP1::read(CP1_CTRL, 31) & (1 << 23)))
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::bc1tl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if CP1 condition bit is 1
    // Otherwise, discard the delay slot opcode
    if (CPU_CP1::read(CP1_CTRL, 31) & (1 << 23))
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void CPU::cop0(uint32_t opcode)
{
    // Trigger a CP0 unusable exception if CP0 is disabled
    if (!CPU_CP0::cpUsable(0))
        return exception(11);

    // Look up CP0 instructions that weren't worth making a table for
    switch ((opcode >> 21) & 0x1F)
    {
        case 0x00: return mfc0(opcode);
        case 0x04: return mtc0(opcode);
        case 0x10: return (opcode == 0x42000018) ? eret(opcode) : unk(opcode);
        default:   return unk(opcode);
    }
}

void CPU::cop1(uint32_t opcode)
{
    // Trigger a CP1 unusable exception if CP1 is disabled
    if (!CPU_CP0::cpUsable(1))
        return exception(11);

    // Look up CP1 instructions that weren't worth making a table for
    switch ((opcode >> 21) & 0x1F)
    {
        case 0x00: return mfc1(opcode);
        case 0x01: return dmfc1(opcode);
        case 0x02: return cfc1(opcode);
        case 0x04: return mtc1(opcode);
        case 0x05: return dmtc1(opcode);
        case 0x06: return ctc1(opcode);
        case 0x10: return (*CPU_CP1::sglInstrs[opcode & 0x3F])(opcode);
        case 0x11: return (*CPU_CP1::dblInstrs[opcode & 0x3F])(opcode);
        case 0x14: return (*CPU_CP1::wrdInstrs[opcode & 0x3F])(opcode);
        case 0x15: return (*CPU_CP1::lwdInstrs[opcode & 0x3F])(opcode);
        default:   return unk(opcode);

        case 0x08:
            switch ((opcode >> 16) & 0x3)
            {
                case 0x0: return bc1f(opcode);
                case 0x1: return bc1t(opcode);
                case 0x2: return bc1fl(opcode);
                case 0x3: return bc1tl(opcode);
            }
    }
}

void CPU::unk(uint32_t opcode)
{
    // Warn about unknown instructions
    LOG_CRIT("Unknown CPU opcode: 0x%08X @ 0x%X\n", opcode, programCounter - 4);
}
