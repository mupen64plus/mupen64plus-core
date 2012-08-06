/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus - savestates.c                                            *
 *   Mupen64Plus homepage: http://code.google.com/p/mupen64plus/           *
 *   Copyright (C) 2012 CasualJames                                        *
 *   Copyright (C) 2009 Olejl Tillin9                                      *
 *   Copyright (C) 2008 Richard42 Tillin9                                  *
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
#include <string.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "api/m64p_config.h"
#include "api/config.h"

#include "savestates.h"
#include "main.h"
#include "rom.h"
#include "util.h"

#include "memory/memory.h"
#include "memory/flashram.h"
#include "memory/tlb.h"
#include "r4300/macros.h"
#include "r4300/r4300.h"
#include "r4300/interupt.h"
#include "osal/preproc.h"
#include "osd/osd.h"

#ifdef LIBMINIZIP
    #include <unzip.h>
    #include <zip.h>
#else
    #include "main/zip/unzip.h"
    #include "main/zip/zip.h"
#endif

static const char* savestate_magic = "M64+SAVE";
static const int savestate_latest_version = 0x00010000;  /* 1.0 */
static const int pj64_magic = 0x23D8A6C8;

static savestates_job job = savestates_job_nothing;
static savestates_type type = savestates_type_unknown;
static char *fname = NULL;

static unsigned int slot = 0;
static int autoinc_save_slot = 0;

typedef struct _TLB_pj64
{
    unsigned int _EntryDefined;

    struct _BreakDownPageMask
    {
        unsigned int zero : 13;
        unsigned int Mask : 12;
        unsigned int zero2 : 7;
    } BreakDownPageMask;

    struct _BreakDownEntryHi
    {
        unsigned int ASID : 8;
        unsigned int Zero : 4;
        unsigned int G : 1;
        unsigned int VPN2 : 19;
    } BreakDownEntryHi;

    struct _BreakDownEntryLo0 
    {
        unsigned int GLOBAL: 1;
        unsigned int V : 1;
        unsigned int D : 1;
        unsigned int C : 3;
        unsigned int PFN : 20;
        unsigned int ZERO: 6;
    } BreakDownEntryLo0;

    struct _BreakDownEntryLo1 
    {
        unsigned int GLOBAL: 1;
        unsigned int V : 1;
        unsigned int D : 1;
        unsigned int C : 3;
        unsigned int PFN : 20;
        unsigned int ZERO: 6;
    } BreakDownEntryLo1;
} TLB_pj64;

/* Returns the malloc'd full path of the currently selected savestate. */
static char *savestates_generate_path(savestates_type type)
{
    if(fname != NULL) /* A specific path was given. */
    {
        return strdup(fname);
    }
    else /* Use the selected savestate slot */
    {
        char *filename;
        switch (type)
        {
            case savestates_type_m64p:
                filename = formatstr("%s.st%d", ROM_SETTINGS.goodname, slot);
                break;
            case savestates_type_pj64_zip:
                filename = formatstr("%s.pj%d.zip", ROM_PARAMS.headername, slot);
                break;
            case savestates_type_pj64_unc:
                filename = formatstr("%s.pj%d", ROM_PARAMS.headername, slot);
                break;
            default:
                filename = NULL;
                break;
        }

        if (filename != NULL)
        {
            char *filepath = formatstr("%s%s", get_savestatepath(), filename);
            free(filename);
            return filepath;
        }
        else
            return NULL;
    }
}

void savestates_select_slot(unsigned int s)
{
    if(s>9||s==slot)
        return;
    slot = s;
    ConfigSetParameter(g_CoreConfig, "CurrentSaveSlot", M64TYPE_INT, &s);
    StateChanged(M64CORE_SAVESTATE_SLOT, slot);

    if(rom)
    {
        char* filepath = savestates_generate_path(savestates_type_m64p);
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Selected state file: %s", namefrompath(filepath));
        free(filepath);
    }
    else
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Selected state slot: %d", slot);
}

/* Returns the currently selected save slot. */
unsigned int savestates_get_slot(void)
{
    return slot;
}

