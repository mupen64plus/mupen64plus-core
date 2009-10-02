/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - ops.h                                                   *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void NI();
void LW();
void LUI();
void ADDIU();
void BNE();
void SLL();
void SW();
void ORI();
void ADDI();
void OR();
void JAL();
void SLTI();
void BEQL();
void ANDI();
void XORI();
void JR();
void SRL();
void BNEL();
void BEQ();
void BLEZL();
void SUBU();
void MULTU();
void MFLO();
void ADDU();
void SB();
void AND();
void LBU();
void BGEZL();
void SLT();
void ADD();
void CACHE();
void SLTU();
void SRLV();
void SLLV();
void XOR();
void BGEZAL();
void CFC1();
void MTC0();
void MFC0();
void CTC1();
void BLEZ();
void TLBWI();
void LD();
void DMULTU();
void DSLL32();
void DSRA32();
void DDIVU();
void SRA();
void SLTIU();
void SH();
void LHU();
void MTLO();
void MTHI();
void ERET();
void SD();
void J();
void DIV();
void MFHI();
void BGEZ();
void MULT();
void LWC1();
void MTC1();
void CVT_S_W();
void DIV_S();
void MUL_S();
void ADD_S();
void CVT_D_S();
void ADD_D();
void TRUNC_W_D();
void MFC1();
void NOP();
void RESERVED();

void TLBP();
void TLBR();
void SWL();
void SWR();
void LWL();
void LWR();
void SRAV();
void BLTZ();

void BGTZ();
void LB();

void SWC1();
void CVT_D_W();
void MUL_D();
void DIV_D();
void CVT_S_D();
void MOV_S();
void C_LE_S();
void BC1T();
void TRUNC_W_S();
void C_LT_S();
void BC1FL();
void NEG_S();
void LDC1();
void SUB_D();
void C_LE_D();
void BC1TL();
void BGEZAL_IDLE();
void J_IDLE();
void BLEZ_IDLE();
void BEQ_IDLE();

void LH();
void NOR();
void NEG_D();
void MOV_D();
void C_LT_D();
void BC1F();

void SUB();

void CVT_W_S();
void DIVU();

void JALR();
void SDC1();
void C_EQ_S();
void SUB_S();
void BLTZL();

void CVT_W_D();
void SQRT_S();
void C_EQ_D();
void FIN_BLOCK();
void DDIV();
void DADDIU();
void ABS_S();
void BGTZL();
void DSRAV();
void DSLLV();
void CVT_S_L();
void DMTC1();
void DSRLV();
void DSRA();
void DMULT();
void DSLL();
void SC();

void SYSCALL();
void DADD();
void DADDU();
void DSUB();
void DSUBU();
void TEQ();
void DSRL();
void DSRL32();
void BLTZ_IDLE();
void BGEZ_IDLE();
void BLTZL_IDLE();
void BGEZL_IDLE();
void BLTZAL();
void BLTZAL_IDLE();
void BLTZALL();
void BLTZALL_IDLE();
void BGEZALL();
void BGEZALL_IDLE();
void TLBWR();
void BC1F_IDLE();
void BC1T_IDLE();
void BC1FL_IDLE();
void BC1TL_IDLE();
void ROUND_L_S();
void TRUNC_L_S();
void CEIL_L_S();
void FLOOR_L_S();
void ROUND_W_S();
void CEIL_W_S();
void FLOOR_W_S();
void CVT_L_S();
void C_F_S();
void C_UN_S();
void C_UEQ_S();
void C_OLT_S();
void C_ULT_S();
void C_OLE_S();
void C_ULE_S();
void C_SF_S();
void C_NGLE_S();
void C_SEQ_S();
void C_NGL_S();
void C_NGE_S();
void C_NGT_S();
void SQRT_D();
void ABS_D();
void ROUND_L_D();
void TRUNC_L_D();
void CEIL_L_D();
void FLOOR_L_D();
void ROUND_W_D();
void CEIL_W_D();
void FLOOR_W_D();
void CVT_L_D();
void C_F_D();
void C_UN_D();
void C_UEQ_D();
void C_OLT_D();
void C_ULT_D();
void C_OLE_D();
void C_ULE_D();
void C_SF_D();
void C_NGLE_D();
void C_SEQ_D();
void C_NGL_D();
void C_NGE_D();
void C_NGT_D();
void CVT_D_L();
void DMFC1();
void JAL_IDLE();
void BNE_IDLE();
void BGTZ_IDLE();
void BEQL_IDLE();
void BNEL_IDLE();
void BLEZL_IDLE();
void BGTZL_IDLE();
void DADDI();
void LDL();
void LDR();
void LWU();
void SDL();
void SDR();
void SYNC();
void BLTZ_OUT();
void BGEZ_OUT();
void BLTZL_OUT();
void BGEZL_OUT();
void BLTZAL_OUT();
void BGEZAL_OUT();
void BLTZALL_OUT();
void BGEZALL_OUT();
void BC1F_OUT();
void BC1T_OUT();
void BC1FL_OUT();
void BC1TL_OUT();
void J_OUT();
void JAL_OUT();
void BEQ_OUT();
void BNE_OUT();
void BLEZ_OUT();
void BGTZ_OUT();
void BEQL_OUT();
void BNEL_OUT();
void BLEZL_OUT();
void BGTZL_OUT();
void NOTCOMPILED();
void LL();
void NOTCOMPILED2();

