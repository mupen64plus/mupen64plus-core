/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - dma.c                                                   *
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

#include <stdlib.h>
#include <stdio.h>

#include "dma.h"
#include "memory.h"
#include "pif.h"
#include "flashram.h"

#include "../r4300/r4300.h"
#include "../r4300/interupt.h"
#include "../r4300/macros.h"
#include "../r4300/ops.h"

#include "../main/config.h"
#include "../main/main.h"
#include "../main/rom.h"

static unsigned char sram[0x8000];

void dma_pi_read()
{
    int i;
   
    if (pi_register.pi_cart_addr_reg >= 0x08000000
    && pi_register.pi_cart_addr_reg < 0x08010000)
    {
        if (use_flashram != 1)
        {
            char *filename;
            FILE *f;
            filename = malloc(strlen(get_savespath())+
            strlen(ROM_SETTINGS.goodname)+4+1);
            strcpy(filename, get_savespath());
            strcat(filename, ROM_SETTINGS.goodname);
            strcat(filename, ".sra");
            f = fopen(filename, "rb");
            if (f)
            {
                fread(sram, 1, 0x8000, f);
                fclose(f);
            }
            else
            {
                for (i=0; i<0x8000; i++)
                {
                    sram[i] = 0;
                }
            }
            for (i=0; i < (pi_register.pi_rd_len_reg & 0xFFFFFF)+1; i++)
            {
                sram[((pi_register.pi_cart_addr_reg-0x08000000)+i)^S8] =
                ((unsigned char*)rdram)[(pi_register.pi_dram_addr_reg+i)^S8];
            }
            f = fopen(filename, "wb");
            if (f == NULL)
            {
                 printf("Warning: couldn't open flash ram file '%s' for writing.\n", filename);
            }
            else
            {
                fwrite(sram, 1, 0x8000, f);
                fclose(f);
            }
            free(filename);
            use_flashram = -1;
        }
        else
        {
            dma_write_flashram();
        }
    }
    else
    {
        printf("unknown dma read\n");
    }
    
    pi_register.read_pi_status_reg |= 1;
    update_count();
    add_interupt_event(PI_INT, 0x1000/*pi_register.pi_rd_len_reg*/);
}

void dma_pi_write()
{
    unsigned int longueur;
    int i;
    
    if (pi_register.pi_cart_addr_reg < 0x10000000)
    {
        if (pi_register.pi_cart_addr_reg >= 0x08000000 
        && pi_register.pi_cart_addr_reg < 0x08010000)
        {
            if (use_flashram != 1)
            {
                char *filename;
                FILE *f;
                int i;
                
                filename = malloc(strlen(get_savespath())+
                strlen(ROM_SETTINGS.goodname)+4+1);
                strcpy(filename, get_savespath());
                strcat(filename, ROM_SETTINGS.goodname);
                strcat(filename, ".sra");
                f = fopen(filename, "rb");
                
                if (f)
                {
                    fread(sram, 1, 0x8000, f);
                    fclose(f);
                }
                else
                {
                    for (i=0; i<0x8000; i++)
                    {
                        sram[i] = 0x0;
                    }
                }
                
                free(filename);
                
                for (i=0; i<(pi_register.pi_wr_len_reg & 0xFFFFFF)+1; i++)
                {
                    ((unsigned char*)rdram)[(pi_register.pi_dram_addr_reg+i)^S8]=
                    sram[(((pi_register.pi_cart_addr_reg-0x08000000)&0xFFFF)+i)^S8];
                }
                
                use_flashram = -1;
            }
            else
            {
                dma_read_flashram();
            }
        }
        else if (pi_register.pi_cart_addr_reg >= 0x06000000
               && pi_register.pi_cart_addr_reg < 0x08000000)
        {
        }
        else
        {
            printf("unknown dma write:%x\n", (int)pi_register.pi_cart_addr_reg);
        }
        
        pi_register.read_pi_status_reg |= 1;
        update_count();
        add_interupt_event(PI_INT, /*pi_register.pi_wr_len_reg*/0x1000);
        
        return;
    }

    if (pi_register.pi_cart_addr_reg >= 0x1fc00000) // for paper mario
    {
        pi_register.read_pi_status_reg |= 1;
        update_count();
        add_interupt_event(PI_INT, 0x1000);
        
        return;
    }

    longueur = (pi_register.pi_wr_len_reg & 0xFFFFFF)+1;
    i = (pi_register.pi_cart_addr_reg-0x10000000)&0x3FFFFFF;
    longueur = (i + longueur) > taille_rom ?
    (taille_rom - i) : longueur;
    longueur = (pi_register.pi_dram_addr_reg + longueur) > 0x7FFFFF ?
    (0x7FFFFF - pi_register.pi_dram_addr_reg) : longueur;

    if(i>taille_rom || pi_register.pi_dram_addr_reg > 0x7FFFFF)
    {
        pi_register.read_pi_status_reg |= 3;
        update_count();
        add_interupt_event(PI_INT, longueur/8);
        
        return;
    }
    
    if(!interpcore)
    {
        for (i=0; i<longueur; i++)
        {
            unsigned long rdram_address1 = pi_register.pi_dram_addr_reg+i+0x80000000;
            unsigned long rdram_address2 = pi_register.pi_dram_addr_reg+i+0xa0000000;
            ((unsigned char*)rdram)[(pi_register.pi_dram_addr_reg+i)^S8]=
            rom[(((pi_register.pi_cart_addr_reg-0x10000000)&0x3FFFFFF)+i)^S8];

            if(!invalid_code[rdram_address1>>12])
            {
                if(blocks[rdram_address1>>12]->block[(rdram_address1&0xFFF)/4].ops != NOTCOMPILED)
                {
                    invalid_code[rdram_address1>>12] = 1;
                }
            }
            if(!invalid_code[rdram_address2>>12])
            {
                if(blocks[rdram_address2>>12]->block[(rdram_address2&0xFFF)/4].ops != NOTCOMPILED)
                {
                    invalid_code[rdram_address2>>12] = 1;
                }
            }
        }
    }
    else
    {
        for (i=0; i<longueur; i++)
        {
            ((unsigned char*)rdram)[(pi_register.pi_dram_addr_reg+i)^S8]=
            rom[(((pi_register.pi_cart_addr_reg-0x10000000)&0x3FFFFFF)+i)^S8];
        }
    }

    if ((debug_count+Count) < 0x100000)
    {
        
        switch(CIC_Chip)
        {
            case 1:
            case 2:
            case 3:
            case 6:
            {
                if ( config_get_bool( "NoMemoryExpansion", 0 ) )
                {
                    rdram[0x318/4] = 0x400000;
                }
                else
                {
                    rdram[0x318/4] = 0x800000;
                }
                break;
            }
            case 5:
            {
                if ( config_get_bool( "NoMemoryExpansion", 0 ) )
                {
                    rdram[0x3F0/4] = 0x400000;
                }
                else
                {
                    rdram[0x3F0/4] = 0x800000;
                }
                break;
            }
        }
    }

    pi_register.read_pi_status_reg |= 3;
    update_count();
    add_interupt_event(PI_INT, longueur/8);
    
    return;
}
    