/* Sets save state slot autoincrement on or off. */
void savestates_set_autoinc_slot(int b)
{
    autoinc_save_slot = b;
}

void savestates_inc_slot(void)
{
    if(++slot>9)
        slot = 0;
    StateChanged(M64CORE_SAVESTATE_SLOT, slot);
}

savestates_job savestates_get_job(void)
{
    return job;
}

void savestates_set_job(savestates_job j, savestates_type t, const char *fn)
{
    if (fname != NULL)
    {
        free(fname);
        fname = NULL;
    }

    job = j;
    type = t;
    if (fn != NULL)
        fname = strdup(fn);
}

void savestates_clear_job(void)
{
    savestates_set_job(savestates_job_nothing, savestates_type_unknown, NULL);
}

static int savestates_load_m64p(char *filepath)
{
    char buffer[1024];
    unsigned char inbuf[4];
    gzFile f;
    int queuelength, version;

    f = gzopen(filepath, "rb");

    if(f==NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not open state file: %s", filepath);
        return 0;
    }

    /* Read and check Mupen64Plus magic number. */
    gzread(f, buffer, 8);
    if(strncmp(buffer, savestate_magic, 8)!=0)
        {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State file: %s is not a valid Mupen64plus savestate.", filepath);
        gzclose(f);
        return 0;
        }

    /* Read savestate file version in big-endian order. */
    gzread(f, inbuf, 4);
    version = inbuf[0];
    version = (version << 8) | inbuf[1];
    version = (version << 8) | inbuf[2];
    version = (version << 8) | inbuf[3];
    if(version != 0x00010000)
        {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State version (%08x) isn't compatible. Please update Mupen64Plus.", version);
        gzclose(f);
        return 0;
        }

    gzread(f, buffer, 32);
    if(memcmp(buffer, ROM_SETTINGS.MD5, 32))
        {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State ROM MD5 does not match current ROM.");
        gzclose(f);
        return 0;
        }

    gzread(f, &rdram_register, sizeof(RDRAM_register));
    gzread(f, &MI_register, sizeof(mips_register));
    gzread(f, &pi_register, sizeof(PI_register));
    gzread(f, &sp_register, sizeof(SP_register));
    gzread(f, &rsp_register, sizeof(RSP_register));
    gzread(f, &si_register, sizeof(SI_register));
    gzread(f, &vi_register, sizeof(VI_register));
    update_vi_status(vi_register.vi_status);
    update_vi_width(vi_register.vi_width);
    gzread(f, &ri_register, sizeof(RI_register));
    gzread(f, &ai_register, sizeof(AI_register));
    update_ai_dacrate(ai_register.ai_dacrate);
    gzread(f, &dpc_register, sizeof(DPC_register));
    gzread(f, &dps_register, sizeof(DPS_register));
    gzread(f, rdram, 0x800000);
    gzread(f, SP_DMEM, 0x1000);
    gzread(f, SP_IMEM, 0x1000);
    gzread(f, PIF_RAM, 0x40);

    gzread(f, buffer, 24);
    load_flashram_infos(buffer);

    gzread(f, tlb_LUT_r, 0x100000*4);
    gzread(f, tlb_LUT_w, 0x100000*4);

    gzread(f, &llbit, 4);
    gzread(f, reg, 32*8);
    gzread(f, reg_cop0, 32*4);
    set_fpr_pointers(Status);  // Status is reg_cop0[12]
    gzread(f, &lo, 8);
    gzread(f, &hi, 8);
    gzread(f, reg_cop1_fgr_64, 32*8);
    if ((Status & 0x04000000) == 0)  // 32-bit FPR mode requires data shuffling because 64-bit layout is always stored in savestate file
        shuffle_fpr_data(0x04000000, 0);
    gzread(f, &FCR0, 4);
    gzread(f, &FCR31, 4);
    gzread(f, tlb_e, 32*sizeof(tlb));
    if(r4300emu == CORE_PURE_INTERPRETER)
        gzread(f, &interp_addr, 4);
    else
        {
        int i;
        gzread(f, &queuelength, 4);
        for (i = 0; i < 0x100000; i++)
            invalid_code[i] = 1;
        jump_to(queuelength);
        }

    gzread(f, &next_interupt, 4);
    gzread(f, &next_vi, 4);
    gzread(f, &vi_field, 4);

    queuelength = 0;
    while(1)
        {
        gzread(f, buffer+queuelength, 4);
        if(*((unsigned int*)&buffer[queuelength])==0xFFFFFFFF)
            break;
        gzread(f, buffer+queuelength+4, 4);
        queuelength += 8;
        }
    load_eventqueue_infos(buffer);

    gzclose(f);
    if(r4300emu == CORE_PURE_INTERPRETER)
        last_addr = interp_addr;
    else
        last_addr = PC->addr;

    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State loaded from: %s", namefrompath(filepath));

    return 1;
}

