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

#include "vr4300.h"
#include "memory.h"

// Immediate-type instruction lookup table, using opcode bits 26-31
static void (*immInstrs[0x40])(uint32_t) =
{
    nullptr,       VR4300::unk,    VR4300::j,     VR4300::jal,   // 0x00-0x03
    VR4300::beq,   VR4300::bne,    VR4300::blez,  VR4300::bgtz,  // 0x04-0x07
    VR4300::addi,  VR4300::addiu,  VR4300::unk,   VR4300::unk,   // 0x08-0x0B
    VR4300::andi,  VR4300::ori,    VR4300::unk,   VR4300::lui,   // 0x0C-0x0F
    VR4300::unk,   VR4300::unk,    VR4300::unk,   VR4300::unk,   // 0x10-0x13
    VR4300::beql,  VR4300::bnel,   VR4300::blezl, VR4300::bgtzl, // 0x14-0x17
    VR4300::daddi, VR4300::daddiu, VR4300::unk,   VR4300::unk,   // 0x18-0x1B
    VR4300::unk,   VR4300::unk,    VR4300::unk,   VR4300::unk,   // 0x1C-0x1F
    VR4300::lb,    VR4300::lh,     VR4300::unk,   VR4300::lw,    // 0x20-0x23
    VR4300::lbu,   VR4300::lhu,    VR4300::unk,   VR4300::lwu,   // 0x24-0x27
    VR4300::sb,    VR4300::sh,     VR4300::unk,   VR4300::sw,    // 0x28-0x2B
    VR4300::unk,   VR4300::unk,    VR4300::unk,   VR4300::unk,   // 0x2C-0x2F
    VR4300::unk,   VR4300::unk,    VR4300::unk,   VR4300::unk,   // 0x30-0x33
    VR4300::unk,   VR4300::unk,    VR4300::unk,   VR4300::ld,    // 0x34-0x37
    VR4300::unk,   VR4300::unk,    VR4300::unk,   VR4300::unk,   // 0x38-0x3B
    VR4300::unk,   VR4300::unk,    VR4300::unk,   VR4300::sd,    // 0x3C-0x3F
};

// Register-type instruction lookup table, using opcode bits 0-5
static void (*regInstrs[0x40])(uint32_t) =
{
    VR4300::sll,    VR4300::unk,  VR4300::srl,    VR4300::sra,    // 0x00-0x03
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x04-0x07
    VR4300::jr,     VR4300::jalr, VR4300::unk,    VR4300::unk,    // 0x08-0x0B
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x0C-0x0F
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x10-0x13
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x14-0x17
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x18-0x1B
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x1C-0x1F
    VR4300::add,    VR4300::addu, VR4300::unk,    VR4300::unk,    // 0x20-0x23
    VR4300::and_,   VR4300::or_,  VR4300::xor_,   VR4300::nor,    // 0x24-0x27
    VR4300::unk,    VR4300::unk,  VR4300::slt,    VR4300::sltu,   // 0x28-0x2B
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x2C-0x2F
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x30-0x33
    VR4300::unk,    VR4300::unk,  VR4300::unk,    VR4300::unk,    // 0x34-0x37
    VR4300::dsll,   VR4300::unk,  VR4300::dsrl,   VR4300::dsra,   // 0x38-0x3B
    VR4300::dsll32, VR4300::unk,  VR4300::dsrl32, VR4300::dsra32, // 0x3C-0x3F
};

static uint64_t registersR[33];
static uint64_t *registersW[32];
static uint32_t programCounter;
static uint32_t nextOpcode;

void VR4300::reset()
{
    // Map the writable registers so that writes to r0 are redirected
    registersW[0] = &registersR[32];
    for (int i = 1; i < 32; i++)
        registersW[i] = &registersR[i];

    // Reset the interpreter to its initial state
    programCounter = 0x84000040; // IPL3
    nextOpcode = Memory::read<uint32_t>(programCounter);
}

