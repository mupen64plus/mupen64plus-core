/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - decoder.c                                               *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2002 davFr                                              *
 *   Copyright (C) 2008 ZZT32                                              *
 *   Copyright (C) 2008 DarkJezter                                         *
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

#include <stdio.h>
#include <string.h>

#include "types.h"
#include "decoder.h"
#include "opprintf.h"

static int  mot;
static char *op;
static char *args;
static int pc;

static void RESERV(){
    sprintf(op, "INVLD(%02X)       0x%08X", (mot>>26)&0x3F, mot);
}

static void mr4kd_disassemble( uint32 instruction, uint32 counter, char * buffer );

//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ SPECIAL ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void SLL()
{
    if( !mot )
        mr4kd_sprintf( op, "NOP", mot, pc, "%n0" );
    
    else{
        mr4kd_sprintf( op, "SLL", mot, pc, "%ns%rd, %rt, %sa" );
    }
}

static void SRL(){
    mr4kd_sprintf( op, "SRL", mot, pc, "%ns%rd, %rt, %sa" );
}

static void SRA(){
    mr4kd_sprintf( op, "SRA", mot, pc, "%ns%rd, %rt, %sa" );
}

static void SLLV(){
    mr4kd_sprintf( op, "SLLV", mot, pc, "%ns%rd, %rt, %rs" );
}

static void SRLV(){
    mr4kd_sprintf( op, "SRLV", mot, pc, "%ns%rd, %rt, %rs" );
}

static void SRAV(){
    mr4kd_sprintf( op, "SRAV", mot, pc, "%ns%rd, %rt, %rs" );

}

static void JR(){
    mr4kd_sprintf( op, "JR", mot, pc, "%ns%rs" );
}

static void JALR(){
    mr4kd_sprintf( op, "JALR", mot, pc, "%ns%rd, %rs" );
}

static void SYSCALL(){
    mr4kd_sprintf( op, "SYSCALL", mot, pc, "%n0" );
}

static void BREAK(){
    mr4kd_sprintf( op, "BREAK", mot, pc, "%n0" );
}

static void SYNC(){
    mr4kd_sprintf( op, "SYNC", mot, pc, "%n0" );
}

static void MFHI(){
    mr4kd_sprintf( op, "MFHI", mot, pc, "%ns%rd" );
}

static void MTHI(){
    mr4kd_sprintf( op, "MTHI", mot, pc, "%ns%rs" );
}

static void MFLO(){
    mr4kd_sprintf( op, "MFLO", mot, pc, "%ns%rd" );
}

static void MTLO(){
    mr4kd_sprintf( op, "MTLO", mot, pc, "%ns%rs" );
}

static void DSLLV(){
    mr4kd_sprintf( op, "DSLLV", mot, pc, "%ns%rd, %rt, %rs" );
}

static void DSRLV(){
    mr4kd_sprintf( op, "DSRLV", mot, pc, "%ns%rd, %rt, %rs" );
}

static void DSRAV(){
    mr4kd_sprintf( op, "DSRAV", mot, pc, "%ns%rd, %rt, %rs" );
}

static void MULT(){
    mr4kd_sprintf( op, "MULT", mot, pc, "%ns%rs, %rt" );
}

static void MULTU(){
    mr4kd_sprintf( op, "MULTU", mot, pc, "%ns%rs, %rt" );
}

static void DIV(){
    mr4kd_sprintf( op, "DIV", mot, pc, "%ns%rs, %rt" );
}

static void DIVU(){
    mr4kd_sprintf( op, "DIVU", mot, pc, "%ns%rs, %rt" ); 
}

static void DMULT(){
    mr4kd_sprintf( op, "DMULT", mot, pc, "%ns%rs, %rt" ); 
}

static void DMULTU(){
    mr4kd_sprintf( op, "DMULTU", mot, pc, "%ns%rs, %rt" ); 
}

static void DDIV(){
    mr4kd_sprintf( op, "DDIV", mot, pc, "%ns%rs, %rt" ); 
}

static void DDIVU(){
    mr4kd_sprintf( op, "DDIVU", mot, pc, "%ns%rs, %rt" ); 
}

static void ADD(){
    mr4kd_sprintf( op, "ADD", mot, pc, "%ns%rd, %rs, %rt" ); 
}

static void ADDU(){
    mr4kd_sprintf( op, "ADDU", mot, pc, "%ns%rd, %rs, %rt" ); 
}

static void SUB()
{
    mr4kd_sprintf( op, "SUB", mot, pc, "%ns%rd, %rs, %rt" );
}

static void SUBU(){
    mr4kd_sprintf( op, "SUBU", mot, pc, "%ns%rd, %rs, %rt" );
}

static void AND(){
    mr4kd_sprintf( op, "AND", mot, pc, "%ns%rd, %rs, %rt" );
}