static int savestates_load_pj64(char *filepath, void *handle,
                                int (*read_func)(void *, void *, size_t))
{
    char buffer[1024], RomHeader[64];
    unsigned int magic, value, vi_timer, SaveRDRAMSize;
    int i;
    TLB_pj64 tlb_pj64;

    /* Read and check Project64 magic number. */
    read_func(handle, &magic, 4);
    if (magic!=pj64_magic)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State file: %s is not a valid Project64 savestate. Unrecognized file format.", filepath);
        return 0;
    }

    read_func(handle, &SaveRDRAMSize, 4);

    read_func(handle, RomHeader, 0x40);
    if(memcmp(RomHeader, rom, 0x40)!=0) 
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State ROM header does not match current ROM.");
        return 0;
    }

    // vi_timer
    read_func(handle, &vi_timer,4);

    // Program Counter
    read_func(handle, &last_addr, 4);

    // GPR
    read_func(handle, reg,8*32);

    // FPR
    read_func(handle, reg_cop1_fgr_64,8*32);

    // CP0
    read_func(handle, reg_cop0, 4*32);

    set_fpr_pointers(Status);  // Status is reg_cop0[12]
    if ((Status & 0x04000000) == 0) // TODO not sure how pj64 handles this
        shuffle_fpr_data(0x04000000, 0);

    // Initialze the interupts
    vi_timer += reg_cop0[9]; // Add current Count
    next_interupt = (Compare < vi_timer) ? Compare : vi_timer;
    next_vi = vi_timer;
    vi_field = 0;
    *((unsigned int*)&buffer[0]) = VI_INT;
    *((unsigned int*)&buffer[4]) = vi_timer;
    *((unsigned int*)&buffer[8]) = COMPARE_INT;
    *((unsigned int*)&buffer[12]) = Compare;
    *((unsigned int*)&buffer[16]) = 0xFFFFFFFF;

    load_eventqueue_infos(buffer);

    // FPCR
    read_func(handle, &FCR0,4);
    read_func(handle, &buffer,120);   // Dummy read.
    read_func(handle, &FCR31,4);

    // hi / lo
    read_func(handle,&hi,8);
    read_func(handle,&lo,8);

    // rdram register
    read_func(handle, &rdram_register, sizeof(RDRAM_register));

    // sp_register
    read_func(handle, &sp_register.sp_mem_addr_reg, 4);
    read_func(handle, &sp_register.sp_dram_addr_reg, 4);
    read_func(handle, &sp_register.sp_rd_len_reg, 4);
    read_func(handle, &sp_register.sp_wr_len_reg, 4);
    read_func(handle, &sp_register.sp_status_reg, 4);
    read_func(handle, &sp_register.sp_dma_full_reg, 4);
    read_func(handle, &sp_register.sp_dma_busy_reg, 4);
    read_func(handle, &sp_register.sp_semaphore_reg, 4);
    read_func(handle, &rsp_register.rsp_pc, 4);
    read_func(handle, &rsp_register.rsp_ibist, 4);

    make_w_sp_status_reg();

    // dpc_register
    read_func(handle, &dpc_register.dpc_start, 4);
    read_func(handle, &dpc_register.dpc_end, 4);
    read_func(handle, &dpc_register.dpc_current, 4);
    read_func(handle, &dpc_register.dpc_status, 4);
    read_func(handle, &dpc_register.dpc_clock, 4);
    read_func(handle, &dpc_register.dpc_bufbusy, 4);
    read_func(handle, &dpc_register.dpc_pipebusy, 4);
    read_func(handle, &dpc_register.dpc_tmem, 4);
    read_func(handle, &value, 4); // Dummy read
    read_func(handle, &value, 4); // Dummy read

    make_w_dpc_status();

    // mi_register
    read_func(handle, &MI_register.mi_init_mode_reg, 4);
    read_func(handle, &MI_register.mi_version_reg, 4);
    read_func(handle, &MI_register.mi_intr_reg, 4);
    read_func(handle, &MI_register.mi_intr_mask_reg, 4);

    make_w_mi_init_mode_reg();
    make_w_mi_intr_mask_reg();

    // vi_register 
    read_func(handle, &vi_register, 4*14);
    update_vi_status(vi_register.vi_status);
    update_vi_width(vi_register.vi_width);

    // ai_register
    read_func(handle, &ai_register, 4*6);
    update_ai_dacrate(ai_register.ai_dacrate);

    // pi_register
    read_func(handle, &pi_register, sizeof(PI_register));

    // ri_register
    read_func(handle, &ri_register, sizeof(RI_register));

    // si_register
    read_func(handle, &si_register, sizeof(SI_register));

    // tlb
    memset(tlb_LUT_r, 0, 0x400000);
    memset(tlb_LUT_w, 0, 0x400000);
    for (i=0; i < 32; i++)
    {
        read_func(handle, &tlb_pj64, sizeof(TLB_pj64));
        tlb_e[i].mask = (short) tlb_pj64.BreakDownPageMask.Mask;
        tlb_e[i].vpn2 = tlb_pj64.BreakDownEntryHi.VPN2;
        tlb_e[i].g = (char) tlb_pj64.BreakDownEntryLo0.GLOBAL & tlb_pj64.BreakDownEntryLo1.GLOBAL;
        tlb_e[i].asid = (unsigned char) tlb_pj64.BreakDownEntryHi.ASID;
        tlb_e[i].pfn_even = tlb_pj64.BreakDownEntryLo0.PFN;
        tlb_e[i].c_even = (char) tlb_pj64.BreakDownEntryLo0.C;
        tlb_e[i].d_even = (char) tlb_pj64.BreakDownEntryLo0.D;
        tlb_e[i].v_even = (char) tlb_pj64.BreakDownEntryLo0.V;
        tlb_e[i].pfn_odd = tlb_pj64.BreakDownEntryLo1.PFN;
        tlb_e[i].c_odd = (char) tlb_pj64.BreakDownEntryLo1.C;
        tlb_e[i].d_odd = (char) tlb_pj64.BreakDownEntryLo1.D;
        tlb_e[i].v_odd = (char) tlb_pj64.BreakDownEntryLo1.V;

        // This is copied from TLBWI instruction
        // tlb_e[i].r = 0;
        tlb_e[i].start_even = (unsigned int) tlb_e[i].vpn2 << 13;
        tlb_e[i].end_even = (unsigned int) tlb_e[i].start_even + (tlb_e[i].mask << 12) + 0xFFF;
        tlb_e[i].phys_even = (unsigned int) tlb_e[i].pfn_even << 12;;
        tlb_e[i].start_odd = (unsigned int) tlb_e[i].end_even + 1;
        tlb_e[i].end_odd = (unsigned int) tlb_e[i].start_odd + (tlb_e[i].mask << 12) + 0xFFF;;
        tlb_e[i].phys_odd = (unsigned int) tlb_e[i].pfn_odd << 12;

        tlb_map(&tlb_e[i]);
    }

    // pif ram
    read_func(handle, PIF_RAM, 0x40);

    // RDRAM
    memset(rdram, 0, 0x800000);
    read_func(handle, rdram, SaveRDRAMSize);

    // DMEM
    read_func(handle, SP_DMEM, 0x1000);

    // IMEM
    read_func(handle, SP_IMEM, 0x1000);

    // The following values should not matter because we don't have any AI interrupt
    // ai_register.next_delay = 0; ai_register.next_len = 0;
    // ai_register.current_delay = 0; ai_register.current_len = 0;

    // The following is not available in PJ64 savestate. Keep the values as is.
    // dps_register.dps_tbist = 0; dps_register.dps_test_mode = 0;
    // dps_register.dps_buftest_addr = 0; dps_register.dps_buftest_data = 0; llbit = 0;

    // No flashram info in pj64 savestate.
    init_flashram();

    if(r4300emu == CORE_PURE_INTERPRETER)
        interp_addr = last_addr;
    else
    {
        for (i = 0; i < 0x100000; i++)
            invalid_code[i] = 1;
        jump_to(last_addr);
    }

    return 1;
}

