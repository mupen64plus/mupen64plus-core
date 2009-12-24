/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - memory.h                                                *
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

#ifndef MEMORY_H
#define MEMORY_H

int init_memory(int DoByteSwap);
void free_memory();
#define read_word_in_memory() readmem[address>>16]()
#define read_byte_in_memory() readmemb[address>>16]()
#define read_hword_in_memory() readmemh[address>>16]()
#define read_dword_in_memory() readmemd[address>>16]()
#define write_word_in_memory() writemem[address>>16]()
#define write_byte_in_memory() writememb[address >>16]()
#define write_hword_in_memory() writememh[address >>16]()
#define write_dword_in_memory() writememd[address >>16]()
extern unsigned int SP_DMEM[0x1000/4*2];
extern unsigned char *SP_DMEMb;
extern unsigned int *SP_IMEM;
extern unsigned char *SP_IMEMb;
extern unsigned int PIF_RAM[0x40/4];
extern unsigned char *PIF_RAMb;
extern unsigned int rdram[0x800000/4];
extern unsigned int address, word;
extern unsigned char cpu_byte;
extern unsigned short hword;
extern unsigned long long dword, *rdword;

extern void (*readmem[0x10000])();
extern void (*readmemb[0x10000])();
extern void (*readmemh[0x10000])();
extern void (*readmemd[0x10000])();
extern void (*writemem[0x10000])();
extern void (*writememb[0x10000])();
extern void (*writememh[0x10000])();
extern void (*writememd[0x10000])();

typedef struct _RDRAM_register
{
   unsigned int rdram_config;
   unsigned int rdram_device_id;
   unsigned int rdram_delay;
   unsigned int rdram_mode;
   unsigned int rdram_ref_interval;
   unsigned int rdram_ref_row;
   unsigned int rdram_ras_interval;
   unsigned int rdram_min_interval;
   unsigned int rdram_addr_select;
   unsigned int rdram_device_manuf;
} RDRAM_register;

typedef struct _SP_register
{
   unsigned int sp_mem_addr_reg;
   unsigned int sp_dram_addr_reg;
   unsigned int sp_rd_len_reg;
   unsigned int sp_wr_len_reg;
   unsigned int w_sp_status_reg;
   unsigned int sp_status_reg;
   char halt;
   char broke;
   char dma_busy;
   char dma_full;
   char io_full;
   char single_step;
   char intr_break;
   char signal0;
   char signal1;
   char signal2;
   char signal3;
   char signal4;
   char signal5;
   char signal6;
   char signal7;
   unsigned int sp_dma_full_reg;
   unsigned int sp_dma_busy_reg;
   unsigned int sp_semaphore_reg;
} SP_register;

typedef struct _RSP_register
{
   unsigned int rsp_pc;
   unsigned int rsp_ibist;
} RSP_register;

typedef struct _DPC_register
{
   unsigned int dpc_start;
   unsigned int dpc_end;
   unsigned int dpc_current;
   unsigned int w_dpc_status;
   unsigned int dpc_status;
   char xbus_dmem_dma;
   char freeze;
   char flush;
   char start_glck;
   char tmem_busy;
   char pipe_busy;
   char cmd_busy;
   char cbuf_busy;
   char dma_busy;
   char end_valid;
   char start_valid;
   unsigned int dpc_clock;
   unsigned int dpc_bufbusy;
   unsigned int dpc_pipebusy;
   unsigned int dpc_tmem;
} DPC_register;

typedef struct _DPS_register
{
   unsigned int dps_tbist;
   unsigned int dps_test_mode;
   unsigned int dps_buftest_addr;
   unsigned int dps_buftest_data;
} DPS_register;

typedef struct _mips_register
{
   unsigned int w_mi_init_mode_reg;
   unsigned int mi_init_mode_reg;
   char init_length;
   char init_mode;
   char ebus_test_mode;
   char RDRAM_reg_mode;
   unsigned int mi_version_reg;
   unsigned int mi_intr_reg;
   unsigned int mi_intr_mask_reg;
   unsigned int w_mi_intr_mask_reg;
   char SP_intr_mask;
   char SI_intr_mask;
   char AI_intr_mask;
   char VI_intr_mask;
   char PI_intr_mask;
   char DP_intr_mask;
} mips_register;

typedef struct _VI_register
{
   unsigned int vi_status;
   unsigned int vi_origin;
   unsigned int vi_width;
   unsigned int vi_v_intr;
   unsigned int vi_current;
   unsigned int vi_burst;
   unsigned int vi_v_sync;
   unsigned int vi_h_sync;
   unsigned int vi_leap;
   unsigned int vi_h_start;
   unsigned int vi_v_start;
   unsigned int vi_v_burst;
   unsigned int vi_x_scale;
   unsigned int vi_y_scale;
   unsigned int vi_delay;
} VI_register;

typedef struct _AI_register
{
   unsigned int ai_dram_addr;
   unsigned int ai_len;
   unsigned int ai_control;
   unsigned int ai_status;
   unsigned int ai_dacrate;
   unsigned int ai_bitrate;
   unsigned int next_delay;
   unsigned int next_len;
   unsigned int current_delay;
   unsigned int current_len;
} AI_register;