static void OR(){
    mr4kd_sprintf( op, "OR", mot, pc, "%ns%rd, %rs, %rt" );
}

static void XOR(){
    mr4kd_sprintf( op, "XOR", mot, pc, "%ns%rd, %rs, %rt" );
}

static void NOR(){
    mr4kd_sprintf( op, "NOR", mot, pc, "%ns%rd, %rs, %rt" );
}

static void SLT(){
    mr4kd_sprintf( op, "SLT", mot, pc, "%ns%rd, %rs, %rt" );
}

static void SLTU(){
    mr4kd_sprintf( op, "SLTU", mot, pc, "%ns%rd, %rs, %rt" );
}

static void DADD(){
    mr4kd_sprintf( op, "DADD", mot, pc, "%ns%rd, %rs, %rt" ); 
}

static void DADDU(){
    mr4kd_sprintf( op, "DADDU", mot, pc, "%ns%rd, %rs, %rt" ); 
}

static void DSUB(){
    mr4kd_sprintf( op, "DSUB", mot, pc, "%ns%rd, %rs, %rt" ); 
}

static void DSUBU(){
    mr4kd_sprintf( op, "DSUBU", mot, pc, "%ns%rd, %rs, %rt" ); 
}

static void TGE(){
    mr4kd_sprintf( op, "TGE", mot, pc, "%ns%rs, %rt" ); 
}

static void TGEU(){
    mr4kd_sprintf( op, "TGEU", mot, pc, "%ns%rs, %rt" ); 
}

static void TLT(){
    mr4kd_sprintf( op, "TLT", mot, pc, "%ns%rs, %rt" ); 
}

static void TLTU(){
    mr4kd_sprintf( op, "TLTU", mot, pc, "%ns%rs, %rt" ); 
}

static void TEQ(){
    mr4kd_sprintf( op, "TEQ", mot, pc, "%ns%rs, %rt" ); 
}

static void TNE(){
    mr4kd_sprintf( op, "TNE", mot, pc, "%ns%rs, %rt" ); 
}

static void DSLL(){
    mr4kd_sprintf( op, "DSLL", mot, pc, "%ns%rd, %rt, %sa" ); 
}

static void DSRL(){
    mr4kd_sprintf( op, "DSRL", mot, pc, "%ns%rd, %rt, %sa" ); 
}

static void DSRA(){
    mr4kd_sprintf( op, "DSRA", mot, pc, "%ns%rd, %rt, %sa" ); 
}

static void DSLL32(){
    mr4kd_sprintf( op, "DSLL32", mot, pc, "%ns%rd, %rt, %sa" );
}

static void DSRL32(){
    mr4kd_sprintf( op, "DSRL32", mot, pc, "%ns%rd, %rt, %sa" );
}

static void DSRA32(){
    mr4kd_sprintf( op, "DSRA32", mot, pc, "%ns%rd, %rt, %sa" );
}

