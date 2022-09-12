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

#include "rsp.h"
#include "core.h"
#include "log.h"
#include "memory.h"
#include "rsp_cp0.h"
#include "rsp_cp2.h"

namespace RSP
{
    uint32_t registersR[33];
    uint32_t *registersW[32];
    uint32_t programCounter;
    uint32_t nextOpcode;

    extern void (*immInstrs[])(uint32_t);
    extern void (*regInstrs[])(uint32_t);
    extern void (*extInstrs[])(uint32_t);

    void j(uint32_t opcode);
    void jal(uint32_t opcode);
    void beq(uint32_t opcode);
    void bne(uint32_t opcode);
    void blez(uint32_t opcode);
    void bgtz(uint32_t opcode);
    void addiu(uint32_t opcode);
    void slti(uint32_t opcode);
    void sltiu(uint32_t opcode);
    void andi(uint32_t opcode);
    void ori(uint32_t opcode);
    void xori(uint32_t opcode);
    void lui(uint32_t opcode);
    void lb(uint32_t opcode);
    void lh(uint32_t opcode);
    void lw(uint32_t opcode);
    void lbu(uint32_t opcode);
    void lhu(uint32_t opcode);
    void sb(uint32_t opcode);
    void sh(uint32_t opcode);
    void sw(uint32_t opcode);

    void sll(uint32_t opcode);
    void srl(uint32_t opcode);
    void sra(uint32_t opcode);
    void sllv(uint32_t opcode);
    void srlv(uint32_t opcode);
    void srav(uint32_t opcode);
    void jr(uint32_t opcode);
    void jalr(uint32_t opcode);
    void break_(uint32_t opcode);
    void addu(uint32_t opcode);
    void subu(uint32_t opcode);
    void and_(uint32_t opcode);
    void or_(uint32_t opcode);
    void xor_(uint32_t opcode);
    void nor(uint32_t opcode);
    void slt(uint32_t opcode);
    void sltu(uint32_t opcode);

    void bltz(uint32_t opcode);
    void bgez(uint32_t opcode);
    void bltzal(uint32_t opcode);
    void bgezal(uint32_t opcode);

    void mfc0(uint32_t opcode);
    void mtc0(uint32_t opcode);
    void mfc2(uint32_t opcode);
    void cfc2(uint32_t opcode);
    void mtc2(uint32_t opcode);
    void ctc2(uint32_t opcode);

    void lbv(uint32_t opcode);
    void lsv(uint32_t opcode);
    void llv(uint32_t opcode);
    void ldv(uint32_t opcode);
    void lqv(uint32_t opcode);
    void lrv(uint32_t opcode);
    void lpv(uint32_t opcode);
    void luv(uint32_t opcode);
    void ltv(uint32_t opcode);
    void sbv(uint32_t opcode);
    void ssv(uint32_t opcode);
    void slv(uint32_t opcode);
    void sdv(uint32_t opcode);
    void sqv(uint32_t opcode);
    void srv(uint32_t opcode);
    void spv(uint32_t opcode);
    void suv(uint32_t opcode);
    void stv(uint32_t opcode);

    void cop0(uint32_t opcode);
    void cop2(uint32_t opcode);
    void lwc2(uint32_t opcode);
    void swc2(uint32_t opcode);
    void unk(uint32_t opcode);
}

// Immediate-type RSP instruction lookup table, using opcode bits 26-31
void (*RSP::immInstrs[0x40])(uint32_t) =
{
    nullptr, nullptr, j,    jal,   beq,  bne,  blez,  bgtz, // 0x00-0x07
    addiu,   addiu,   slti, sltiu, andi, ori,  xori,  lui,  // 0x08-0x0F
    cop0,    unk,     cop2, unk,   unk,  unk,  unk,   unk,  // 0x10-0x17
    unk,     unk,     unk,  unk,   unk,  unk,  unk,   unk,  // 0x18-0x1F
    lb,      lh,      unk,  lw,    lbu,  lhu,  unk,   unk,  // 0x20-0x27
    sb,      sh,      unk,  sw,    unk,  unk,  unk,   unk,  // 0x28-0x2F
    unk,     unk,     lwc2, unk,   unk,  unk,  unk,   unk,  // 0x30-0x37
    unk,     unk,     swc2, unk,   unk,  unk,  unk,   unk   // 0x38-0x3F
};