static int read_data_from_zip(void *zip, void *buffer, size_t length)
{
    return unzReadCurrentFile((unzFile)zip, buffer, (unsigned)length) == length;
}

static int savestates_load_pj64_zip(char *filepath)
{
    char szFileName[256], szExtraField[256], szComment[256];
    unzFile zipstatefile = NULL;
    unz_file_info fileinfo;
    int ret = 0;

    /* Open the .zip file. */
    zipstatefile = unzOpen(filepath);
    if (zipstatefile == NULL ||
        unzGoToFirstFile(zipstatefile) != UNZ_OK ||
        unzGetCurrentFileInfo(zipstatefile, &fileinfo, szFileName, 255, szExtraField, 255, szComment, 255) != UNZ_OK ||
        unzOpenCurrentFile(zipstatefile) != UNZ_OK)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Zip error. Could not open state file: %s", filepath);
        goto clean_and_exit;
    }

    if (!savestates_load_pj64(filepath, zipstatefile, read_data_from_zip))
        goto clean_and_exit;

    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State loaded from: %s", namefrompath(filepath));
    ret = 1;

    clean_and_exit:
        if (zipstatefile != NULL)
            unzClose(zipstatefile);
        return ret;
}

static int read_data_from_file(void *file, void *buffer, size_t length)
{
    return fread(buffer, 1, length, file) == length;
}