void VR4300::runOpcode()
{
    // Move an opcode through the pipeline
    uint32_t opcode = nextOpcode;
    nextOpcode = Memory::read<uint32_t>(programCounter += 4);

    // Look up and execute an instruction
    if (auto instr = immInstrs[opcode >> 26])
        return (*instr)(opcode);
    return (*regInstrs[opcode & 0x3F])(opcode);
}

void VR4300::j(uint32_t opcode)
{
    // Jump to an immediate value
    programCounter = ((programCounter & 0xF0000000) | ((opcode & 0x3FFFFFF) << 2)) - 4;
}

void VR4300::jal(uint32_t opcode)
{
    // Save the return address and jump to an immediate value
    *registersW[31] = programCounter + 4;
    programCounter = ((programCounter & 0xF0000000) | ((opcode & 0x3FFFFFF) << 2)) - 4;
}

void VR4300::beq(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers are equal
    if (registersR[(opcode >> 21) & 0x1F] == registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
}

void VR4300::bne(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers aren't equal
    if (registersR[(opcode >> 21) & 0x1F] != registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
}

void VR4300::blez(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less or equal to zero
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] <= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void VR4300::bgtz(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater than zero
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] > 0)
        programCounter += ((int16_t)opcode << 2) - 4;
}