void dma_sp_write()
{
    int i;
    
    if ((sp_register.sp_mem_addr_reg & 0x1000) > 0)
    {
        for (i=0; i<((sp_register.sp_rd_len_reg & 0xFFF)+1); i++)
        {
            ((unsigned char *)(SP_IMEM))[((sp_register.sp_mem_addr_reg & 0xFFF)+i)^S8]=
            ((unsigned char *)(rdram))[((sp_register.sp_dram_addr_reg & 0xFFFFFF)+i)^S8];
        }
    }
    else
    {
        for (i=0; i<((sp_register.sp_rd_len_reg & 0xFFF)+1); i++)
        {
            ((unsigned char *)(SP_DMEM))[((sp_register.sp_mem_addr_reg & 0xFFF)+i)^S8]=
            ((unsigned char *)(rdram))[((sp_register.sp_dram_addr_reg & 0xFFFFFF)+i)^S8];
        }
    }
}

void dma_sp_read()
{
    int i;
    
    if ((sp_register.sp_mem_addr_reg & 0x1000) > 0)
    {
        for (i=0; i<((sp_register.sp_wr_len_reg & 0xFFF)+1); i++)
        {
            ((unsigned char *)(rdram))[((sp_register.sp_dram_addr_reg & 0xFFFFFF)+i)^S8]=
            ((unsigned char *)(SP_IMEM))[((sp_register.sp_mem_addr_reg & 0xFFF)+i)^S8];
        }
    }
    else
    {
        for (i=0; i<((sp_register.sp_wr_len_reg & 0xFFF)+1); i++)
        {
            ((unsigned char *)(rdram))[((sp_register.sp_dram_addr_reg & 0xFFFFFF)+i)^S8]=
            ((unsigned char *)(SP_DMEM))[((sp_register.sp_mem_addr_reg & 0xFFF)+i)^S8];
        }
    }
}

void dma_si_write()
{
    int i;
    
    if (si_register.si_pif_addr_wr64b != 0x1FC007C0)
    {
        printf("unknown SI use\n");
        stop=1;
    }
    
    for (i=0; i<(64/4); i++)
    {
        PIF_RAM[i] = sl(rdram[si_register.si_dram_addr/4+i]);
    }
    
    update_pif_write();
    update_count();
    add_interupt_event(SI_INT, /*0x100*/0x900);
}

void dma_si_read()
{
    int i;
    
    if (si_register.si_pif_addr_rd64b != 0x1FC007C0)
    {
        printf("unknown SI use\n");
        stop=1;
    }
    
    update_pif_read();
    
    for (i=0; i<(64/4); i++)
    {
        rdram[si_register.si_dram_addr/4+i] = sl(PIF_RAM[i]);
    }
    
    update_count();
    add_interupt_event(SI_INT, /*0x100*/0x900);
}