static int savestates_load_pj64_unc(char *filepath)
{
    FILE *f;

    /* Open the file. */
    f = fopen(filepath, "rb");
    if (f == NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not open state file: %s", filepath);
        return 0;
    }

    if (!savestates_load_pj64(filepath, f, read_data_from_file))
    {
        fclose(f);
        return 0;
    }

    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State loaded from: %s", namefrompath(filepath));
    fclose(f);
    return 1;
}

savestates_type savestates_detect_type(char *filepath)
{
    char magic[4];
    FILE *f = fopen(filepath, "rb");
    if (f == NULL)
    {
        DebugMessage(M64MSG_STATUS, "Could not open state file %s\n", filepath);
        return savestates_type_unknown;
    }

    if (fread(magic, 1, 4, f) != 4)
    {
        fclose(f);
        DebugMessage(M64MSG_STATUS, "Could not read from state file %s\n", filepath);
        return savestates_type_unknown;
    }

    fclose(f);

    if (magic[0] == 0x1f && magic[1] == 0x8b) // GZIP header
        return savestates_type_m64p;
    else if (strncmp(magic, "PK\x03\x04", 4) == 0) // ZIP header
        return savestates_type_pj64_zip;
    else if (*((int *)magic) == pj64_magic) // PJ64 header
        return savestates_type_pj64_unc;
    else
    {
        DebugMessage(M64MSG_STATUS, "Unknown state file type %s\n", filepath);
        return savestates_type_unknown;
    }
}

int savestates_load(void)
{
    char *filepath;
    int ret = 0;

    if (fname != NULL && type == savestates_type_unknown)
        type = savestates_detect_type(fname);
    else if (fname == NULL) // Always load slots in M64P format
        type = savestates_type_m64p;

    filepath = savestates_generate_path(type);
    if (filepath != NULL)
    {
        switch (type)
        {
            case savestates_type_m64p: ret = savestates_load_m64p(filepath); break;
            case savestates_type_pj64_zip: ret = savestates_load_pj64_zip(filepath); break;
            case savestates_type_pj64_unc: ret = savestates_load_pj64_unc(filepath); break;
            default: ret = 0; break;
        }
        free(filepath);
    }

    savestates_clear_job();

    return ret;
}