static void special()
{
//  sprintf(op, "[00] SPECIAL");
    
    switch( mot & 0x3F)
    {
        case 0x00: SLL();   break;
        case 0x02: SRL();   break;
        case 0x03: SRA();   break;
        case 0x04: SLLV();  break;
        case 0x06: SRLV();  break;
        case 0x07: SRAV();  break;
        case 0x08: JR();    break;
        case 0x09: JALR();  break;
        case 0x0C: SYSCALL();   break;
        case 0x0D: BREAK(); break;
        case 0x0F: SYNC();  break;
        case 0x10: MFHI();  break;
        case 0x11: MTHI();  break;
        case 0x12: MFLO();  break;
        case 0x13: MTLO();  break;
        case 0x14: DSLLV(); break;
        case 0x16: DSRLV(); break;
        case 0x17: DSRAV(); break;
        case 0x18: MULT();  break;
        case 0x19: MULTU(); break;
        case 0x1A: DIV();   break;
        case 0x1B: DIVU();  break;
        case 0x1C: DMULT(); break;
        case 0x1D: DMULTU();    break;
        case 0x1E: DDIV();  break;
        case 0x1F: DDIVU(); break;
        case 0x20: ADD();   break;
        case 0x21: ADDU();  break;
        case 0x22: SUB();   break;
        case 0x23: SUBU();  break;
        case 0x24: AND();   break;
        case 0x25: OR();    break;
        case 0x26: XOR();   break;
        case 0x27: NOR();   break;
        case 0x2A: SLT();   break;
        case 0x2B: SLTU();  break;
        case 0x2C: DADD();  break;
        case 0x2D: DADDU(); break;
        case 0x2E: DSUB();  break;
        case 0x2F: DSUBU(); break;
        case 0x30: TGE();   break;
        case 0x31: TGEU();  break;
        case 0x32: TLT();   break;
        case 0x33: TLTU();  break;
        case 0x34: TEQ();   break;
        case 0x36: TNE();   break;
        case 0x38: DSLL();  break;
        case 0x3A: DSRL();  break;
        case 0x3B: DSRA();  break;
        case 0x3C: DSLL32();    break;
        case 0x3E: DSRL32();    break;
        case 0x3F: DSRA32();    break;
        default :  RESERV(); //just to be sure
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ REGIMM ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void BLTZ(){
    mr4kd_sprintf( op, "BLTZ", mot, pc, "%ns%rs, %br" );
}

static void BGEZ(){
    mr4kd_sprintf( op, "BGEZ", mot, pc, "%ns%rs, %br" );
}

static void BLTZL(){
    mr4kd_sprintf( op, "BLTZL", mot, pc, "%ns%rs, %br" );
}

static void BGEZL(){
    mr4kd_sprintf( op, "BGEZL", mot, pc, "%ns%rs, %br" );
}

static void TGEI(){
    mr4kd_sprintf( op, "TGEI", mot, pc, "%ns%rs, %ih" );
}

static void TGEIU(){
    mr4kd_sprintf( op, "TGEIU", mot, pc, "%ns%rs, %ih" );
}

static void TLTI(){
    mr4kd_sprintf( op, "TLTI", mot, pc, "%ns%rs, %ih" );
}

static void TLTIU(){
    mr4kd_sprintf( op, "TLTIU", mot, pc, "%ns%rs, %ih" );
}

static void TEQI(){
    mr4kd_sprintf( op, "TEQI", mot, pc, "%ns%rs, %ih" );
}

static void TNEI(){
    mr4kd_sprintf( op, "TNEI", mot, pc, "%ns%rs, %ih" );
}

static void BLTZAL(){
    mr4kd_sprintf( op, "BLTZAL", mot, pc, "%ns%rs, %br" );
}

static void BGEZAL(){
    mr4kd_sprintf( op, "BGEZAL", mot, pc, "%ns%rs, %br" );
}

static void BLTZALL(){
    mr4kd_sprintf( op, "BLTZALL", mot, pc, "%ns%rs, %br" );
}

static void BGEZALL(){
    mr4kd_sprintf( op, "BGEZALL", mot, pc, "%ns%rs, %br" );
}

static void regimm()
{
//  sprintf(op, "[01] REGIMM");
    
    switch( (mot>>16) & 0x1F)
    {
        case 0x00: BLTZ();  break;
        case 0x01: BGEZ();  break;
        case 0x02: BLTZL(); break;
        case 0x03: BGEZL(); break;
        case 0x08: TGEI();  break;
        case 0x09: TGEIU(); break;
        case 0x0A: TLTI();  break;
        case 0x0B: TLTIU(); break;
        case 0x0C: TEQI();  break;
        case 0x0E: TNEI();  break;
        case 0x10: BLTZAL();    break;
        case 0x11: BGEZAL();    break;
        case 0x12: BLTZALL();   break;
        case 0x13: BGEZALL();   break;
        default: RESERV();
        }
}


//]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ ... ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[

static void J(){
    mr4kd_sprintf( op, "J", mot, pc, "%ns%jm" );
}

static void JAL(){
    mr4kd_sprintf( op, "JAL", mot, pc, "%ns%jm" );
}

static void BEQ(){
    mr4kd_sprintf( op, "BEQ", mot, pc, "%ns%rs, %rt, %br" );
}

static void BNE(){
    mr4kd_sprintf( op, "BNE", mot, pc, "%ns%rs, %rt, %br" );
}

static void BLEZ(){
    mr4kd_sprintf( op, "BLEZ", mot, pc, "%ns%rs, %br" );
}

static void BGTZ(){
    mr4kd_sprintf( op, "BGTZ", mot, pc, "%ns%rs, %br" );
}

static void ADDI(){
    mr4kd_sprintf( op, "ADDI", mot, pc, "%ns%rt, %rs, %ih" );
}

static void ADDIU(){
    mr4kd_sprintf( op, "ADDIU", mot, pc, "%ns%rt, %rs, %ih" );
}

static void SLTI(){
    mr4kd_sprintf( op, "SLTI", mot, pc, "%ns%rt, %rs, %ih" );
}

static void SLTIU(){
    mr4kd_sprintf( op, "SLTIU", mot, pc, "%ns%rt, %rs, %ih" );
}

static void ANDI(){
    mr4kd_sprintf( op, "ANDI", mot, pc, "%ns%rt, %rs, %ih" );
}

static void ORI(){
    mr4kd_sprintf( op, "ORI", mot, pc, "%ns%rt, %rs, %ih" );
}

static void XORI(){
    mr4kd_sprintf( op, "XORI", mot, pc, "%ns%rt, %rs, %ih" );
}

static void LUI(){
    mr4kd_sprintf( op, "LUI", mot, pc, "%ns%rt, %ih" );
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ cop0 ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void MFC0(){
    mr4kd_sprintf( op, "MFC0", mot, pc, "%ns%rt, %cp" );
}   

static void MTC0(){
    mr4kd_sprintf( op, "MTC0", mot, pc, "%ns%rt, %cp" );
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ tlb ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void TLBR(){   
    mr4kd_sprintf( op, "TLBR", mot, pc, "%n0" );
}   
    
static void TLBWI(){
    mr4kd_sprintf( op, "TLBWI", mot, pc, "%n0" );
}   

static void TLBWR(){
    mr4kd_sprintf( op, "TLBWR", mot, pc, "%n0" );
}   

static void TLBP(){
    mr4kd_sprintf( op, "TLBP", mot, pc, "%n0" );
}   

static void ERET(){
    mr4kd_sprintf( op, "ERET", mot, pc, "%n0" );
}   

static void tlb()
{
//  sprintf(op, "[10] tlb");
    
    switch( mot & 0x3F)
    {
        case 0x01: TLBR()   ; break;
        case 0x02: TLBWI()  ; break;
        case 0x06: TLBWR()  ; break;
        case 0x08: TLBP()   ; break;
        case 0x18: ERET()   ; break;
        default: RESERV();
    }
}

static void cop0()
{
//  sprintf(op, "[10] COP0");
    
    switch( (mot>>21) & 0x1F)
    {
        case 0x00: MFC0()   ; break;
        case 0x04: MTC0()   ; break;
        case 0x10: tlb()    ; break;
        default: RESERV();
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ cop1 ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//
static void MFC1(){
    mr4kd_sprintf( op, "MFC1", mot, pc, "%ns%rt, %fs" );
}

static void DMFC1(){
    mr4kd_sprintf( op, "DMFC1", mot, pc, "%ns%rt, %fs" );
}

static void CFC1(){
    mr4kd_sprintf( op, "CFC1", mot, pc, "%ns%rt, %fs" );
}

static void MTC1(){
    mr4kd_sprintf( op, "MTC1", mot, pc, "%ns%rt, %fs" );
}

static void DMTC1(){
    mr4kd_sprintf( op, "DMTC1", mot, pc, "%ns%rt, %fs" );
}

static void CTC1(){
    mr4kd_sprintf( op, "CTC1", mot, pc, "%ns%rt, %fs" );
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ BC ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void BC1F(){
    mr4kd_sprintf( op, "BC1F", mot, pc, "%ns%br" );
}

static void BC1T(){
    mr4kd_sprintf( op, "BC1T", mot, pc, "%ns%br" );
    
}

static void BC1FL(){
    mr4kd_sprintf( op, "BC1FL", mot, pc, "%ns%br" );
    
}

static void BC1TL(){
    mr4kd_sprintf( op, "BC1TL", mot, pc, "%ns%br" );
}

static void BC()
{
//  sprintf(op, "[11] BC");

    switch( (mot>>16) & 3)
    {
        case 0x00: BC1F();  break;
        case 0x01: BC1T();  break;
        case 0x02: BC1FL(); break;
        case 0x03: BC1TL(); break;
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ S ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void ADD_S(){
    mr4kd_sprintf( op, "ADD.S", mot, pc, "%ns%fd, %fs, %ft" );
}

static void SUB_S(){
    mr4kd_sprintf( op, "SUB.S", mot, pc, "%ns%fd, %fs, %ft" );
}

static void MUL_S(){
    mr4kd_sprintf( op, "MUL.S", mot, pc, "%ns%fd, %fs, %ft" );
}

static void DIV_S(){
    mr4kd_sprintf( op, "DIV.S", mot, pc, "%ns%fd, %fs, %ft" );
}

static void SQRT_S(){
    mr4kd_sprintf( op, "SQRT.S", mot, pc, "%ns%fd, %fs" );
}

static void ABS_S(){
    mr4kd_sprintf( op, "ABS.S", mot, pc, "%ns%fd, %fs" );
}

static void MOV_S(){
    mr4kd_sprintf( op, "MOV.S", mot, pc, "%ns%fd, %fs" );
}

static void NEG_S(){
    mr4kd_sprintf( op, "NEG.S", mot, pc, "%ns%fd, %fs" );
}

static void ROUND_L_S(){
    mr4kd_sprintf( op, "ROUND.L.S", mot, pc, "%ns%fd, %fs" );
}

static void TRUNC_L_S(){
    mr4kd_sprintf( op, "TRUNC.L.S", mot, pc, "%ns%fd, %fs" );
}

static void CEIL_L_S(){
    mr4kd_sprintf( op, "CEIL.L.S", mot, pc, "%ns%fd, %fs" );
}

static void FLOOR_L_S(){
    mr4kd_sprintf( op, "FLOOR.L.S", mot, pc, "%ns%fd, %fs" );
}

static void ROUND_W_S(){
    mr4kd_sprintf( op, "ROUND.W.S", mot, pc, "%ns%fd, %fs" );
}

static void TRUNC_W_S(){
    mr4kd_sprintf( op, "TRUNC.W.S", mot, pc, "%ns%fd, %fs" );
}

static void CEIL_W_S(){
    mr4kd_sprintf( op, "CEIL.W.S", mot, pc, "%ns%fd, %fs" );
}

static void FLOOR_W_S(){
    mr4kd_sprintf( op, "FLOOR.W.S", mot, pc, "%ns%fd, %fs" );
}

static void CVT_D_S(){
    mr4kd_sprintf( op, "CVT.D.S", mot, pc, "%ns%fd, %fs" );
}

static void CVT_W_S(){
    mr4kd_sprintf( op, "CVT.W.S", mot, pc, "%ns%fd, %fs" );
}

static void CVT_L_S(){
    mr4kd_sprintf( op, "CVT.L.S", mot, pc, "%ns%fd, %fs" );
}


static void C_F_S(){
    mr4kd_sprintf( op, "C.F.S", mot, pc, "%ns%fs, %ft" );
}

static void C_UN_S(){
    mr4kd_sprintf( op, "C.UN.S", mot, pc, "%ns%fs, %ft" );
}

static void C_EQ_S(){
    mr4kd_sprintf( op, "C.EQ.S", mot, pc, "%ns%fs, %ft" );
}

static void C_UEQ_S(){
    mr4kd_sprintf( op, "C.UEQ.S", mot, pc, "%ns%fs, %ft" );
}

static void C_OLT_S(){
    mr4kd_sprintf( op, "C.OLT.S", mot, pc, "%ns%fs, %ft" );
}

static void C_ULT_S(){
    mr4kd_sprintf( op, "C.ULT.S", mot, pc, "%ns%fs, %ft" );
}

static void C_OLE_S(){
    mr4kd_sprintf( op, "C.OLE.S", mot, pc, "%ns%fs, %ft" );
}

static void C_ULE_S(){
    mr4kd_sprintf( op, "C.ULE.S", mot, pc, "%ns%fs, %ft" );
}

static void C_SF_S(){
    mr4kd_sprintf( op, "C.SF.S", mot, pc, "%ns%fs, %ft" );
}

static void C_NGLE_S(){
    mr4kd_sprintf( op, "C.NGLE.S", mot, pc, "%ns%fs, %ft" );
}

static void C_SEQ_S(){
    mr4kd_sprintf( op, "C.SEQ.S", mot, pc, "%ns%fs, %ft" );
}

static void C_NGL_S(){
    mr4kd_sprintf( op, "C.NGL.S", mot, pc, "%ns%fs, %ft" );
}

static void C_LT_S(){
    mr4kd_sprintf( op, "C.LT.S", mot, pc, "%ns%fs, %ft" );
}

static void C_NGE_S(){
    mr4kd_sprintf( op, "C.NGE.S", mot, pc, "%ns%fs, %ft" );
}

static void C_LE_S(){
    mr4kd_sprintf( op, "C.LE.S", mot, pc, "%ns%fs, %ft" );
}

static void C_NGT_S(){
    mr4kd_sprintf( op, "C.NGT.S", mot, pc, "%ns%fs, %ft" );
}


static void S()
{
//  sprintf(op, "[11] S");
    
    switch( mot & 0x3F)
    {
        case 0x00: ADD_S(); break;
        case 0x01: SUB_S(); break;
        case 0x02: MUL_S(); break;
        case 0x03: DIV_S(); break;
        case 0x04: SQRT_S();    break;
        case 0x05: ABS_S(); break;
        case 0x06: MOV_S(); break;
        case 0x07: NEG_S(); break;
        case 0x08: ROUND_L_S(); break;
        case 0x09: TRUNC_L_S(); break;
        case 0x0A: CEIL_L_S();  break;
        case 0x0B: FLOOR_L_S(); break;
        case 0x0C: ROUND_W_S(); break;
        case 0x0D: TRUNC_W_S(); break;
        case 0x0E: CEIL_W_S();  break;
        case 0x0F: FLOOR_W_S(); break;
        case 0x21: CVT_D_S();   break;
        case 0x24: CVT_W_S();   break;
        case 0x25: CVT_L_S();   break;
        case 0x30: C_F_S(); break;
        case 0x31: C_UN_S();    break;
        case 0x32: C_EQ_S();    break;
        case 0x33: C_UEQ_S();   break;
        case 0x34: C_OLT_S();   break;
        case 0x35: C_ULT_S();   break;
        case 0x36: C_OLE_S();   break;
        case 0x37: C_ULE_S();   break;
        case 0x38: C_SF_S();    break;
        case 0x39: C_NGLE_S();  break;
        case 0x3A: C_SEQ_S();   break;
        case 0x3B: C_NGL_S();   break;
        case 0x3C: C_LT_S();    break;
        case 0x3D: C_NGE_S();   break;
        case 0x3E: C_LE_S();    break;
        case 0x3F: C_NGT_S();   break;
        default: RESERV();
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ D ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//


static void ADD_D(){
    mr4kd_sprintf( op, "ADD.D", mot, pc, "%ns%fd, %fs, %ft" );
}

static void SUB_D(){
    mr4kd_sprintf( op, "SUB.D", mot, pc, "%ns%fd, %fs, %ft" );
}

static void MUL_D(){
    mr4kd_sprintf( op, "MUL.D", mot, pc, "%ns%fd, %fs, %ft" );
}

static void DIV_D(){
    mr4kd_sprintf( op, "DIV.D", mot, pc, "%ns%fd, %fs, %ft" );
}

static void SQRT_D(){
    mr4kd_sprintf( op, "SQRT.D", mot, pc, "%ns%fd, %fs" );
}

static void ABS_D(){
    mr4kd_sprintf( op, "ABS.D", mot, pc, "%ns%fd, %fs" );
}

static void MOV_D(){
    mr4kd_sprintf( op, "MOV.D", mot, pc, "%ns%fd, %fs" );
}

static void NEG_D(){
    mr4kd_sprintf( op, "NEG.D", mot, pc, "%ns%fd, %fs" );
}

static void ROUND_L_D(){
    mr4kd_sprintf( op, "ROUND.L.D", mot, pc, "%ns%fd, %fs" );
}

static void TRUNC_L_D(){
    mr4kd_sprintf( op, "TRUNC.L.D", mot, pc, "%ns%fd, %fs" );
}

static void CEIL_L_D(){
    mr4kd_sprintf( op, "CEIL.L.D", mot, pc, "%ns%fd, %fs" );
}

static void FLOOR_L_D(){
    mr4kd_sprintf( op, "FLOOR.L.D", mot, pc, "%ns%fd, %fs" );
}

static void ROUND_W_D(){
    mr4kd_sprintf( op, "ROUND.W.D", mot, pc, "%ns%fd, %fs" );
}

static void TRUNC_W_D(){
    mr4kd_sprintf( op, "TRUNC.W.D", mot, pc, "%ns%fd, %fs" );
}

static void CEIL_W_D(){
    mr4kd_sprintf( op, "CEIL.W.D", mot, pc, "%ns%fd, %fs" );
}

static void FLOOR_W_D(){
    mr4kd_sprintf( op, "FLOOR.W.D", mot, pc, "%ns%fd, %fs" );
}

static void CVT_S_D(){
    mr4kd_sprintf( op, "CVT.S.D", mot, pc, "%ns%fd, %fs" );
}

static void CVT_W_D(){
    mr4kd_sprintf( op, "CVT.W.D", mot, pc, "%ns%fd, %fs" );
}

static void CVT_L_D(){
    mr4kd_sprintf( op, "CVT.L.D", mot, pc, "%ns%fd, %fs" );
}



static void C_F_D(){
    mr4kd_sprintf( op, "C.F.D", mot, pc, "%ns%fs, %ft" );
}

static void C_UN_D(){
    mr4kd_sprintf( op, "C.UN.D", mot, pc, "%ns%fs, %ft" );
}

static void C_EQ_D(){
    mr4kd_sprintf( op, "C.EQ.D", mot, pc, "%ns%fs, %ft" );
}

static void C_UEQ_D(){
    mr4kd_sprintf( op, "C.UEQ.D", mot, pc, "%ns%fs, %ft" );
}

static void C_OLT_D(){
    mr4kd_sprintf( op, "C.OLT.D", mot, pc, "%ns%fs, %ft" );
}

static void C_ULT_D(){
    mr4kd_sprintf( op, "C.ULT.D", mot, pc, "%ns%fs, %ft" );
}

static void C_OLE_D(){
    mr4kd_sprintf( op, "C.OLE.D", mot, pc, "%ns%fs, %ft" );
}

static void C_ULE_D(){
    mr4kd_sprintf( op, "C.ULE.D", mot, pc, "%ns%fs, %ft" );
}

static void C_SF_D(){
    mr4kd_sprintf( op, "C.SF.D", mot, pc, "%ns%fs, %ft" );
}

static void C_NGLE_D(){
    mr4kd_sprintf( op, "C.NGLE.D", mot, pc, "%ns%fs, %ft" );
}

static void C_SEQ_D(){
    mr4kd_sprintf( op, "C.SEQ.D", mot, pc, "%ns%fs, %ft" );
}

static void C_NGL_D(){
    mr4kd_sprintf( op, "C.NGL.D", mot, pc, "%ns%fs, %ft" );
}

static void C_LT_D(){
    mr4kd_sprintf( op, "C.LT.D", mot, pc, "%ns%fs, %ft" );
}

static void C_NGE_D(){
    mr4kd_sprintf( op, "C.NGE.D", mot, pc, "%ns%fs, %ft" );
}

static void C_LE_D(){
    mr4kd_sprintf( op, "C.LE.D", mot, pc, "%ns%fs, %ft" );
}

static void C_NGT_D(){
    mr4kd_sprintf( op, "C.NGT.D", mot, pc, "%ns%fs, %ft" );
}


static void D()
{
//  sprintf(op, "[11] D");

    switch( mot & 0x3F)
    {
        case 0x00: ADD_D(); break;
        case 0x01: SUB_D(); break;
        case 0x02: MUL_D(); break;
        case 0x03: DIV_D(); break;
        case 0x04: SQRT_D();    break;
        case 0x05: ABS_D(); break;
        case 0x06: MOV_D(); break;
        case 0x07: NEG_D(); break;
        case 0x08: ROUND_L_D(); break;
        case 0x09: TRUNC_L_D(); break;
        case 0x0A: CEIL_L_D();  break;
        case 0x0B: FLOOR_L_D(); break;
        case 0x0C: ROUND_W_D(); break;
        case 0x0D: TRUNC_W_D(); break;
        case 0x0E: CEIL_W_D();  break;
        case 0x0F: FLOOR_W_D(); break;
        case 0x20: CVT_S_D();   break;
        case 0x24: CVT_W_D();   break;
        case 0x25: CVT_L_D();   break;
        case 0x30: C_F_D(); break;
        case 0x31: C_UN_D();    break;
        case 0x32: C_EQ_D();    break;
        case 0x33: C_UEQ_D();   break;
        case 0x34: C_OLT_D();   break;
        case 0x35: C_ULT_D();   break;
        case 0x36: C_OLE_D();   break;
        case 0x37: C_ULE_D();   break;
        case 0x38: C_SF_D();    break;
        case 0x39: C_NGLE_D();  break;
        case 0x3A: C_SEQ_D();   break;
        case 0x3B: C_NGL_D();   break;
        case 0x3C: C_LT_D();    break;
        case 0x3D: C_NGE_D();   break;
        case 0x3E: C_LE_D();    break;
        case 0x3F: C_NGT_D();   break;
        default: RESERV();
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ W ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void CVT_S_W(){
    mr4kd_sprintf( op, "CVT.S.W", mot, pc, "%ns%fd, %fs" );
}

static void CVT_D_W(){
    mr4kd_sprintf( op, "CVT.D.W", mot, pc, "%ns%fd, %fs" );
}


static void W()
{
//  sprintf(op, "[11] W");
//  sprintf(args, " ");
    
    switch( mot & 0x3F)
    {
        case 0x20: CVT_S_W();   break;
        case 0x21: CVT_D_W();   break;
        default: RESERV();
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ L ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void CVT_S_L(){
    mr4kd_sprintf( op, "CVT.S.L", mot, pc, "%ns%fd, %fs" );
}

static void CVT_D_L(){
    mr4kd_sprintf( op, "CVT.D.L", mot, pc, "%ns%fd, %fs" );
}


static void L(){
//  sprintf(op, "[11] L");
//  sprintf(args, " ");

    switch( mot & 0x3F)
    {
        case 0x20: CVT_S_L();   break;
        case 0x21: CVT_D_L();   break;
        default: RESERV();
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ cop1 ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void cop1()
{
//  sprintf(op, "[11] COP1");
    
    switch( (mot>>21) & 0x1F)
    {
        case 0x00: MFC1();  break;
        case 0x01: DMFC1(); break;
        case 0x02: CFC1();  break;
        case 0x04: MTC1();  break;
        case 0x05: DMTC1(); break;
        case 0x06: CTC1();  break;
        case 0x08: BC();    break;
        case 0x10: S();     break;
        case 0x11: D();     break;
        case 0x14: W();     break;
        case 0x15: L();     break;
        default: RESERV();
    }
}


//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ ... ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//

static void BEQL(){
    mr4kd_sprintf( op, "BEQL", mot, pc, "%ns%rs, %rt, %br" );
}

static void BNEL(){
    mr4kd_sprintf( op, "BNEL", mot, pc, "%ns%rs, %rt, %br" );
}

static void BLEZL(){
    mr4kd_sprintf( op, "BLEZL", mot, pc, "%ns%rs, %br" );
}

static void BGTZL(){
    mr4kd_sprintf( op, "BGTZL", mot, pc, "%ns%rs, %br" );
}


static void DADDI(){
    mr4kd_sprintf( op, "DADDI", mot, pc, "%ns%rt, %rs, %ih" );
}

static void DADDIU(){
    mr4kd_sprintf( op, "DADDIU", mot, pc, "%ns%rt, %rs, %ih" );
}


static void LDL(){
    mr4kd_sprintf( op, "LDL", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LDR(){
    mr4kd_sprintf( op, "LDR", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LB(){
    mr4kd_sprintf( op, "LB", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LH(){
    mr4kd_sprintf( op, "LH", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LWL(){
    mr4kd_sprintf( op, "LWL", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LW(){
    mr4kd_sprintf( op, "LW", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LBU(){
    mr4kd_sprintf( op, "LBU", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LHU(){
    mr4kd_sprintf( op, "LHU", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LWR(){
    mr4kd_sprintf( op, "LWR", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LWU(){
    mr4kd_sprintf( op, "LWU", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SB(){
    mr4kd_sprintf( op, "SB", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SH(){
    mr4kd_sprintf( op, "SH", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SWL(){
    mr4kd_sprintf( op, "SWL", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SW(){
    mr4kd_sprintf( op, "SW", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SDL(){
    mr4kd_sprintf( op, "SDL", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SDR(){
    mr4kd_sprintf( op, "SDR", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SWR(){
    mr4kd_sprintf( op, "SWR", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void CACHE(){
    mr4kd_sprintf( op, "CACHE", mot, pc, "%ns%rt, %ih(%rs)");
}

static void LL(){
    mr4kd_sprintf( op, "LL", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LWC1(){
    mr4kd_sprintf( op, "LWC1", mot, pc, "%ns%ft, %ih(%rs)" );
}

static void LLD(){
    mr4kd_sprintf( op, "LLD", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void LDC1(){
    mr4kd_sprintf( op, "LDC1", mot, pc, "%ns%ft, %ih(%rs)" );
}

static void LD(){
    mr4kd_sprintf( op, "LD", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SC(){
    mr4kd_sprintf( op, "SC", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SWC1(){
    mr4kd_sprintf( op, "SWC1", mot, pc, "%ns%ft, %ih(%rs)" );
}

static void SCD(){
    mr4kd_sprintf( op, "SCD", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SDC1(){
    mr4kd_sprintf( op, "SDC1", mot, pc, "%ns%rt, %ih(%rs)" );
}

static void SD(){
    mr4kd_sprintf( op, "SD", mot, pc, "%ns%rt, %ih(%rs)" );
}



//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[ DECODE_OP ]=-=-=-=-=-=-=-=-=-=-=-=-=-=-=[//
void r4300_decode_op( uint32 instr, char *opcode, char *arguments, int counter )
{
    char buffer[256]; int result;
    mr4kd_disassemble( instr, counter, buffer );
    
    /* Split it up */
    if( (result = sscanf( buffer, "%s %s", opcode, arguments )) == 1 )
        strcpy( arguments, " " );
    else
        strcpy( arguments, buffer + 16);
}

/* Disassemble */
static void mr4kd_disassemble ( uint32 instruction, uint32 counter, char * buffer )
{
    mot = instruction;
    pc  = counter;
    op  = buffer;
    
    switch((mot>>26)&0x3F)
    {
        case 0x00: special();   break;
        case 0x01: regimm();    break;
        case 0x02: J();     break;
        case 0x03: JAL();   break;
        case 0x04: BEQ();   break;
        case 0x05: BNE();   break;
        case 0x06: BLEZ();  break;
        case 0x07: BGTZ();  break;
        case 0x08: ADDI();  break;
        case 0x09: ADDIU(); break;
        case 0x0A: SLTI();  break;
        case 0x0B: SLTIU(); break;
        case 0x0C: ANDI();  break;
        case 0x0D: ORI();   break;
        case 0x0E: XORI();  break;
        case 0x0F: LUI();   break;
        case 0x10: cop0();  break;
        case 0x11: cop1();  break;
        case 0x14: BEQL();  break;
        case 0x15: BNEL();  break;
        case 0x16: BLEZL(); break;
        case 0x17: BGTZL(); break;
        case 0x18: DADDI(); break;
        case 0x19: DADDIU();    break;
        case 0x1A: LDL();   break;
        case 0x1B: LDR();   break;
        case 0x20: LB();    break;
        case 0x21: LH();    break;
        case 0x22: LWL();   break;
        case 0x23: LW();    break;
        case 0x24: LBU();   break;
        case 0x25: LHU();   break;
        case 0x26: LWR();   break;
        case 0x27: LWU();   break;
        case 0x28: SB();    break;
        case 0x29: SH();    break;
        case 0x2A: SWL();   break;
        case 0x2B: SW();    break;
        case 0x2C: SDL();   break;
        case 0x2D: SDR();   break;
        case 0x2E: SWR();   break;
        case 0x2F: CACHE(); break;
        case 0x30: LL();    break;
        case 0x31: LWC1();  break;
        case 0x34: LLD();   break;
        case 0x35: LDC1();  break;
        case 0x37: LD();    break;
        case 0x38: SC();    break;
        case 0x39: SWC1();  break;
        case 0x3C: SCD();   break;
        case 0x3D: SDC1();  break;
        case 0x3F: SD();    break;
        default: RESERV();
    }
}

