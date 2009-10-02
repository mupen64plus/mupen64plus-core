/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - opprintf.c                                              *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2008 ZZT32                                              *
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
#include <ctype.h>

#include "opprintf.h"

/* Command Access Macros */
#define _OPCODE(x)      (((x) >> 26) & 0x3F)
#define _RS(x)          (((x) >> 21) & 0x1F)
#define _RT(x)          (((x) >> 16) & 0x1F)
#define _RD(x)          (((x) >> 11) & 0x1F)
#define _FS(x)          (_RD(x))
#define _FT(x)          (_RT(x))
#define _FD(x)          (((x) >> 6) & 0x1F)
#define _BASE(x)        (_RS(x))
#define _IMMEDIATE(x)   ((x) & 0xFFFF)
#define _OFFSET(x)      (_IMMEDIATE(x))
#define _B_OFFSET(x)    (((s16)_IMMEDIATE(x)) * (s16)4)
#define _TARGET_RAW(x)  ((x) & 0x3FFFFFF)
#define _TARGET(x)      (_TARGET_RAW(x) << 2)
#define _FUNC(x)        ((x) & 0x3F)
#define _SHAM(x)        (((x) >> 6) & 0x1F)
#define _FMT(x)         (_RS(x))
#define _COP(x)         ((x) >> 26 & 3)

/* FMT modes */
static char fpu_fmt_names[] =
{
    'S', 'D', /* single/double */
    '?', '?', /* reserved */
    'W', 'L'  /* word/longword */
};

/* Floating point condition */
static char *fp_compare_types[] =
{
    "F", "UN", "EQ", "UEQ", "OLT", "ULT", 
    "OLE", "ULE", "SF", "NGLE", "SEQ",  "NGL", 
    "LT", "NGE", "LE", "NGT"
};

/* General purpose register names */
static char *registers_a_gpr[] =
{
    "R0", "AT", "V0", "V1",
    "A0", "A1", "A2", "A3",
    "T0", "T1", "T2", "T3",
    "T4", "T5", "T6", "T7",
    "S0", "S1", "S2", "S3",
    "S4", "S5", "S6", "S7",
    "T8", "T9", "K0", "K1",
    "GP", "SP", "S8", "RA"
};

/* Floating point register names */
static char *registers_a_fpr[] =
{
    "F0",  "F1",  "F2",  "F3",
    "F4",  "F5",  "F6",  "F7",
    "F8",  "F9",  "F10", "F11",
    "F12", "F13", "F14", "F15",
    "F16", "F17", "F18", "F19",
    "F20", "F21", "F22", "F23",
    "F24", "F25", "F26", "F27",
    "F28", "F29", "F30", "F31"
};

/* Coprocessor register names */
static char *registers_a_cop[] =
{
    "Index",    "Random",   "EntryLo0",
    "EntryLo1", "Context",  "PageMask",
    "Wired",    "Resrvd07", "BadVAddr",
    "Count",    "EntryHi",  "Compare",
    "Status",   "Cause",    "EPC", "PRevID",
    "Config",   "LLAddr",   "WatchLo",
    "WatchHi",  "XContext", "Resrvd21",
    "Resrvd22", "Resrvd23", "Resrvd24", 
    "Resrvd25", "PErr",     "CacheErr", "TagLo", 
    "TagHi",    "ErrorEPC", "Resrvd31"
};

/* The configuration word */
static long mr4kd_conf = 
    MR4KD_SPACING(16) | MR4KD_RPREFIX | MR4KD_RLOWER | MR4KD_OLOWER;

/* Copy a register name to dest */
static int mr4kd_rcpy_gpr ( char *dest, int reg )
{
    int i = 0, s = 0;
    
    /* Do we put a '$' ? */
    if( mr4kd_conf & MR4KD_RPREFIX )
        dest[i++] = '$';
    
    for( ; registers_a_gpr[reg][s]; i++, s++ )
        dest[i] = ( (mr4kd_conf & MR4KD_RLOWER) ? tolower(registers_a_gpr[reg][s]) : registers_a_gpr[reg][s] );
    dest[i] = 0;
    
    return i;
}