static void savestates_save_m64p(char *filepath)
{
    char buffer[1024];
    unsigned char outbuf[4];
    gzFile f;
    int queuelength;

    if(autoinc_save_slot)
        savestates_inc_slot();

    f = gzopen(filepath, "wb");

    if (f==NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not open state file: %s", filepath);
        return;
    }

    /* Write magic number. */
    gzwrite(f, savestate_magic, 8);

    /* Write savestate file version in big-endian. */
    outbuf[0] = (savestate_latest_version >> 24) & 0xff;
    outbuf[1] = (savestate_latest_version >> 16) & 0xff;
    outbuf[2] = (savestate_latest_version >>  8) & 0xff;
    outbuf[3] = (savestate_latest_version >>  0) & 0xff;
    gzwrite(f, outbuf, 4);

    gzwrite(f, ROM_SETTINGS.MD5, 32);

    gzwrite(f, &rdram_register, sizeof(RDRAM_register));
    gzwrite(f, &MI_register, sizeof(mips_register));
    gzwrite(f, &pi_register, sizeof(PI_register));
    gzwrite(f, &sp_register, sizeof(SP_register));
    gzwrite(f, &rsp_register, sizeof(RSP_register));
    gzwrite(f, &si_register, sizeof(SI_register));
    gzwrite(f, &vi_register, sizeof(VI_register));
    gzwrite(f, &ri_register, sizeof(RI_register));
    gzwrite(f, &ai_register, sizeof(AI_register));
    gzwrite(f, &dpc_register, sizeof(DPC_register));
    gzwrite(f, &dps_register, sizeof(DPS_register));
    gzwrite(f, rdram, 0x800000);
    gzwrite(f, SP_DMEM, 0x1000);
    gzwrite(f, SP_IMEM, 0x1000);
    gzwrite(f, PIF_RAM, 0x40);

    save_flashram_infos(buffer);
    gzwrite(f, buffer, 24);

    gzwrite(f, tlb_LUT_r, 0x100000*4);
    gzwrite(f, tlb_LUT_w, 0x100000*4);

    gzwrite(f, &llbit, 4);
    gzwrite(f, reg, 32*8);
    gzwrite(f, reg_cop0, 32*4);
    gzwrite(f, &lo, 8);
    gzwrite(f, &hi, 8);

    if ((Status & 0x04000000) == 0)
    {   // FR bit == 0 means 32-bit (MIPS I) FGR mode
        shuffle_fpr_data(0, 0x04000000);  // shuffle data into 64-bit register format for storage
        gzwrite(f, reg_cop1_fgr_64, 32*8);
        shuffle_fpr_data(0x04000000, 0);  // put it back in 32-bit mode
    }
    else
    {
        gzwrite(f, reg_cop1_fgr_64, 32*8);
    }

    gzwrite(f, &FCR0, 4);
    gzwrite(f, &FCR31, 4);
    gzwrite(f, tlb_e, 32*sizeof(tlb));
    if(r4300emu == CORE_PURE_INTERPRETER)
        gzwrite(f, &interp_addr, 4);
    else
        gzwrite(f, &PC->addr, 4);

    gzwrite(f, &next_interupt, 4);
    gzwrite(f, &next_vi, 4);
    gzwrite(f, &vi_field, 4);

    queuelength = save_eventqueue_infos(buffer);
    gzwrite(f, buffer, queuelength);

    gzclose(f);
    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Saved state to: %s", namefrompath(filepath));
}

