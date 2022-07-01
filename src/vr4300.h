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

#ifndef VR4300_H
#define VR4300_H

#include <string>

namespace VR4300
{
    void reset();
    void runOpcode();

    void j(uint32_t opcode);
    void jal(uint32_t opcode);
    void beq(uint32_t opcode);
    void bne(uint32_t opcode);
    void blez(uint32_t opcode);
    void bgtz(uint32_t opcode);
    void addi(uint32_t opcode);
    void addiu(uint32_t opcode);
    void andi(uint32_t opcode);
    void ori(uint32_t opcode);
    void lui(uint32_t opcode);
    void beql(uint32_t opcode);
    void bnel(uint32_t opcode);
    void blezl(uint32_t opcode);
    void bgtzl(uint32_t opcode);
    void daddi(uint32_t opcode);
    void daddiu(uint32_t opcode);
    void lb(uint32_t opcode);
    void lh(uint32_t opcode);
    void lw(uint32_t opcode);
    void lbu(uint32_t opcode);
    void lhu(uint32_t opcode);
    void lwu(uint32_t opcode);
    void sb(uint32_t opcode);
    void sh(uint32_t opcode);
    void sw(uint32_t opcode);
    void ld(uint32_t opcode);
    void sd(uint32_t opcode);

    void sll(uint32_t opcode);
    void srl(uint32_t opcode);
    void sra(uint32_t opcode);
    void jr(uint32_t opcode);
    void jalr(uint32_t opcode);
    void add(uint32_t opcode);
    void addu(uint32_t opcode);
    void and_(uint32_t opcode);
    void or_(uint32_t opcode);
    void xor_(uint32_t opcode);
    void nor(uint32_t opcode);
    void slt(uint32_t opcode);
    void sltu(uint32_t opcode);
    void dsll(uint32_t opcode);
    void dsrl(uint32_t opcode);
    void dsra(uint32_t opcode);
    void dsll32(uint32_t opcode);
    void dsrl32(uint32_t opcode);
    void dsra32(uint32_t opcode);

    void unk(uint32_t opcode);
}

#endif // VR4300_H