void VR4300::addi(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the lower result
    // TODO: overflow exception
    uint32_t value = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void VR4300::addiu(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the lower result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void VR4300::andi(uint32_t opcode)
{
    // Bitwise and a register with a 16-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] & (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void VR4300::ori(uint32_t opcode)
{
    // Bitwise or a register with a 16-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] | (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void VR4300::lui(uint32_t opcode)
{
    // Load a 16-bit immediate into the upper 16 bits of a register
    *registersW[(opcode >> 16) & 0x1F] = (opcode & 0xFFFF) << 16;
}

void VR4300::beql(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers are equal
    // Otherwise, discard the delay slot opcode
    if (registersR[(opcode >> 21) & 0x1F] == registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void VR4300::bnel(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if two registers aren't equal
    // Otherwise, discard the delay slot opcode
    if (registersR[(opcode >> 21) & 0x1F] != registersR[(opcode >> 16) & 0x1F])
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void VR4300::blezl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is less or equal to zero
    // Otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] <= 0)
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void VR4300::bgtzl(uint32_t opcode)
{
    // Add a 16-bit offset to the program counter if a register is greater than zero
    // Otherwise, discard the delay slot opcode
    if ((int64_t)registersR[(opcode >> 21) & 0x1F] > 0)
        programCounter += ((int16_t)opcode << 2) - 4;
    else
        nextOpcode = 0;
}

void VR4300::daddi(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the result
    // TODO: overflow exception
    uint64_t value = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void VR4300::daddiu(uint32_t opcode)
{
    // Add a signed 16-bit immediate to a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] + (int16_t)opcode;
    *registersW[(opcode >> 16) & 0x1F] = value;
}

void VR4300::lb(uint32_t opcode)
{
    // Load a signed byte from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = (int8_t)Memory::read<uint8_t>(address);
}

void VR4300::lh(uint32_t opcode)
{
    // Load a signed half-word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = (int16_t)Memory::read<uint16_t>(address);
}

void VR4300::lw(uint32_t opcode)
{
    // Load a signed word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = (int32_t)Memory::read<uint32_t>(address);
}

void VR4300::lbu(uint32_t opcode)
{
    // Load a byte from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint8_t>(address);
}

void VR4300::lhu(uint32_t opcode)
{
    // Load a half-word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint16_t>(address);
}

void VR4300::lwu(uint32_t opcode)
{
    // Load a word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint32_t>(address);
}

void VR4300::sb(uint32_t opcode)
{
    // Store a byte to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    Memory::write<uint8_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void VR4300::sh(uint32_t opcode)
{
    // Store a half-word to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    Memory::write<uint16_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void VR4300::sw(uint32_t opcode)
{
    // Store a word to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    Memory::write<uint32_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void VR4300::ld(uint32_t opcode)
{
    // Load a double-word from memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    *registersW[(opcode >> 16) & 0x1F] = Memory::read<uint64_t>(address);
}

void VR4300::sd(uint32_t opcode)
{
    // Store a double-word to memory at base register plus immeditate offset
    uint32_t address = registersR[(opcode >> 21) & 0x1F] + (opcode & 0xFFFF);
    Memory::write<uint64_t>(address, registersR[(opcode >> 16) & 0x1F]);
}

void VR4300::sll(uint32_t opcode)
{
    // Shift a register left by a 5-bit immediate and store the lower result
    uint32_t value = registersR[(opcode >> 16) & 0x1F] << ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::srl(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the lower result
    uint32_t value = registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::sra(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the lower signed result
    uint32_t value = (int32_t)registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::jr(uint32_t opcode)
{
    // Jump to an address stored in a register
    programCounter = registersR[(opcode >> 21) & 0x1F] - 4;
}

void VR4300::jalr(uint32_t opcode)
{
    // Save the return address and jump to an address stored in a register
    *registersW[(opcode >> 11) & 0x1F] = programCounter + 4;
    programCounter = registersR[(opcode >> 21) & 0x1F] - 4;
}

void VR4300::add(uint32_t opcode)
{
    // Add a register to a register and store the lower result
    // TODO: overflow exception
    uint32_t value = registersR[(opcode >> 21) & 0x1F] + registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::addu(uint32_t opcode)
{
    // Add a register to a register and store the result
    uint32_t value = registersR[(opcode >> 21) & 0x1F] + registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::and_(uint32_t opcode)
{
    // Bitwise and a register with a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] & registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::or_(uint32_t opcode)
{
    // Bitwise or a register with a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] | registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::xor_(uint32_t opcode)
{
    // Bitwise exclusive or a register with a register and store the result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] ^ registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::nor(uint32_t opcode)
{
    // Bitwise or a register with a register and store the negated result
    uint64_t value = registersR[(opcode >> 21) & 0x1F] | registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = ~value;
}

void VR4300::slt(uint32_t opcode)
{
    // Check if a signed register is less than another signed register, and store the result
    bool value = (int64_t)registersR[(opcode >> 21) & 0x1F] < (int64_t)registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::sltu(uint32_t opcode)
{
    // Check if a register is less than another register, and store the result
    bool value = registersR[(opcode >> 21) & 0x1F] < registersR[(opcode >> 16) & 0x1F];
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::dsll(uint32_t opcode)
{
    // Shift a register left by a 5-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] << ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::dsrl(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::dsra(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate and store the signed result
    uint64_t value = (int64_t)registersR[(opcode >> 16) & 0x1F] >> ((opcode >> 6) & 0x1F);
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::dsll32(uint32_t opcode)
{
    // Shift a register left by a 5-bit immediate plus 32 and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] << (32 + ((opcode >> 6) & 0x1F));
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::dsrl32(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate plus 32 and store the result
    uint64_t value = registersR[(opcode >> 16) & 0x1F] >> (32 + ((opcode >> 6) & 0x1F));
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::dsra32(uint32_t opcode)
{
    // Shift a register right by a 5-bit immediate plus 32 and store the signed result
    uint64_t value = (int64_t)registersR[(opcode >> 16) & 0x1F] >> (32 + ((opcode >> 6) & 0x1F));
    *registersW[(opcode >> 11) & 0x1F] = value;
}

void VR4300::unk(uint32_t opcode)
{
    // Unknown instructions mean disaster, so just warn and exit
    printf("Unknown opcode: 0x%08X @ 0x%X\n", opcode, programCounter - 4);
    exit(0);
}