static int savestates_save_pj64(char *filepath, void *handle,
                                int (*write_func)(void *, const void *, size_t))
{
    // TODO fpr shuffle
    unsigned int i, vi_timer, addr;
    TLB_pj64 tlb_pj64[32];
    unsigned int dummy = 0;
    unsigned int SaveRDRAMSize = 0x800000;

    vi_timer = get_event(VI_INT) - reg_cop0[9]; /* Subtract current Count according to how PJ64 stores the timer. */

    if(r4300emu == CORE_PURE_INTERPRETER)
        addr = interp_addr;
    else
        addr = PC->addr;

    for (i=0; i < 32;i++)
    {
        tlb_pj64[i].BreakDownPageMask.Mask   = (unsigned int) tlb_e[i].mask;
        tlb_pj64[i].BreakDownEntryHi.VPN2    = (unsigned int) tlb_e[i].vpn2;
        tlb_pj64[i].BreakDownEntryLo0.GLOBAL = (unsigned int) tlb_e[i].g;
        tlb_pj64[i].BreakDownEntryLo1.GLOBAL = (unsigned int) tlb_e[i].g;
        tlb_pj64[i].BreakDownEntryHi.ASID    = (unsigned int) tlb_e[i].asid;
        tlb_pj64[i].BreakDownEntryLo0.PFN    = (unsigned int) tlb_e[i].pfn_even;
        tlb_pj64[i].BreakDownEntryLo0.C      = (unsigned int) tlb_e[i].c_even;
        tlb_pj64[i].BreakDownEntryLo0.D      = (unsigned int) tlb_e[i].d_even;
        tlb_pj64[i].BreakDownEntryLo0.V      = (unsigned int) tlb_e[i].v_even;
        tlb_pj64[i].BreakDownEntryLo1.PFN    = (unsigned int) tlb_e[i].pfn_odd;
        tlb_pj64[i].BreakDownEntryLo1.C      = (unsigned int) tlb_e[i].c_odd;
        tlb_pj64[i].BreakDownEntryLo1.D      = (unsigned int) tlb_e[i].d_odd;
        tlb_pj64[i].BreakDownEntryLo1.V      = (unsigned int) tlb_e[i].v_odd;
    }

    if ((Status & 0x04000000) == 0) // TODO not sure how pj64 handles this
        shuffle_fpr_data(0x04000000, 0);

    if (!write_func(handle, &pj64_magic,                     4) ||
        !write_func(handle, &SaveRDRAMSize,                  4) ||
        !write_func(handle, rom,                             0x40) || 
        !write_func(handle, &vi_timer,                       4) || 
        !write_func(handle, &addr,                           4) || 
        !write_func(handle, reg,                             32*8) || 
        !write_func(handle, reg_cop1_fgr_64,                 32*8) || 
        !write_func(handle, reg_cop0,                        32*4) || 
        !write_func(handle, &FCR0,                           4) || 
        !write_func(handle, &dummy,                          4*30) || 
        !write_func(handle, &FCR31,                          4) || 
        !write_func(handle, &hi,                             8) || 
        !write_func(handle, &lo,                             8) || 
        !write_func(handle, &rdram_register,                 sizeof(RDRAM_register)) || 
        !write_func(handle, &sp_register.sp_mem_addr_reg,    4) || 
        !write_func(handle, &sp_register.sp_dram_addr_reg,   4) || 
        !write_func(handle, &sp_register.sp_rd_len_reg,      4) || 
        !write_func(handle, &sp_register.sp_wr_len_reg,      4) || 
        !write_func(handle, &sp_register.sp_status_reg,      4) || 
        !write_func(handle, &sp_register.sp_dma_full_reg,    4) || 
        !write_func(handle, &sp_register.sp_dma_busy_reg,    4) || 
        !write_func(handle, &sp_register.sp_semaphore_reg,   4) || 
        !write_func(handle, &rsp_register.rsp_pc,            4) ||
        !write_func(handle, &rsp_register.rsp_ibist,         4) ||
        !write_func(handle, &dpc_register.dpc_start,         4) || 
        !write_func(handle, &dpc_register.dpc_end,           4) || 
        !write_func(handle, &dpc_register.dpc_current,       4) || 
        !write_func(handle, &dpc_register.dpc_status,        4) || 
        !write_func(handle, &dpc_register.dpc_clock,         4) || 
        !write_func(handle, &dpc_register.dpc_bufbusy,       4) || 
        !write_func(handle, &dpc_register.dpc_pipebusy,      4) || 
        !write_func(handle, &dpc_register.dpc_tmem,          4) || 
        !write_func(handle, &dummy,                          4) ||  // ?
        !write_func(handle, &dummy,                          4) ||  // ?
        !write_func(handle, &MI_register.mi_init_mode_reg,   4) ||  //TODO Secial handling in pj64
        !write_func(handle, &MI_register.mi_version_reg,     4) || 
        !write_func(handle, &MI_register.mi_intr_reg,        4) || 
        !write_func(handle, &MI_register.mi_intr_mask_reg,   4) || 
        !write_func(handle, &vi_register,                    4*14) || 
        !write_func(handle, &ai_register,                    4*6) || 
        !write_func(handle, &pi_register,                    sizeof(PI_register)) || 
        !write_func(handle, &ri_register,                    sizeof(RI_register)) || 
        !write_func(handle, &si_register,                    sizeof(SI_register)) || 
        !write_func(handle, tlb_pj64,                        sizeof(TLB_pj64)*32) || 
        !write_func(handle, PIF_RAM,                         0x40) || 
        !write_func(handle, rdram,                           0x800000) || 
        !write_func(handle, SP_DMEM,                         0x1000) || 
        !write_func(handle, SP_IMEM,                         0x1000))
    {
        return 0;
    }

    if ((Status & 0x04000000) == 0) // TODO not sure how pj64 handles this
        shuffle_fpr_data(0x04000000, 0);

    return 1;
}