// Register-type RSP instruction lookup table, using opcode bits 0-5
void (*RSP::regInstrs[0x40])(uint32_t) =
{
    sll,  unk,  srl,  sra,  sllv, unk,    srlv, srav, // 0x00-0x07
    jr,   jalr, unk,  unk,  unk,  break_, unk,  unk,  // 0x08-0x0F
    unk,  unk,  unk,  unk,  unk,  unk,    unk,  unk,  // 0x10-0x17
    unk,  unk,  unk,  unk,  unk,  unk,    unk,  unk,  // 0x18-0x1F
    addu, addu, subu, subu, and_, or_,    xor_, nor,  // 0x20-0x27
    unk,  unk,  slt,  sltu, unk,  unk,    unk,  unk,  // 0x28-0x2F
    unk,  unk,  unk,  unk,  unk,  unk,    unk,  unk,  // 0x30-0x37
    unk,  unk,  unk,  unk,  unk,  unk,    unk,  unk   // 0x38-0x3F
};

// Extra-type RSP instruction lookup table, using opcode bits 16-20
void (*RSP::extInstrs[0x20])(uint32_t) =
{
    bltz,   bgez,   unk, unk, unk, unk, unk, unk, // 0x00-0x07
    unk,    unk,    unk, unk, unk, unk, unk, unk, // 0x08-0x0F
    bltzal, bgezal, unk, unk, unk, unk, unk, unk, // 0x10-0x17
    unk,    unk,    unk, unk, unk, unk, unk, unk  // 0x18-0x1F
};

void RSP::reset()
{
    // Map the writable registers so that writes to r0 are redirected
    registersW[0] = &registersR[32];
    for (int i = 1; i < 32; i++)
        registersW[i] = &registersR[i];

    // Reset the RSP to its initial state
    memset(registersR, 0, sizeof(registersR));
    writePC(0);
    setState(true);
}

uint32_t RSP::readPC()
{
    // Get the effective bits of the RSP program counter
    return (programCounter + 4) & 0xFFC;
}

void RSP::writePC(uint32_t value)
{
    // Set the effective bits of the RSP program counter
    programCounter = 0xA4001000 | ((value - 4) & 0xFFC);
    nextOpcode = 0;
}

void RSP::setState(bool halted)
{
    // Update the RSP running state
    Core::rspRunning = !halted;
}

void RSP::runOpcode()
{
    // Move an opcode through the pipeline
    uint32_t opcode = nextOpcode;
    programCounter = 0xA4001000 | ((programCounter + 4) & 0xFFC);
    nextOpcode = Memory::read<uint32_t>(programCounter);

    // Look up and execute an instruction
    // TODO: execute scalar and vector opcodes simultaneously
    switch (opcode >> 26)
    {
        default: return (*immInstrs[opcode >> 26])(opcode);
        case 0:  return (*regInstrs[opcode & 0x3F])(opcode);
        case 1:  return (*extInstrs[(opcode >> 16) & 0x1F])(opcode);
    }
}

void RSP::j(uint32_t opcode)
{
    // Jump to an immediate value
    programCounter = ((programCounter & 0xF0000000) | ((opcode & 0x3FFFFFF) << 2)) - 4;
}

void RSP::jal(uint32_t opcode)
{
    // Save the return address and jump to an immediate value
    *registersW[31] = (programCounter + 4) & 0xFFF;
    programCounter = ((programCounter & 0xF0000000) | ((opcode & 0x3FFFFFF) << 2)) - 4;
}