typedef struct _PI_register
{
   unsigned int pi_dram_addr_reg;
   unsigned int pi_cart_addr_reg;
   unsigned int pi_rd_len_reg;
   unsigned int pi_wr_len_reg;
   unsigned int read_pi_status_reg;
   unsigned int pi_bsd_dom1_lat_reg;
   unsigned int pi_bsd_dom1_pwd_reg;
   unsigned int pi_bsd_dom1_pgs_reg;
   unsigned int pi_bsd_dom1_rls_reg;
   unsigned int pi_bsd_dom2_lat_reg;
   unsigned int pi_bsd_dom2_pwd_reg;
   unsigned int pi_bsd_dom2_pgs_reg;
   unsigned int pi_bsd_dom2_rls_reg;
} PI_register;

typedef struct _RI_register
{
   unsigned int ri_mode;
   unsigned int ri_config;
   unsigned int ri_current_load;
   unsigned int ri_select;
   unsigned int ri_refresh;
   unsigned int ri_latency;
   unsigned int ri_error;
   unsigned int ri_werror;
} RI_register;

typedef struct _SI_register
{
   unsigned int si_dram_addr;
   unsigned int si_pif_addr_rd64b;
   unsigned int si_pif_addr_wr64b;
   unsigned int si_stat;
} SI_register;

extern RDRAM_register rdram_register;
extern PI_register pi_register;
extern mips_register MI_register;
extern SP_register sp_register;
extern SI_register si_register;
extern VI_register vi_register;
extern RSP_register rsp_register;
extern RI_register ri_register;
extern AI_register ai_register;
extern DPC_register dpc_register;
extern DPS_register dps_register;

extern unsigned char *rdramb;

#ifndef _BIG_ENDIAN
#define sl(mot) \
( \
((mot & 0x000000FF) << 24) | \
((mot & 0x0000FF00) <<  8) | \
((mot & 0x00FF0000) >>  8) | \
((mot & 0xFF000000) >> 24) \
)

#define S8 3
#define S16 2
#define Sh16 1

#else

#define sl(mot) mot
#define S8 0
#define S16 0
#define Sh16 0

#endif

void read_nothing();
void read_nothingh();
void read_nothingb();
void read_nothingd();
void read_nomem();
void read_nomemb();
void read_nomemh();
void read_nomemd();
void read_rdram();
void read_rdramb();
void read_rdramh();
void read_rdramd();
void read_rdramFB();
void read_rdramFBb();
void read_rdramFBh();
void read_rdramFBd();
void read_rdramreg();
void read_rdramregb();
void read_rdramregh();
void read_rdramregd();
void read_rsp_mem();
void read_rsp_memb();
void read_rsp_memh();
void read_rsp_memd();
void read_rsp_reg();
void read_rsp_regb();
void read_rsp_regh();
void read_rsp_regd();
void read_rsp();
void read_rspb();
void read_rsph();
void read_rspd();
void read_dp();
void read_dpb();
void read_dph();
void read_dpd();
void read_dps();
void read_dpsb();
void read_dpsh();
void read_dpsd();
void read_mi();
void read_mib();
void read_mih();
void read_mid();
void read_vi();
void read_vib();
void read_vih();
void read_vid();
void read_ai();
void read_aib();
void read_aih();
void read_aid();
void read_pi();
void read_pib();
void read_pih();
void read_pid();
void read_ri();
void read_rib();
void read_rih();
void read_rid();
void read_si();
void read_sib();
void read_sih();
void read_sid();
void read_flashram_status();
void read_flashram_statusb();
void read_flashram_statush();
void read_flashram_statusd();
void read_rom();
void read_romb();
void read_romh();
void read_romd();
void read_pif();
void read_pifb();
void read_pifh();
void read_pifd();

void write_nothing();
void write_nothingb();
void write_nothingh();
void write_nothingd();
void write_nomem();
void write_nomemb();
void write_nomemd();
void write_nomemh();
void write_rdram();
void write_rdramb();
void write_rdramh();
void write_rdramd();
void write_rdramFB();
void write_rdramFBb();
void write_rdramFBh();
void write_rdramFBd();
void write_rdramreg();
void write_rdramregb();
void write_rdramregh();
void write_rdramregd();
void write_rsp_mem();
void write_rsp_memb();
void write_rsp_memh();
void write_rsp_memd();
void write_rsp_reg();
void write_rsp_regb();
void write_rsp_regh();
void write_rsp_regd();
void write_rsp();
void write_rspb();
void write_rsph();
void write_rspd();
void write_dp();
void write_dpb();
void write_dph();
void write_dpd();
void write_dps();
void write_dpsb();
void write_dpsh();
void write_dpsd();
void write_mi();
void write_mib();
void write_mih();
void write_mid();
void write_vi();
void write_vib();
void write_vih();
void write_vid();
void write_ai();
void write_aib();
void write_aih();
void write_aid();
void write_pi();
void write_pib();
void write_pih();
void write_pid();
void write_ri();
void write_rib();
void write_rih();
void write_rid();
void write_si();
void write_sib();
void write_sih();
void write_sid();
void write_flashram_dummy();
void write_flashram_dummyb();
void write_flashram_dummyh();
void write_flashram_dummyd();
void write_flashram_command();
void write_flashram_commandb();
void write_flashram_commandh();
void write_flashram_commandd();
void write_rom();
void write_pif();
void write_pifb();
void write_pifh();
void write_pifd();

void update_SP();
void update_DPC();
void update_MI_init_mode_reg();
void update_MI_intr_mode_reg();
void update_MI_init_mask_reg();
void update_MI_intr_mask_reg();
void update_ai_dacrate(unsigned int word);
void update_vi_status(unsigned int word);
void update_vi_width(unsigned int word);

#endif