static int write_data_to_zip(void *zip, const void *buffer, size_t length)
{
    return zipWriteInFileInZip((zipFile)zip, buffer, (unsigned)length) == ZIP_OK;
}

static int savestates_save_pj64_zip(char *filepath)
{
    int retval;
    zipFile zipfile = NULL;

    zipfile = zipOpen(filepath, APPEND_STATUS_CREATE);
    if(zipfile == NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not create PJ64 state file: %s", filepath);
        goto clean_and_exit;
    }

    retval = zipOpenNewFileInZip(zipfile, namefrompath(filepath), NULL, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION);
    if(retval != ZIP_OK)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Zip error. Could not create state file: %s", filepath);
        goto clean_and_exit;
    }

    if (!savestates_save_pj64(filepath, zipfile, write_data_to_zip))
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Zip error. Could not write state file %s", filepath);
        goto clean_and_exit;
    }

    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Saved state to: %s", namefrompath(filepath));

    clean_and_exit:
        if (zipfile != NULL)
        {
            zipCloseFileInZip(zipfile); // This may fail, but we don't care
            zipClose(zipfile, "");
        }
        return 1;
}

static int write_data_to_file(void *file, const void *buffer, size_t length)
{
    return fwrite(buffer, 1, length, (FILE *)file) == length;
}

static int savestates_save_pj64_unc(char *filepath)
{
    FILE *f;

    f = fopen(filepath, "wb");
    if (f == NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not create PJ64 state file: %s", filepath);
        return 0;
    }

    if (!savestates_save_pj64(filepath, f, write_data_to_file))
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "File write error. Could not write state file %s", filepath);
        fclose(f);
        return 0;
    }

    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Saved state to: %s", namefrompath(filepath));
    fclose(f);
    return 1;
}

int savestates_save(void)
{
    char *filepath;
    int ret = 0;

    /* Can only save PJ64 savestates on VI / COMPARE interrupt.
       Otherwise try again in a little while. */
    if ((type == savestates_type_pj64_zip ||
         type == savestates_type_pj64_unc) &&
        get_next_event_type() > COMPARE_INT)
        return 0;

    if (fname != NULL && type == savestates_type_unknown)
        type = savestates_type_m64p;
    else if (fname == NULL) // Always save slots in M64P format
        type = savestates_type_m64p;

    filepath = savestates_generate_path(type);
    if (filepath != NULL)
    {
        switch (type)
        {
            case savestates_type_m64p: savestates_save_m64p(filepath); ret = 1; break;
            case savestates_type_pj64_zip: ret = savestates_save_pj64_zip(filepath); break;
            case savestates_type_pj64_unc: ret = savestates_save_pj64_unc(filepath); break;
            default: ret = 0; break;
        }
        free(filepath);
    }

    savestates_clear_job();
    return ret;
}