/* Copy a register name to dest */
static int mr4kd_rcpy_fpr ( char *dest, int reg )
{
    int i = 0, s = 0;
    
    /* Do we put a '$' ? */
    if( mr4kd_conf & MR4KD_RPREFIX )
        dest[i++] = '$';
    
    for(; registers_a_fpr[reg][s]; i++, s++ )
        dest[i] = ( (mr4kd_conf & MR4KD_RLOWER) ? tolower(registers_a_fpr[reg][s]) : registers_a_fpr[reg][s] );
    dest[i] = 0;
    
    return i;
}

#define TOKEN(x, y) ((x) << 8 | (y))

/* Give some pretty, formatted output */
int mr4kd_sprintf ( char *dest, char *name, uint32 instruction, uint32 pc, char *fmt )
{
    /* 
    ** Format specifiers:
    **  - %rs
    **  - %rt
    **  - %rd
    **
    **  - %fs
    **  - %ft
    **  - %fd
    **
    **  - %cp   Coprocessor register
    **
    **  - %ff   FP mode (single, double etc)
    **  - %fc   FP compare condition
    **
    **  - %ih   Immediate (hex)
    **  - %id   Immediate (dec)
    **  - %br   Branch address
    **  - %jm   Jump target
    **
    **  - %co   COP #
    **
    **  - %ns   Name
    **  - %nc   Name with COP number
    **  - %nf   Name with FP format
    **
    **  - %sa   Shift amount
    **
    **  - %SP   Remainder spacing
    */
    
    int s = 0; /* Source */
    int d = 0; /* Dest   */
    
    uint16 token;
    
    /* Scan the format string for specifiers */
    for( s = 0; fmt[s]; s++ )
    {
        /* No token? */
        if( fmt[s] != '%' )
        {
            dest[d++] = fmt[s];
            continue;
        }
        
        /* Got token, read it in */
        token = (fmt[s + 1] << 8 | fmt[s + 2]);
        
        /* Check */
        switch( token )
        {
            /* GPRs */
            case TOKEN('r', 's'): d += mr4kd_rcpy_gpr( &dest[d], _RS(instruction) ); break;
            case TOKEN('r', 't'): d += mr4kd_rcpy_gpr( &dest[d], _RT(instruction) ); break;
            case TOKEN('r', 'd'): d += mr4kd_rcpy_gpr( &dest[d], _RD(instruction) ); break;
            
            /* FPRs */
            case TOKEN('f', 's'): d += mr4kd_rcpy_fpr( &dest[d], _FS(instruction) ); break;
            case TOKEN('f', 't'): d += mr4kd_rcpy_fpr( &dest[d], _FT(instruction) ); break;
            case TOKEN('f', 'd'): d += mr4kd_rcpy_fpr( &dest[d], _FD(instruction) ); break;
            
            /* COP */
            case TOKEN('c', 'p'): d += sprintf( &dest[d], "%s", registers_a_cop[_FS(instruction)] ); break;
            
            /* Immediate (hex) */
            case TOKEN('i', 'h'):
             d += sprintf( &dest[d], MR4KD_FLAG_GET(MR4KD_HLOWER) ? "0x%04x" : "0x%04X", _IMMEDIATE(instruction) );
            break;
             
            /* Immediate (decimal) */
            case TOKEN('i', 'd'):
             d += sprintf( &dest[d], "%i", (int)((short)_IMMEDIATE(instruction)) );
            break;
            
            /* Branch address */
            case TOKEN('b', 'r'):
             d += sprintf( &dest[d], MR4KD_FLAG_GET(MR4KD_HLOWER) ? "0x%08x" : "0x%08X", ((unsigned int)pc + 4 + (short)(instruction & 0xFFFF) * 4) );
            break;
            
            /* Jump target */
            case TOKEN('j', 'm'):
             d += sprintf( &dest[d], MR4KD_FLAG_GET(MR4KD_HLOWER) ? "0x%08x" : "0x%08X", _TARGET(instruction) | 0x80000000 );
            break;
            
            /* Coprocessor number */
            case TOKEN('c', 'o'):
             d += sprintf( &dest[d], "%u", _COP(instruction) );
            break;
            
            /* Shift amount */
            case TOKEN('s', 'a'):
             d += sprintf( &dest[d], "%u", _SHAM(instruction) );
            break;
            
            /* Floating point compare mode */
            case TOKEN('f', 'c'):
             d += sprintf( &dest[d], "%s", fp_compare_types[instruction & 0xF] );
            break;
            
            /* Floating point mode */
            case TOKEN('f', 'f'):
             d += sprintf( &dest[d], "%c", fpu_fmt_names[_FMT(instruction) - 16] );
            break;
            
            /* Spacing! */
            case TOKEN('S', 'P'):
             d += sprintf( &dest[d], "%*s", -((mr4kd_conf >> 24) - d), " ");
            break;
            
            /* Instruction name - regular (no space) */
            case TOKEN('n', '0'):
            {
                int i; char nb[32];
                
                /* Convert to the proper case */
                for( i = 0; name[i]; i++ )
                    nb[i] = ( mr4kd_conf & MR4KD_OLOWER ) ? tolower(name[i]) : toupper(name[i]);
                
                /* Copy it to dest */
                d += sprintf( &dest[d], "%.*s", i, nb );
            }
            break;
            
            /* Instruction name - regular */
            case TOKEN('n', 's'):
            {
                int i; char nb[32];
                
                /* Convert to the proper case */
                for( i = 0; name[i]; i++ )
                    nb[i] = ( mr4kd_conf & MR4KD_OLOWER ) ? tolower(name[i]) : toupper(name[i]);
                
                /* Copy it to dest */
                d += sprintf( &dest[d], "%*.*s", -(mr4kd_conf >> 24), i, nb );
            }
            break;
            
            /* Instruction name - with COP# */
            case TOKEN('n', 'c'):
            {
                int i; char nb[32];
                
                /* Convert to the proper case */
                for( i = 0; name[i]; i++ )
                    nb[i] = ( mr4kd_conf & MR4KD_OLOWER ) ? tolower(name[i]) : toupper(name[i]);
                
                /* Copy number */
                i += sprintf( &nb[i], "%u", _COP(instruction) );
                
                /* Copy it to dest */
                d += sprintf( &dest[d], "%*.*s", -(mr4kd_conf >> 24), i, nb );
            }
            break;
            
            /* Floating point format */
            case TOKEN('n', 'f'):
            {
                int i; char nb[32];
                
                /* Convert to the proper case */
                for( i = 0; name[i]; i++ )
                    nb[i] = ( mr4kd_conf & MR4KD_OLOWER ) ? tolower(name[i]) : toupper(name[i]);
                
                /* Copy the precision mode */
                i += sprintf( &nb[i], ".%c",( mr4kd_conf & MR4KD_OLOWER ) ?
                    tolower(fpu_fmt_names[_FMT(instruction)-16]) :
                    toupper(fpu_fmt_names[_FMT(instruction)-16]) );
                
                /* Copy it to dest */
                d += sprintf( &dest[d], "%*.*s", -(mr4kd_conf >> 24), i, nb );
            }
            break;
        }
        
        /* Update source pointer */
        s += 2;
    }
    
    /* Null terminate output */
    dest[d] = 0;
    
    /* Return chars processed */
    return d;
}

/* Set a flag */
void mr4kd_flag_set ( int flag )
{
    MR4KD_FLAG_SET( flag );
}

/* Clear a flag */
void mr4kd_flag_clear ( int flag )
{
    MR4KD_FLAG_CLEAR( flag );
}

/* Get flag status */
int mr4kd_flag_get ( int flag )
{
    return (mr4kd_conf & flag);
}

/* Set spacing (between op & operands) */
void mr4kd_spacing ( int space )
{
    MR4KD_SET_SPACE( space );
}