void RSP::beq(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers are equal
    if (registersR[(opcode >> 21) & 0x1F] == registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::bne(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers aren't equal
    if (registersR[(opcode >> 21) & 0x1F] != registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::blez(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less or equal to zero
    if ((int32_t)registersR[(opcode >> 21) & 0x1F] <= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::bgtz(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater than zero
    if ((int32_t)registersR[(opcode >> 21) & 0x1F] > 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::addiu(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the result
    int32_t value = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void RSP::slti(uint32_t opcode)
{
    // Check if a signed register is less than a signed 16-bit immediate, and store the result
    bool value = (int32_t)registersR[(opcode >> 21) & 0x1F] < (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void RSP::sltiu(uint32_t opcode)
{
    // Check if a register is less than a signed 16-bit immediate, and store the result
    bool value = registersR[(opcode >> 21) & 0x1F] < (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void RSP::andi(uint32_t opcode)
{
    // Bitwise and a register with a 16-bit immediate and store the result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] & (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void RSP::ori(uint32_t opcode)
{
    // Bitwise or a register with a 16-bit immediate and store the result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] | (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void RSP::xori(uint32_t opcode)
{
    // Bitwise exclusive or a register with a 16-bit immediate and store the result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] ^ (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void RSP::lui(uint32_t opcode)
{
    // Load a 16-bit immediate into the upper 16 bits of a register
    *registersW[(opcode >> 16) & 0x1F] = (int16_t)opcode << 16;
}

void RSP::lb(uint32_t opcode)
{
    // Load a signed byte from RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    *registersW[(opcode >> 16) & 0x1F] = (int8_t)Memory::read<uint8_t>(address);
}

void RSP::lh(uint32_t opcode)
{
    // Load a signed half-word from RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    *registersW[(opcode >> 16) & 0x1F] = (int16_t)Memory::read<uint16_t>(address);
}

void RSP::lw(uint32_t opcode)
{
    // Load a signed word from RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    *registersW[(opcode >> 16) & 0x1F] = (int32_t)Memory::read<uint32_t>(address);
}

void RSP::lbu(uint32_t opcode)
{
    // Load a byte from RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint8_t>(address);
}

void RSP::lhu(uint32_t opcode)
{
    // Load a half-word from RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint16_t>(address);
}

void RSP::sb(uint32_t opcode)
{
    // Store a byte to RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    Memory::write<uint8_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void RSP::sh(uint32_t opcode)
{
    // Store a half-word to RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    Memory::write<uint16_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void RSP::sw(uint32_t opcode)
{
    // Store a word to RSP DMEM at base register plus immeditate offset
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode) & 0xFFF);
    Memory::write<uint32_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void RSP::sll(uint32_t opcode)
{
    // Shift a register left by a 5-bit immediate and store the result
    int32_t value = registersR[(opcode >> 16) & 0x1F] << ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::srl(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the result
    int32_t value = (uint32_t)registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::sra(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the signed result
    int32_t value = (int32_t)registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::sllv(uint32_t opcode)
{
    // Shift a register left by a register and store the result
    int32_t value = registersR[(opcode >> 16) & 0x1F] << (registersR[(opcode >> 21) & 0x1F] & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::srlv(uint32_t opcode)
{
    // Shift a register right by a register and store the result
    int32_t value = (uint32_t)registersR[(opcode >> 16) & 0x1F] >> (registersR[(opcode >> 21) & 0x1F] & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::srav(uint32_t opcode)
{
    // Shift a register right by a register and store the signed result
    int32_t value = (int32_t)registersR[(opcode >> 16) & 0x1F] >> (registersR[(opcode >> 21) & 0x1F] & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::jr(uint32_t opcode)
{
    // Jump to an address stored in a register
    programCounter = registersR[(opcode >> 21) & 0x1F] - 4;
}

void RSP::jalr(uint32_t opcode)
{
    // Save the return address and jump to an address stored in a register
    *registersW[(opcode >> 11) & 0x1F] = (programCounter + 4) & 0xFFF;
    programCounter = registersR[(opcode >> 21) & 0x1F] - 4;
}

void RSP::break_(uint32_t opcode)
{
    // Trigger a break, halting the RSP
    RSP_CP0::triggerBreak();
}

void RSP::addu(uint32_t opcode)
{
    // Add a register to a register and store the result
    int32_t value = registersR[(opcode >> 21) & 0x1F] + registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::subu(uint32_t opcode)
{
    // Subtract a register from a register and store the result
    int32_t value = registersR[(opcode >> 21) & 0x1F] - registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::and_(uint32_t opcode)
{
    // Bitwise and a register with a register and store the result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] & registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::or_(uint32_t opcode)
{
    // Bitwise or a register with a register and store the result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] | registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::xor_(uint32_t opcode)
{
    // Bitwise exclusive or a register with a register and store the result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] ^ registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::nor(uint32_t opcode)
{
    // Bitwise or a register with a register and store the negated result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] | registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = ~value;
}

void RSP::slt(uint32_t opcode)
{
    // Check if a signed register is less than another signed register, and store the result
    bool value = (int32_t)registersR[(opcode >> 21) & 0x1F] < (int32_t)registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::sltu(uint32_t opcode)
{
    // Check if a register is less than another register, and store the result
    bool value = registersR[(opcode >> 21) & 0x1F] < registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void RSP::bltz(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less than zero
    if ((int32_t)registersR[(opcode >> 21) & 0x1F] < 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::bgez(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater or equal to zero
    if ((int32_t)registersR[(opcode >> 21) & 0x1F] >= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::bltzal(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less than zero
    // Also, save the return address
    *registersW[31] = (programCounter + 4) & 0xFFF;
    if ((int32_t)registersR[(opcode >> 21) & 0x1F] < 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::bgezal(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater or equal to zero
    // Also, save the return address
    *registersW[31] = (programCounter + 4) & 0xFFF;
    if ((int32_t)registersR[(opcode >> 21) & 0x1F] >= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void RSP::mfc0(uint32_t opcode)
{
    // Copy a 32-bit RSP register value from a CP0 register
    *registersW[(opcode >> 16) & 0x1F] = RSP_CP0::read((opcode >> 11) & 0x1F);
}

void RSP::mtc0(uint32_t opcode)
{
    // Copy a 32-bit RSP register value to a CP0 register
    RSP_CP0::write((opcode >> 11) & 0x1F, registersR[(opcode >> 16) & 0x1F]);
}

void RSP::mfc2(uint32_t opcode)
{
    // Copy a 16-bit RSP register value from a CP2 register
    *registersW[(opcode >> 16) & 0x1F] = RSP_CP2::read(false, (opcode >> 11) & 0x1F, (opcode >> 7) & 0xF);
}

void RSP::cfc2(uint32_t opcode)
{
    // Copy a 16-bit RSP register value from a CP2 control register
    *registersW[(opcode >> 16) & 0x1F] = RSP_CP2::read(true, (opcode >> 11) & 0x1F, 0);
}

void RSP::mtc2(uint32_t opcode)
{
    // Copy a 16-bit RSP register value to a CP2 register
    RSP_CP2::write(false, (opcode >> 11) & 0x1F, (opcode >> 7) & 0xF, registersR[(opcode >> 16) & 0x1F]);
}

void RSP::ctc2(uint32_t opcode)
{
    // Copy a 16-bit RSP register value to a CP2 control register
    RSP_CP2::write(true, (opcode >> 11) & 0x1F, 0, registersR[(opcode >> 16) & 0x1F]);
}

void RSP::lbv(uint32_t opcode)
{
    // Load an 8-bit value from memory to a vector register
    // Vector accesses are 16-bit, so 8 bits of the existing value are used
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = 0xA4000000 | ((registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) >> 1)) & 0xFFF);
    RSP_CP2::write(false, index, byte, (RSP_CP2::read(false, index, byte) & 0xFF) | (Memory::read<uint8_t>(address) << 8));
}

void RSP::lsv(uint32_t opcode)
{
    // Load a 16-bit value from memory to a vector register
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int8_t)(opcode << 1);
    RSP_CP2::write(false, index, byte, Memory::read<uint16_t>(0xA4000000 | (address & 0xFFF)));
}

void RSP::llv(uint32_t opcode)
{
    // Load a 32-bit value from memory to a vector register
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 1);
    for (int i = 0; i < 4; i += 2)
        RSP_CP2::write(false, index, byte + i, Memory::read<uint16_t>(0xA4000000 | ((address + i) & 0xFFF)));
}

void RSP::ldv(uint32_t opcode)
{
    // Load a 64-bit value from memory to a vector register
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 2);
    for (int i = 0; i < 8; i += 2)
        RSP_CP2::write(false, index, byte + i, Memory::read<uint16_t>(0xA4000000 | ((address + i) & 0xFFF)));
}

void RSP::lqv(uint32_t opcode)
{
    // Load up to 16 bytes from memory to a vector register, similar to the CPU's LDL instruction
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 3);
    for (int i = 0; i < 15 - (address & 0xF); i += 2)
        RSP_CP2::write(false, index, byte + i, Memory::read<uint16_t>(0xA4000000 | ((address + i) & 0xFFF)));

    // If loading an odd number of bytes, handle the last byte specially
    // Vector accesses are 16-bit, so 8 bits of the existing value are used
    if (address & 0x1)
    {
        int i = 15 - (address & 0xF);
        uint8_t value = Memory::read<uint8_t>(0xA4000000 | ((address + i) & 0xFFF));
        RSP_CP2::write(false, index, byte + i, (RSP_CP2::read(false, index, byte + i) & 0xFF) | (value << 8));
    }
}

void RSP::lrv(uint32_t opcode)
{
    // Load up to 16 bytes from memory to a vector register, similar to the CPU's LDR instruction
    // Since vector writes past byte 15 are ignored, odd numbers don't need special handling
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 3);
    for (int i = 16 - (address & 0xF); i < 16; i += 2)
        RSP_CP2::write(false, index, byte + i, Memory::read<uint16_t>(0xA4000000 | ((address + i - 16) & 0xFFF)));
}

void RSP::lpv(uint32_t opcode)
{
    // Load 8 signed 8-bit values from memory to a vector register
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 2);
    for (int i = 0; i < 8; i++)
        RSP_CP2::write(false, index, byte + i * 2, Memory::read<uint8_t>(0xA4000000 | ((address + i) & 0xFFF)) << 8);
}

void RSP::luv(uint32_t opcode)
{
    // Load 8 unsigned 8-bit values from memory to a vector register
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 2);
    for (int i = 0; i < 8; i++)
        RSP_CP2::write(false, index, byte + i * 2, Memory::read<uint8_t>(0xA4000000 | ((address + i) & 0xFFF)) << 7);
}

void RSP::ltv(uint32_t opcode)
{
    // Transpose 16 bytes from memory across 8 vector registers
    int index = (opcode >> 16) & 0x18;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 3);
    for (int i = 0; i < 16; i += 2)
    {
        uint8_t b = (byte + i) & 0xF;
        RSP_CP2::write(false, index + b / 2, i, Memory::read<uint16_t>(0xA4000000 | ((address + b) & 0xFFF)));
    }
}

void RSP::sbv(uint32_t opcode)
{
    // Store an 8-bit value from a vector register to memory
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) >> 1);
    Memory::write<uint8_t>(0xA4000000 | (address & 0xFFF), RSP_CP2::read(false, index, byte) >> 8);
}

void RSP::ssv(uint32_t opcode)
{
    // Store a 16-bit value from a vector register to memory
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (int8_t)(opcode << 1);
    Memory::write<uint16_t>(0xA4000000 | (address & 0xFFF), RSP_CP2::read(false, index, byte));
}

void RSP::slv(uint32_t opcode)
{
    // Store a 32-bit value from a vector register to memory
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 1);
    for (int i = 0; i < 4; i += 2)
        Memory::write<uint16_t>(0xA4000000 | ((address + i) & 0xFFF), RSP_CP2::read(false, index, byte + i));
}

void RSP::sdv(uint32_t opcode)
{
    // Store a 64-bit value from a vector register to memory
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 2);
    for (int i = 0; i < 8; i += 2)
        Memory::write<uint16_t>(0xA4000000 | ((address + i) & 0xFFF), RSP_CP2::read(false, index, byte + i));
}

void RSP::sqv(uint32_t opcode)
{
    // Store up to 16 bytes from a vector register to memory, similar to the CPU's SDL instruction
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 3);
    for (int i = 0; i < 15 - (address & 0xF); i += 2)
        Memory::write<uint16_t>(0xA4000000 | ((address + i) & 0xFFF), RSP_CP2::read(false, index, byte + i));

    // If storing an odd number of bytes, handle the last byte specially
    if (address & 0x1)
    {
        int i = 15 - (address & 0xF);
        address = 0xA4000000 | ((address + i) & 0xFFF);
        Memory::write<uint8_t>(address, RSP_CP2::read(false, index, byte + i) >> 8);
    }
}

void RSP::srv(uint32_t opcode)
{
    // Store up to 16 bytes from a vector register to memory, similar to the CPU's SDR instruction
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 3);
    for (int i = 16 - (address & 0xF); i < 15; i += 2)
        Memory::write<uint16_t>(0xA4000000 | ((address + i - 16) & 0xFFF), RSP_CP2::read(false, index, byte + i));

    // If storing an odd number of bytes, handle the last byte specially
    if (address & 0x1)
    {
        int i = 15;
        address = 0xA4000000 | ((address + i - 16) & 0xFFF);
        Memory::write<uint8_t>(address, RSP_CP2::read(false, index, byte + i) >> 8);
    }
}

void RSP::spv(uint32_t opcode)
{
    // Store 8 signed 8-bit values from a vector register to memory
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 2);
    for (int i = 0; i < 8; i++)
        Memory::write<uint8_t>(0xA4000000 | ((address + i) & 0xFFF), RSP_CP2::read(false, index, byte + i * 2) >> 8);
}

void RSP::suv(uint32_t opcode)
{
    // Store 8 unsigned 8-bit values from a vector register to memory
    int index = (opcode >> 16) & 0x1F;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 2);
    for (int i = 0; i < 8; i++)
        Memory::write<uint8_t>(0xA4000000 | ((address + i) & 0xFFF), RSP_CP2::read(false, index, byte + i * 2) >> 7);
}

void RSP::stv(uint32_t opcode)
{
    // Store 16 bytes transposed across 8 vector registers to memory
    int index = (opcode >> 16) & 0x18;
    int byte = (opcode >> 7) & 0xF;
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + ((int8_t)(opcode << 1) << 3);
    for (int i = 0; i < 16; i += 2)
    {
        uint16_t a = (address & 0xFF0) + ((address + i) & 0xF);
        uint16_t b = index + ((byte + i) & 0xF) / 2;
        Memory::write<uint16_t>(0xA4000000 | a, RSP_CP2::read(false, b, i));
    }
}

void RSP::cop0(uint32_t opcode)
{
    // Look up a CP0 instruction
    switch ((opcode >> 21) & 0x1F)
    {
        case 0x00: return mfc0(opcode);
        case 0x04: return mtc0(opcode);
        default:   return unk(opcode);
    }
}

void RSP::cop2(uint32_t opcode)
{
    // Look up a CP2 instruction
    switch ((opcode >> 21) & 0x1F)
    {
        case 0x00: return mfc2(opcode);
        case 0x02: return cfc2(opcode);
        case 0x04: return mtc2(opcode);
        case 0x06: return ctc2(opcode);

        default:
            if (opcode & (1 << 25))
                return (*RSP_CP2::vecInstrs[opcode & 0x3F])(opcode);
            return unk(opcode);
    }
}

void RSP::lwc2(uint32_t opcode)
{
    // Look up a CP2 load instruction
    switch ((opcode >> 11) & 0x1F)
    {
        case 0x00: return lbv(opcode);
        case 0x01: return lsv(opcode);
        case 0x02: return llv(opcode);
        case 0x03: return ldv(opcode);
        case 0x04: return lqv(opcode);
        case 0x05: return lrv(opcode);
        case 0x06: return lpv(opcode);
        case 0x07: return luv(opcode);
        case 0x0B: return ltv(opcode);
        default:   return unk(opcode);
    }
}

void RSP::swc2(uint32_t opcode)
{
    // Look up a CP2 store instruction
    switch ((opcode >> 11) & 0x1F)
    {
        case 0x00: return sbv(opcode);
        case 0x01: return ssv(opcode);
        case 0x02: return slv(opcode);
        case 0x03: return sdv(opcode);
        case 0x04: return sqv(opcode);
        case 0x05: return srv(opcode);
        case 0x06: return spv(opcode);
        case 0x07: return suv(opcode);
        case 0x0B: return stv(opcode);
        default:   return unk(opcode);
    }
}

void RSP::unk(uint32_t opcode)
{
    // Warn about unknown instructions
    LOG_CRIT("Unknown RSP SU opcode: 0x%08X @ 0x%X\n", opcode, 0x1000 | ((programCounter - 4) & 0xFFF));
}
