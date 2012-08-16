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
static const unsigned char pj64_magic[4] = { 0xC8, 0xA6, 0xD8, 0x23 };

static savestates_job job = savestates_job_nothing;
static savestates_type type = savestates_type_unknown;
static char *fname = NULL;

static unsigned int slot = 0;
static int autoinc_save_slot = 0;

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


static void block_endian_swap(void *buffer, size_t length, size_t count)
{
    size_t i;
    if (length == 2)
    {
        unsigned short *pun = (unsigned short *)buffer;
        for (i = 0; i < count; i++)
            pun[i] = (pun[i] >> 8) || (pun[i] << 8);
    }
    else if (length == 4)
    {
        unsigned int *pun = (unsigned int *)buffer;
        for (i = 0; i < count; i++)
            pun[i] = __builtin_bswap32(pun[i]);
    }
    else if (length == 8)
    {
        unsigned long long *pun = (unsigned long long *)buffer;
        for (i = 0; i < count; i++)
            pun[i] = __builtin_bswap64(pun[i]);
    }
}

static void to_little_endian(void *buffer, size_t length, size_t count)
{
    #ifdef M64P_BIG_ENDIAN
    block_endian_swap(buffer, length, count);
    #endif
}

#define GETARRAY(buff, type, count) \
    (to_little_endian(buff, sizeof(type),count), \
     buff += count*sizeof(type), \
     (type *)(buff-count*sizeof(type)))
#define COPYARRAY(dst, buff, type, count) \
    memcpy(dst, GETARRAY(buff, type, count), sizeof(type)*count)
#define GETDATA(buff, type) *GETARRAY(buff, type, 1)

#define PUTARRAY(src, buff, type, count) \
    memcpy(buff, src, sizeof(type)*count); \
    to_little_endian(buff, sizeof(type), count); \
    buff += count*sizeof(type);

#define PUTDATA(buff, type, value) \
    do { type x = value; PUTARRAY(&x, buff, type, 1); } while(0)


static int savestates_load_m64p(char *filepath)
{
    unsigned char header[44];
    gzFile f;
    int version;
    int i;

    size_t savestateSize;
    unsigned char *savestateData, *curr;
    char queue[1024];

    f = gzopen(filepath, "rb");
    if(f==NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not open state file: %s", filepath);
        return 0;
    }

    /* Read and check Mupen64Plus magic number. */
    if (gzread(f, header, 44) != 44)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not read header from state file %s", filepath);
        gzclose(f);
        return 0;
    }
    curr = header;

    if(strncmp((char *)curr, savestate_magic, 8)!=0)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State file: %s is not a valid Mupen64plus savestate.", filepath);
        gzclose(f);
        return 0;
    }
    curr += 8;

    version = *curr++;
    version = (version << 8) | *curr++;
    version = (version << 8) | *curr++;
    version = (version << 8) | *curr++;
    if(version != 0x00010000)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State version (%08x) isn't compatible. Please update Mupen64Plus.", version);
        gzclose(f);
        return 0;
    }

    if(memcmp((char *)curr, ROM_SETTINGS.MD5, 32))
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State ROM MD5 does not match current ROM.");
        gzclose(f);
        return 0;
    }
    curr += 32;

    /* Read the rest of the savestate */
    savestateSize = 16788244;
    savestateData = curr = (unsigned char *)malloc(savestateSize);
    if (savestateData == NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Insufficient memory to load state.");
        gzclose(f);
        return 0;
    }
    if (gzread(f, savestateData, savestateSize) != savestateSize ||
        (gzread(f, queue, sizeof(queue)) % 4) != 0)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not read Mupen64Plus savestate data from %s", filepath);
        free(savestateData);
        gzclose(f);
        return 0;
    }

    gzclose(f);

    // Parse savestate
    rdram_register.rdram_config = GETDATA(curr, unsigned int);
    rdram_register.rdram_device_id = GETDATA(curr, unsigned int);
    rdram_register.rdram_delay = GETDATA(curr, unsigned int);
    rdram_register.rdram_mode = GETDATA(curr, unsigned int);
    rdram_register.rdram_ref_interval = GETDATA(curr, unsigned int);
    rdram_register.rdram_ref_row = GETDATA(curr, unsigned int);
    rdram_register.rdram_ras_interval = GETDATA(curr, unsigned int);
    rdram_register.rdram_min_interval = GETDATA(curr, unsigned int);
    rdram_register.rdram_addr_select = GETDATA(curr, unsigned int);
    rdram_register.rdram_device_manuf = GETDATA(curr, unsigned int);

    MI_register.w_mi_init_mode_reg = GETDATA(curr, unsigned int);
    MI_register.mi_init_mode_reg = GETDATA(curr, unsigned int);
    MI_register.init_length = GETDATA(curr, unsigned char);
    MI_register.init_mode = GETDATA(curr, unsigned char);
    MI_register.ebus_test_mode = GETDATA(curr, unsigned char);
    MI_register.RDRAM_reg_mode = GETDATA(curr, unsigned char);
    MI_register.mi_version_reg = GETDATA(curr, unsigned int);
    MI_register.mi_intr_reg = GETDATA(curr, unsigned int);
    MI_register.mi_intr_mask_reg = GETDATA(curr, unsigned int);
    MI_register.w_mi_intr_mask_reg = GETDATA(curr, unsigned int);
    MI_register.SP_intr_mask = GETDATA(curr, unsigned char);
    MI_register.SI_intr_mask = GETDATA(curr, unsigned char);
    MI_register.AI_intr_mask = GETDATA(curr, unsigned char);
    MI_register.VI_intr_mask = GETDATA(curr, unsigned char);
    MI_register.PI_intr_mask = GETDATA(curr, unsigned char);
    MI_register.DP_intr_mask = GETDATA(curr, unsigned char);
    curr += 2; // Padding from old implementation

    pi_register.pi_dram_addr_reg = GETDATA(curr, unsigned int);
    pi_register.pi_cart_addr_reg = GETDATA(curr, unsigned int);
    pi_register.pi_rd_len_reg = GETDATA(curr, unsigned int);
    pi_register.pi_wr_len_reg = GETDATA(curr, unsigned int);
    pi_register.read_pi_status_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_lat_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_pwd_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_pgs_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_rls_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_lat_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_pwd_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_pgs_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_rls_reg = GETDATA(curr, unsigned int);

    sp_register.sp_mem_addr_reg = GETDATA(curr, unsigned int);
    sp_register.sp_dram_addr_reg = GETDATA(curr, unsigned int);
    sp_register.sp_rd_len_reg = GETDATA(curr, unsigned int);
    sp_register.sp_wr_len_reg = GETDATA(curr, unsigned int);
    sp_register.w_sp_status_reg = GETDATA(curr, unsigned int);
    sp_register.sp_status_reg = GETDATA(curr, unsigned int);
    sp_register.halt = GETDATA(curr, unsigned char);
    sp_register.broke = GETDATA(curr, unsigned char);
    sp_register.dma_busy = GETDATA(curr, unsigned char);
    sp_register.dma_full = GETDATA(curr, unsigned char);
    sp_register.io_full = GETDATA(curr, unsigned char);
    sp_register.single_step = GETDATA(curr, unsigned char);
    sp_register.intr_break = GETDATA(curr, unsigned char);
    sp_register.signal0 = GETDATA(curr, unsigned char);
    sp_register.signal1 = GETDATA(curr, unsigned char);
    sp_register.signal2 = GETDATA(curr, unsigned char);
    sp_register.signal3 = GETDATA(curr, unsigned char);
    sp_register.signal4 = GETDATA(curr, unsigned char);
    sp_register.signal5 = GETDATA(curr, unsigned char);
    sp_register.signal6 = GETDATA(curr, unsigned char);
    sp_register.signal7 = GETDATA(curr, unsigned char);
    curr++; // Padding from old implementation
    sp_register.sp_dma_full_reg = GETDATA(curr, unsigned int);
    sp_register.sp_dma_busy_reg = GETDATA(curr, unsigned int);
    sp_register.sp_semaphore_reg = GETDATA(curr, unsigned int);

    rsp_register.rsp_pc = GETDATA(curr, unsigned int);
    rsp_register.rsp_ibist = GETDATA(curr, unsigned int);

    si_register.si_dram_addr = GETDATA(curr, unsigned int);
    si_register.si_pif_addr_rd64b = GETDATA(curr, unsigned int);
    si_register.si_pif_addr_wr64b = GETDATA(curr, unsigned int);
    si_register.si_stat = GETDATA(curr, unsigned int);

    vi_register.vi_status = GETDATA(curr, unsigned int);
    vi_register.vi_origin = GETDATA(curr, unsigned int);
    vi_register.vi_width = GETDATA(curr, unsigned int);
    vi_register.vi_v_intr = GETDATA(curr, unsigned int);
    vi_register.vi_current = GETDATA(curr, unsigned int);
    vi_register.vi_burst = GETDATA(curr, unsigned int);
    vi_register.vi_v_sync = GETDATA(curr, unsigned int);
    vi_register.vi_h_sync = GETDATA(curr, unsigned int);
    vi_register.vi_leap = GETDATA(curr, unsigned int);
    vi_register.vi_h_start = GETDATA(curr, unsigned int);
    vi_register.vi_v_start = GETDATA(curr, unsigned int);
    vi_register.vi_v_burst = GETDATA(curr, unsigned int);
    vi_register.vi_x_scale = GETDATA(curr, unsigned int);
    vi_register.vi_y_scale = GETDATA(curr, unsigned int);
    vi_register.vi_delay = GETDATA(curr, unsigned int);
    update_vi_status(vi_register.vi_status);
    update_vi_width(vi_register.vi_width);

    ri_register.ri_mode = GETDATA(curr, unsigned int);
    ri_register.ri_config = GETDATA(curr, unsigned int);
    ri_register.ri_current_load = GETDATA(curr, unsigned int);
    ri_register.ri_select = GETDATA(curr, unsigned int);
    ri_register.ri_refresh = GETDATA(curr, unsigned int);
    ri_register.ri_latency = GETDATA(curr, unsigned int);
    ri_register.ri_error = GETDATA(curr, unsigned int);
    ri_register.ri_werror = GETDATA(curr, unsigned int);

    ai_register.ai_dram_addr = GETDATA(curr, unsigned int);
    ai_register.ai_len = GETDATA(curr, unsigned int);
    ai_register.ai_control = GETDATA(curr, unsigned int);
    ai_register.ai_status = GETDATA(curr, unsigned int);
    ai_register.ai_dacrate = GETDATA(curr, unsigned int);
    ai_register.ai_bitrate = GETDATA(curr, unsigned int);
    ai_register.next_delay = GETDATA(curr, unsigned int);
    ai_register.next_len = GETDATA(curr, unsigned int);
    ai_register.current_delay = GETDATA(curr, unsigned int);
    ai_register.current_len = GETDATA(curr, unsigned int);
    update_ai_dacrate(ai_register.ai_dacrate);

    dpc_register.dpc_start = GETDATA(curr, unsigned int);
    dpc_register.dpc_end = GETDATA(curr, unsigned int);
    dpc_register.dpc_current = GETDATA(curr, unsigned int);
    dpc_register.w_dpc_status = GETDATA(curr, unsigned int);
    dpc_register.dpc_status = GETDATA(curr, unsigned int);
    dpc_register.xbus_dmem_dma = GETDATA(curr, unsigned char);
    dpc_register.freeze = GETDATA(curr, unsigned char);
    dpc_register.flush = GETDATA(curr, unsigned char);
    dpc_register.start_glck = GETDATA(curr, unsigned char);
    dpc_register.tmem_busy = GETDATA(curr, unsigned char);
    dpc_register.pipe_busy = GETDATA(curr, unsigned char);
    dpc_register.cmd_busy = GETDATA(curr, unsigned char);
    dpc_register.cbuf_busy = GETDATA(curr, unsigned char);
    dpc_register.dma_busy = GETDATA(curr, unsigned char);
    dpc_register.end_valid = GETDATA(curr, unsigned char);
    dpc_register.start_valid = GETDATA(curr, unsigned char);
    curr++;
    dpc_register.dpc_clock = GETDATA(curr, unsigned int);
    dpc_register.dpc_bufbusy = GETDATA(curr, unsigned int);
    dpc_register.dpc_pipebusy = GETDATA(curr, unsigned int);
    dpc_register.dpc_tmem = GETDATA(curr, unsigned int);

    dps_register.dps_tbist = GETDATA(curr, unsigned int);
    dps_register.dps_test_mode = GETDATA(curr, unsigned int);
    dps_register.dps_buftest_addr = GETDATA(curr, unsigned int);
    dps_register.dps_buftest_data = GETDATA(curr, unsigned int);

    COPYARRAY(rdram, curr, unsigned int, 0x800000/4);
    COPYARRAY(SP_DMEM, curr, unsigned int, 0x1000/4);
    COPYARRAY(SP_IMEM, curr, unsigned int, 0x1000/4);
    COPYARRAY(PIF_RAM, curr, unsigned char, 0x40);

    flashram_info.use_flashram = GETDATA(curr, int);
    flashram_info.mode = GETDATA(curr, int);
    flashram_info.status = GETDATA(curr, unsigned long long);
    flashram_info.erase_offset = GETDATA(curr, unsigned int);
    flashram_info.write_pointer = GETDATA(curr, unsigned int);

    COPYARRAY(tlb_LUT_r, curr, unsigned int, 0x100000);
    COPYARRAY(tlb_LUT_w, curr, unsigned int, 0x100000);

    llbit = GETDATA(curr, unsigned int);
    COPYARRAY(reg, curr, long long int, 32);
    COPYARRAY(reg_cop0, curr, unsigned int, 32);
    set_fpr_pointers(Status);  // Status is reg_cop0[12]
    lo = GETDATA(curr, long long int);
    hi = GETDATA(curr, long long int);
    COPYARRAY(reg_cop1_fgr_64, curr, long long int, 32);
    if ((Status & 0x04000000) == 0)  // 32-bit FPR mode requires data shuffling because 64-bit layout is always stored in savestate file
        shuffle_fpr_data(0x04000000, 0);
    FCR0 = GETDATA(curr, int);
    FCR31 = GETDATA(curr, int);

    for (i = 0; i < 32; i++)
    {
        tlb_e[i].mask = GETDATA(curr, short);
        curr += 2;
        tlb_e[i].vpn2 = GETDATA(curr, int);
        tlb_e[i].g = GETDATA(curr, char);
        tlb_e[i].asid = GETDATA(curr, unsigned char);
        curr += 2;
        tlb_e[i].pfn_even = GETDATA(curr, int);
        tlb_e[i].c_even = GETDATA(curr, char);
        tlb_e[i].d_even = GETDATA(curr, char);
        tlb_e[i].v_even = GETDATA(curr, char);
        curr++;
        tlb_e[i].pfn_odd = GETDATA(curr, int);
        tlb_e[i].c_odd = GETDATA(curr, char);
        tlb_e[i].d_odd = GETDATA(curr, char);
        tlb_e[i].v_odd = GETDATA(curr, char);
        tlb_e[i].r = GETDATA(curr, char);
   
        tlb_e[i].start_even = GETDATA(curr, unsigned int);
        tlb_e[i].end_even = GETDATA(curr, unsigned int);
        tlb_e[i].phys_even = GETDATA(curr, unsigned int);
        tlb_e[i].start_odd = GETDATA(curr, unsigned int);
        tlb_e[i].end_odd = GETDATA(curr, unsigned int);
        tlb_e[i].phys_odd = GETDATA(curr, unsigned int);
    }

    if(r4300emu == CORE_PURE_INTERPRETER)
        interp_addr = GETDATA(curr, unsigned int);
    else
    {
        for (i = 0; i < 0x100000; i++)
            invalid_code[i] = 1;
        jump_to(GETDATA(curr, unsigned int));
    }

    next_interupt = GETDATA(curr, unsigned int);
    next_vi = GETDATA(curr, unsigned int);
    vi_field = GETDATA(curr, unsigned int);

    // assert(savestateData+savestateSize == curr)

    to_little_endian(queue, 4, 256);
    load_eventqueue_infos(queue);

    if(r4300emu == CORE_PURE_INTERPRETER)
        last_addr = interp_addr;
    else
        last_addr = PC->addr;

    free(savestateData);
    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State loaded from: %s", namefrompath(filepath));
    return 1;
}

static int savestates_load_pj64(char *filepath, void *handle,
                                int (*read_func)(void *, void *, size_t))
{
    char buffer[1024];
    unsigned int vi_timer, SaveRDRAMSize;
    int i;

    unsigned char header[8];
    unsigned char RomHeader[0x40];

    size_t savestateSize;
    unsigned char *savestateData, *curr;

    /* Read and check Project64 magic number. */
    if (!read_func(handle, header, 8))
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not read header from Project64 savestate %s", filepath);
        return 0;
    }

    curr = header;
    if (memcmp(curr, pj64_magic, 4) != 0)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State file: %s is not a valid Project64 savestate. Unrecognized file format.", filepath);
        return 0;
    }
    curr += 4;

    SaveRDRAMSize = GETDATA(curr, unsigned int);

    /* Read the rest of the savestate into memory. */
    savestateSize = SaveRDRAMSize + 0x2754;
    savestateData = curr = (unsigned char *)malloc(savestateSize);
    if (savestateData == NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Insufficient memory to load state.");
        return 0;
    }
    if (!read_func(handle, savestateData, savestateSize))
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not read savestate data from Project64 savestate %s", filepath);
        free(savestateData);
        return 0;
    }

    // check ROM header
    COPYARRAY(RomHeader, curr, unsigned int, 0x40/4);
    if(memcmp(RomHeader, rom, 0x40) != 0)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "State ROM header does not match current ROM.");
        free(savestateData);
        return 0;
    }

    // vi_timer
    vi_timer = GETDATA(curr, unsigned int);

    // Program Counter
    last_addr = GETDATA(curr, unsigned int);

    // GPR
    COPYARRAY(reg, curr, long long int, 32);

    // FPR
    COPYARRAY(reg_cop1_fgr_64, curr, long long int, 32);

    // CP0
    COPYARRAY(reg_cop0, curr, unsigned int, 32);

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
    FCR0 = GETDATA(curr, int);
    curr += 30 * 4; // FCR1...FCR30 not supported
    FCR31 = GETDATA(curr, int);

    // hi / lo
    hi = GETDATA(curr, long long int);
    lo = GETDATA(curr, long long int);

    // rdram register
    rdram_register.rdram_config = GETDATA(curr, unsigned int);
    rdram_register.rdram_device_id = GETDATA(curr, unsigned int);
    rdram_register.rdram_delay = GETDATA(curr, unsigned int);
    rdram_register.rdram_mode = GETDATA(curr, unsigned int);
    rdram_register.rdram_ref_interval = GETDATA(curr, unsigned int);
    rdram_register.rdram_ref_row = GETDATA(curr, unsigned int);
    rdram_register.rdram_ras_interval = GETDATA(curr, unsigned int);
    rdram_register.rdram_min_interval = GETDATA(curr, unsigned int);
    rdram_register.rdram_addr_select = GETDATA(curr, unsigned int);
    rdram_register.rdram_device_manuf = GETDATA(curr, unsigned int);

    // sp_register
    sp_register.sp_mem_addr_reg = GETDATA(curr, unsigned int);
    sp_register.sp_dram_addr_reg = GETDATA(curr, unsigned int);
    sp_register.sp_rd_len_reg = GETDATA(curr, unsigned int);
    sp_register.sp_wr_len_reg = GETDATA(curr, unsigned int);
    sp_register.sp_status_reg = GETDATA(curr, unsigned int);
    sp_register.sp_dma_full_reg = GETDATA(curr, unsigned int);
    sp_register.sp_dma_busy_reg = GETDATA(curr, unsigned int);
    sp_register.sp_semaphore_reg = GETDATA(curr, unsigned int);
    rsp_register.rsp_pc = GETDATA(curr, unsigned int);
    rsp_register.rsp_ibist = GETDATA(curr, unsigned int);

    make_w_sp_status_reg();

    // dpc_register
    dpc_register.dpc_start = GETDATA(curr, unsigned int);
    dpc_register.dpc_end = GETDATA(curr, unsigned int);
    dpc_register.dpc_current = GETDATA(curr, unsigned int);
    dpc_register.dpc_status = GETDATA(curr, unsigned int);
    dpc_register.dpc_clock = GETDATA(curr, unsigned int);
    dpc_register.dpc_bufbusy = GETDATA(curr, unsigned int);
    dpc_register.dpc_pipebusy = GETDATA(curr, unsigned int);
    dpc_register.dpc_tmem = GETDATA(curr, unsigned int);
    (void)GETDATA(curr, unsigned int); // Dummy read
    (void)GETDATA(curr, unsigned int); // Dummy read

    make_w_dpc_status();

    // mi_register
    MI_register.mi_init_mode_reg = GETDATA(curr, unsigned int);
    MI_register.mi_version_reg = GETDATA(curr, unsigned int);
    MI_register.mi_intr_reg = GETDATA(curr, unsigned int);
    MI_register.mi_intr_mask_reg = GETDATA(curr, unsigned int);

    make_w_mi_init_mode_reg();
    make_w_mi_intr_mask_reg();

    // vi_register
    vi_register.vi_status = GETDATA(curr, unsigned int);
    vi_register.vi_origin = GETDATA(curr, unsigned int);
    vi_register.vi_width = GETDATA(curr, unsigned int);
    vi_register.vi_v_intr = GETDATA(curr, unsigned int);
    vi_register.vi_current = GETDATA(curr, unsigned int);
    vi_register.vi_burst = GETDATA(curr, unsigned int);
    vi_register.vi_v_sync = GETDATA(curr, unsigned int);
    vi_register.vi_h_sync = GETDATA(curr, unsigned int);
    vi_register.vi_leap = GETDATA(curr, unsigned int);
    vi_register.vi_h_start = GETDATA(curr, unsigned int);
    vi_register.vi_v_start = GETDATA(curr, unsigned int);
    vi_register.vi_v_burst = GETDATA(curr, unsigned int);
    vi_register.vi_x_scale = GETDATA(curr, unsigned int);
    vi_register.vi_y_scale = GETDATA(curr, unsigned int);
    // TODO vi delay?
    update_vi_status(vi_register.vi_status);
    update_vi_width(vi_register.vi_width);

    // ai_register
    ai_register.ai_dram_addr = GETDATA(curr, unsigned int);
    ai_register.ai_len = GETDATA(curr, unsigned int);
    ai_register.ai_control = GETDATA(curr, unsigned int);
    ai_register.ai_status = GETDATA(curr, unsigned int);
    ai_register.ai_dacrate = GETDATA(curr, unsigned int);
    ai_register.ai_bitrate = GETDATA(curr, unsigned int);
    update_ai_dacrate(ai_register.ai_dacrate);

    // pi_register
    pi_register.pi_dram_addr_reg = GETDATA(curr, unsigned int);
    pi_register.pi_cart_addr_reg = GETDATA(curr, unsigned int);
    pi_register.pi_rd_len_reg = GETDATA(curr, unsigned int);
    pi_register.pi_wr_len_reg = GETDATA(curr, unsigned int);
    pi_register.read_pi_status_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_lat_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_pwd_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_pgs_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom1_rls_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_lat_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_pwd_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_pgs_reg = GETDATA(curr, unsigned int);
    pi_register.pi_bsd_dom2_rls_reg = GETDATA(curr, unsigned int);
    read_func(handle, &pi_register, sizeof(PI_register));

    // ri_register
    ri_register.ri_mode = GETDATA(curr, unsigned int);
    ri_register.ri_config = GETDATA(curr, unsigned int);
    ri_register.ri_current_load = GETDATA(curr, unsigned int);
    ri_register.ri_select = GETDATA(curr, unsigned int);
    ri_register.ri_refresh = GETDATA(curr, unsigned int);
    ri_register.ri_latency = GETDATA(curr, unsigned int);
    ri_register.ri_error = GETDATA(curr, unsigned int);
    ri_register.ri_werror = GETDATA(curr, unsigned int);

    // si_register
    si_register.si_dram_addr = GETDATA(curr, unsigned int);
    si_register.si_pif_addr_rd64b = GETDATA(curr, unsigned int);
    si_register.si_pif_addr_wr64b = GETDATA(curr, unsigned int);
    si_register.si_stat = GETDATA(curr, unsigned int);

    // tlb
    memset(tlb_LUT_r, 0, 0x400000);
    memset(tlb_LUT_w, 0, 0x400000);
    for (i=0; i < 32; i++)
    {
        unsigned int MyEntryDefined = GETDATA(curr, unsigned int);
        unsigned int MyPageMask = GETDATA(curr, unsigned int);
        unsigned int MyEntryHi = GETDATA(curr, unsigned int);
        unsigned int MyEntryLo0 = GETDATA(curr, unsigned int);
        unsigned int MyEntryLo1 = GETDATA(curr, unsigned int);

        // This is copied from TLBWI instruction
        tlb_e[i].g = (MyEntryLo0 & MyEntryLo1 & 1);
        tlb_e[i].pfn_even = (MyEntryLo0 & 0x3FFFFFC0) >> 6;
        tlb_e[i].pfn_odd = (MyEntryLo1 & 0x3FFFFFC0) >> 6;
        tlb_e[i].c_even = (MyEntryLo0 & 0x38) >> 3;
        tlb_e[i].c_odd = (MyEntryLo1 & 0x38) >> 3;
        tlb_e[i].d_even = (MyEntryLo0 & 0x4) >> 2;
        tlb_e[i].d_odd = (MyEntryLo1 & 0x4) >> 2;
        tlb_e[i].v_even = (MyEntryLo0 & 0x2) >> 1;
        tlb_e[i].v_odd = (MyEntryLo1 & 0x2) >> 1;
        tlb_e[i].asid = (MyEntryHi & 0xFF);
        tlb_e[i].vpn2 = (MyEntryHi & 0xFFFFE000) >> 13;
        //tlb_e[i].r = (MyEntryHi & 0xC000000000000000LL) >> 62;
        tlb_e[i].mask = (MyPageMask & 0x1FFE000) >> 13;
           
        tlb_e[i].start_even = tlb_e[i].vpn2 << 13;
        tlb_e[i].end_even = tlb_e[i].start_even+
          (tlb_e[i].mask << 12) + 0xFFF;
        tlb_e[i].phys_even = tlb_e[i].pfn_even << 12;
           
        tlb_e[i].start_odd = tlb_e[i].end_even+1;
        tlb_e[i].end_odd = tlb_e[i].start_odd+
          (tlb_e[i].mask << 12) + 0xFFF;
        tlb_e[i].phys_odd = tlb_e[i].pfn_odd << 12;

        tlb_map(&tlb_e[i]);
    }

    // pif ram
    COPYARRAY(PIF_RAM, curr, unsigned char, 0x40);

    // RDRAM
    memset(rdram, 0, 0x800000);
    COPYARRAY(rdram, curr, unsigned int, SaveRDRAMSize/4);

    // DMEM
    COPYARRAY(SP_DMEM, curr, unsigned int, 0x1000/4);

    // IMEM
    COPYARRAY(SP_IMEM, curr, unsigned int, 0x1000/4);

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

    // assert(savestateData+savestateSize == curr)

    free(savestateData);
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
    unsigned char magic[4];
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
    else if (memcmp(magic, "PK\x03\x04", 4) == 0) // ZIP header
        return savestates_type_pj64_zip;
    else if (memcmp(magic, pj64_magic, 4) == 0) // PJ64 header
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
        type = savestates_type_pj64_zip;

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

static int savestates_save_m64p(char *filepath)
{
    unsigned char outbuf[4];
    gzFile f;
    int i;

    char queue[1024];
    int queuelength;

    size_t savestateSize;
    char *savestateData, *curr;

    if(autoinc_save_slot)
        savestates_inc_slot();

    queuelength = save_eventqueue_infos(queue);

    // Allocate memory for the save state data
    savestateSize = 16788288 + queuelength;
    savestateData = curr = malloc(savestateSize);
    if (savestateData == NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Insufficient memory to save state.");
        return 0;
    }

    // Write the save state data to memory
    PUTARRAY(savestate_magic, curr, unsigned char, 8);

    outbuf[0] = (savestate_latest_version >> 24) & 0xff;
    outbuf[1] = (savestate_latest_version >> 16) & 0xff;
    outbuf[2] = (savestate_latest_version >>  8) & 0xff;
    outbuf[3] = (savestate_latest_version >>  0) & 0xff;
    PUTARRAY(outbuf, curr, unsigned char, 4);

    PUTARRAY(ROM_SETTINGS.MD5, curr, char, 32);

    PUTDATA(curr, unsigned int, rdram_register.rdram_config);
    PUTDATA(curr, unsigned int, rdram_register.rdram_device_id);
    PUTDATA(curr, unsigned int, rdram_register.rdram_delay);
    PUTDATA(curr, unsigned int, rdram_register.rdram_mode);
    PUTDATA(curr, unsigned int, rdram_register.rdram_ref_interval);
    PUTDATA(curr, unsigned int, rdram_register.rdram_ref_row);
    PUTDATA(curr, unsigned int, rdram_register.rdram_ras_interval);
    PUTDATA(curr, unsigned int, rdram_register.rdram_min_interval);
    PUTDATA(curr, unsigned int, rdram_register.rdram_addr_select);
    PUTDATA(curr, unsigned int, rdram_register.rdram_device_manuf);

    PUTDATA(curr, unsigned int, MI_register.w_mi_init_mode_reg);
    PUTDATA(curr, unsigned int, MI_register.mi_init_mode_reg);
    PUTDATA(curr, unsigned char, MI_register.init_length);
    PUTDATA(curr, unsigned char, MI_register.init_mode);
    PUTDATA(curr, unsigned char, MI_register.ebus_test_mode);
    PUTDATA(curr, unsigned char, MI_register.RDRAM_reg_mode);
    PUTDATA(curr, unsigned int, MI_register.mi_version_reg);
    PUTDATA(curr, unsigned int, MI_register.mi_intr_reg);
    PUTDATA(curr, unsigned int, MI_register.mi_intr_mask_reg);
    PUTDATA(curr, unsigned int, MI_register.w_mi_intr_mask_reg);
    PUTDATA(curr, unsigned char, MI_register.SP_intr_mask);
    PUTDATA(curr, unsigned char, MI_register.SI_intr_mask);
    PUTDATA(curr, unsigned char, MI_register.AI_intr_mask);
    PUTDATA(curr, unsigned char, MI_register.VI_intr_mask);
    PUTDATA(curr, unsigned char, MI_register.PI_intr_mask);
    PUTDATA(curr, unsigned char, MI_register.DP_intr_mask);
    PUTDATA(curr, unsigned short, 0); // Padding from old implementation

    PUTDATA(curr, unsigned int, pi_register.pi_dram_addr_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_cart_addr_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_rd_len_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_wr_len_reg);
    PUTDATA(curr, unsigned int, pi_register.read_pi_status_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_lat_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_pwd_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_pgs_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_rls_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_lat_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_pwd_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_pgs_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_rls_reg);

    PUTDATA(curr, unsigned int, sp_register.sp_mem_addr_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_dram_addr_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_rd_len_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_wr_len_reg);
    PUTDATA(curr, unsigned int, sp_register.w_sp_status_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_status_reg);
    PUTDATA(curr, unsigned char, sp_register.halt);
    PUTDATA(curr, unsigned char, sp_register.broke);
    PUTDATA(curr, unsigned char, sp_register.dma_busy);
    PUTDATA(curr, unsigned char, sp_register.dma_full);
    PUTDATA(curr, unsigned char, sp_register.io_full);
    PUTDATA(curr, unsigned char, sp_register.single_step);
    PUTDATA(curr, unsigned char, sp_register.intr_break);
    PUTDATA(curr, unsigned char, sp_register.signal0);
    PUTDATA(curr, unsigned char, sp_register.signal1);
    PUTDATA(curr, unsigned char, sp_register.signal2);
    PUTDATA(curr, unsigned char, sp_register.signal3);
    PUTDATA(curr, unsigned char, sp_register.signal4);
    PUTDATA(curr, unsigned char, sp_register.signal5);
    PUTDATA(curr, unsigned char, sp_register.signal6);
    PUTDATA(curr, unsigned char, sp_register.signal7);
    PUTDATA(curr, unsigned char, 0);
    PUTDATA(curr, unsigned int, sp_register.sp_dma_full_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_dma_busy_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_semaphore_reg);

    PUTDATA(curr, unsigned int, rsp_register.rsp_pc);
    PUTDATA(curr, unsigned int, rsp_register.rsp_ibist);

    PUTDATA(curr, unsigned int, si_register.si_dram_addr);
    PUTDATA(curr, unsigned int, si_register.si_pif_addr_rd64b);
    PUTDATA(curr, unsigned int, si_register.si_pif_addr_wr64b);
    PUTDATA(curr, unsigned int, si_register.si_stat);

    PUTDATA(curr, unsigned int, vi_register.vi_status);
    PUTDATA(curr, unsigned int, vi_register.vi_origin);
    PUTDATA(curr, unsigned int, vi_register.vi_width);
    PUTDATA(curr, unsigned int, vi_register.vi_v_intr);
    PUTDATA(curr, unsigned int, vi_register.vi_current);
    PUTDATA(curr, unsigned int, vi_register.vi_burst);
    PUTDATA(curr, unsigned int, vi_register.vi_v_sync);
    PUTDATA(curr, unsigned int, vi_register.vi_h_sync);
    PUTDATA(curr, unsigned int, vi_register.vi_leap);
    PUTDATA(curr, unsigned int, vi_register.vi_h_start);
    PUTDATA(curr, unsigned int, vi_register.vi_v_start);
    PUTDATA(curr, unsigned int, vi_register.vi_v_burst);
    PUTDATA(curr, unsigned int, vi_register.vi_x_scale);
    PUTDATA(curr, unsigned int, vi_register.vi_y_scale);
    PUTDATA(curr, unsigned int, vi_register.vi_delay);

    PUTDATA(curr, unsigned int, ri_register.ri_mode);
    PUTDATA(curr, unsigned int, ri_register.ri_config);
    PUTDATA(curr, unsigned int, ri_register.ri_current_load);
    PUTDATA(curr, unsigned int, ri_register.ri_select);
    PUTDATA(curr, unsigned int, ri_register.ri_refresh);
    PUTDATA(curr, unsigned int, ri_register.ri_latency);
    PUTDATA(curr, unsigned int, ri_register.ri_error);
    PUTDATA(curr, unsigned int, ri_register.ri_werror);

    PUTDATA(curr, unsigned int, ai_register.ai_dram_addr);
    PUTDATA(curr, unsigned int, ai_register.ai_len);
    PUTDATA(curr, unsigned int, ai_register.ai_control);
    PUTDATA(curr, unsigned int, ai_register.ai_status);
    PUTDATA(curr, unsigned int, ai_register.ai_dacrate);
    PUTDATA(curr, unsigned int, ai_register.ai_bitrate);
    PUTDATA(curr, unsigned int, ai_register.next_delay);
    PUTDATA(curr, unsigned int, ai_register.next_len);
    PUTDATA(curr, unsigned int, ai_register.current_delay);
    PUTDATA(curr, unsigned int, ai_register.current_len);

    PUTDATA(curr, unsigned int, dpc_register.dpc_start);
    PUTDATA(curr, unsigned int, dpc_register.dpc_end);
    PUTDATA(curr, unsigned int, dpc_register.dpc_current);
    PUTDATA(curr, unsigned int, dpc_register.w_dpc_status);
    PUTDATA(curr, unsigned int, dpc_register.dpc_status);
    PUTDATA(curr, unsigned char, dpc_register.xbus_dmem_dma);
    PUTDATA(curr, unsigned char, dpc_register.freeze);
    PUTDATA(curr, unsigned char, dpc_register.flush);
    PUTDATA(curr, unsigned char, dpc_register.start_glck);
    PUTDATA(curr, unsigned char, dpc_register.tmem_busy);
    PUTDATA(curr, unsigned char, dpc_register.pipe_busy);
    PUTDATA(curr, unsigned char, dpc_register.cmd_busy);
    PUTDATA(curr, unsigned char, dpc_register.cbuf_busy);
    PUTDATA(curr, unsigned char, dpc_register.dma_busy);
    PUTDATA(curr, unsigned char, dpc_register.end_valid);
    PUTDATA(curr, unsigned char, dpc_register.start_valid);
    PUTDATA(curr, unsigned char, 0);
    PUTDATA(curr, unsigned int, dpc_register.dpc_clock);
    PUTDATA(curr, unsigned int, dpc_register.dpc_bufbusy);
    PUTDATA(curr, unsigned int, dpc_register.dpc_pipebusy);
    PUTDATA(curr, unsigned int, dpc_register.dpc_tmem);

    PUTDATA(curr, unsigned int, dps_register.dps_tbist);
    PUTDATA(curr, unsigned int, dps_register.dps_test_mode);
    PUTDATA(curr, unsigned int, dps_register.dps_buftest_addr);
    PUTDATA(curr, unsigned int, dps_register.dps_buftest_data);

    PUTARRAY(rdram, curr, unsigned int, 0x800000/4);
    PUTARRAY(SP_DMEM, curr, unsigned int, 0x1000/4);
    PUTARRAY(SP_IMEM, curr, unsigned int, 0x1000/4);
    PUTARRAY(PIF_RAM, curr, unsigned char, 0x40);

    PUTDATA(curr, int, flashram_info.use_flashram);
    PUTDATA(curr, int, flashram_info.mode);
    PUTDATA(curr, unsigned long long, flashram_info.status);
    PUTDATA(curr, unsigned int, flashram_info.erase_offset);
    PUTDATA(curr, unsigned int, flashram_info.write_pointer);

    PUTARRAY(tlb_LUT_r, curr, unsigned int, 0x100000);
    PUTARRAY(tlb_LUT_w, curr, unsigned int, 0x100000);

    PUTDATA(curr, unsigned int, llbit);
    PUTARRAY(reg, curr, long long int, 32);
    PUTARRAY(reg_cop0, curr, unsigned int, 32);
    PUTDATA(curr, long long int, lo);
    PUTDATA(curr, long long int, hi);

    if ((Status & 0x04000000) == 0) // FR bit == 0 means 32-bit (MIPS I) FGR mode
        shuffle_fpr_data(0, 0x04000000);  // shuffle data into 64-bit register format for storage
    PUTARRAY(reg_cop1_fgr_64, curr, long long int, 32);
    if ((Status & 0x04000000) == 0)
        shuffle_fpr_data(0x04000000, 0);  // put it back in 32-bit mode

    PUTDATA(curr, int, FCR0);
    PUTDATA(curr, int, FCR31);
    for (i = 0; i < 32; i++)
    {
        PUTDATA(curr, short, tlb_e[i].mask);
        PUTDATA(curr, short, 0);
        PUTDATA(curr, int, tlb_e[i].vpn2);
        PUTDATA(curr, char, tlb_e[i].g);
        PUTDATA(curr, unsigned char, tlb_e[i].asid);
        PUTDATA(curr, short, 0);
        PUTDATA(curr, int, tlb_e[i].pfn_even);
        PUTDATA(curr, char, tlb_e[i].c_even);
        PUTDATA(curr, char, tlb_e[i].d_even);
        PUTDATA(curr, char, tlb_e[i].v_even);
        PUTDATA(curr, char, 0);
        PUTDATA(curr, int, tlb_e[i].pfn_odd);
        PUTDATA(curr, char, tlb_e[i].c_odd);
        PUTDATA(curr, char, tlb_e[i].d_odd);
        PUTDATA(curr, char, tlb_e[i].v_odd);
        PUTDATA(curr, char, tlb_e[i].r);
   
        PUTDATA(curr, unsigned int, tlb_e[i].start_even);
        PUTDATA(curr, unsigned int, tlb_e[i].end_even);
        PUTDATA(curr, unsigned int, tlb_e[i].phys_even);
        PUTDATA(curr, unsigned int, tlb_e[i].start_odd);
        PUTDATA(curr, unsigned int, tlb_e[i].end_odd);
        PUTDATA(curr, unsigned int, tlb_e[i].phys_odd);
    }
    if(r4300emu == CORE_PURE_INTERPRETER)
        PUTDATA(curr, unsigned int, interp_addr);
    else
        PUTDATA(curr, unsigned int, PC->addr);

    PUTDATA(curr, unsigned int, next_interupt);
    PUTDATA(curr, unsigned int, next_vi);
    PUTDATA(curr, unsigned int, vi_field);

    to_little_endian(queue, 4, queuelength/4);
    PUTARRAY(queue, curr, char, queuelength);

    // assert(curr == savestateData + savestateSize)

    // Write the state to a GZIP file
    f = gzopen(filepath, "wb");

    if (f==NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not open state file: %s", filepath);
        free(savestateData);
        return 0;
    }

    if (gzwrite(f, savestateData, savestateSize) != savestateSize)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Could not write data to state file: %s", filepath);
        gzclose(f);
        free(savestateData);
        return 0;
    }

    gzclose(f);
    free(savestateData);
    main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Saved state to: %s", namefrompath(filepath));
    return 1;
}

static int savestates_save_pj64(char *filepath, void *handle,
                                int (*write_func)(void *, const void *, size_t))
{
    unsigned int i, addr;
    unsigned int SaveRDRAMSize = 0x800000;

    size_t savestateSize;
    unsigned char *savestateData, *curr;

    if(r4300emu == CORE_PURE_INTERPRETER)
        addr = interp_addr;
    else
        addr = PC->addr;

    // Allocate memory for the save state data
    savestateSize = 8 + SaveRDRAMSize + 0x2754;
    savestateData = curr = (unsigned char *)malloc(savestateSize);
    if (savestateData == NULL)
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Insufficient memory to save state.");
        return 0;
    }

    // Write the save state data in memory
    PUTARRAY(pj64_magic, curr, unsigned char, 4);
    PUTDATA(curr, unsigned int, SaveRDRAMSize);
    PUTARRAY(rom, curr, unsigned int, 0x40/4);
    PUTDATA(curr, unsigned int, get_event(VI_INT) - reg_cop0[9]); // vi_timer
    PUTDATA(curr, unsigned int, addr);
    PUTARRAY(reg, curr, long long int, 32);
    if ((Status & 0x04000000) == 0) // TODO not sure how pj64 handles this
        shuffle_fpr_data(0x04000000, 0);
    PUTARRAY(reg_cop1_fgr_64, curr, long long int, 32);
    if ((Status & 0x04000000) == 0) // TODO not sure how pj64 handles this
        shuffle_fpr_data(0x04000000, 0);
    PUTARRAY(reg_cop0, curr, unsigned int, 32);
    PUTDATA(curr, int, FCR0);
    for (i = 0; i < 30; i++)
        PUTDATA(curr, int, 0); // FCR1-30 not implemented
    PUTDATA(curr, int, FCR31);
    PUTDATA(curr, long long int, hi);
    PUTDATA(curr, long long int, lo);

    PUTDATA(curr, unsigned int, rdram_register.rdram_config);
    PUTDATA(curr, unsigned int, rdram_register.rdram_device_id);
    PUTDATA(curr, unsigned int, rdram_register.rdram_delay);
    PUTDATA(curr, unsigned int, rdram_register.rdram_mode);
    PUTDATA(curr, unsigned int, rdram_register.rdram_ref_interval);
    PUTDATA(curr, unsigned int, rdram_register.rdram_ref_row);
    PUTDATA(curr, unsigned int, rdram_register.rdram_ras_interval);
    PUTDATA(curr, unsigned int, rdram_register.rdram_min_interval);
    PUTDATA(curr, unsigned int, rdram_register.rdram_addr_select);
    PUTDATA(curr, unsigned int, rdram_register.rdram_device_manuf);

    PUTDATA(curr, unsigned int, sp_register.sp_mem_addr_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_dram_addr_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_rd_len_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_wr_len_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_status_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_dma_full_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_dma_busy_reg);
    PUTDATA(curr, unsigned int, sp_register.sp_semaphore_reg);

    PUTDATA(curr, unsigned int, rsp_register.rsp_pc);
    PUTDATA(curr, unsigned int, rsp_register.rsp_ibist);

    PUTDATA(curr, unsigned int, dpc_register.dpc_start);
    PUTDATA(curr, unsigned int, dpc_register.dpc_end);
    PUTDATA(curr, unsigned int, dpc_register.dpc_current);
    PUTDATA(curr, unsigned int, dpc_register.dpc_status);
    PUTDATA(curr, unsigned int, dpc_register.dpc_clock);
    PUTDATA(curr, unsigned int, dpc_register.dpc_bufbusy);
    PUTDATA(curr, unsigned int, dpc_register.dpc_pipebusy);
    PUTDATA(curr, unsigned int, dpc_register.dpc_tmem);
    PUTDATA(curr, unsigned int, 0); // ?
    PUTDATA(curr, unsigned int, 0); // ?

    PUTDATA(curr, unsigned int, MI_register.mi_init_mode_reg); //TODO Secial handling in pj64
    PUTDATA(curr, unsigned int, MI_register.mi_version_reg);
    PUTDATA(curr, unsigned int, MI_register.mi_intr_reg);
    PUTDATA(curr, unsigned int, MI_register.mi_intr_mask_reg);

    PUTDATA(curr, unsigned int, vi_register.vi_status);
    PUTDATA(curr, unsigned int, vi_register.vi_origin);
    PUTDATA(curr, unsigned int, vi_register.vi_width);
    PUTDATA(curr, unsigned int, vi_register.vi_v_intr);
    PUTDATA(curr, unsigned int, vi_register.vi_current);
    PUTDATA(curr, unsigned int, vi_register.vi_burst);
    PUTDATA(curr, unsigned int, vi_register.vi_v_sync);
    PUTDATA(curr, unsigned int, vi_register.vi_h_sync);
    PUTDATA(curr, unsigned int, vi_register.vi_leap);
    PUTDATA(curr, unsigned int, vi_register.vi_h_start);
    PUTDATA(curr, unsigned int, vi_register.vi_v_start);
    PUTDATA(curr, unsigned int, vi_register.vi_v_burst);
    PUTDATA(curr, unsigned int, vi_register.vi_x_scale);
    PUTDATA(curr, unsigned int, vi_register.vi_y_scale);

    PUTDATA(curr, unsigned int, ai_register.ai_dram_addr);
    PUTDATA(curr, unsigned int, ai_register.ai_len);
    PUTDATA(curr, unsigned int, ai_register.ai_control);
    PUTDATA(curr, unsigned int, ai_register.ai_status);
    PUTDATA(curr, unsigned int, ai_register.ai_dacrate);
    PUTDATA(curr, unsigned int, ai_register.ai_bitrate);

    PUTDATA(curr, unsigned int, pi_register.pi_dram_addr_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_cart_addr_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_rd_len_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_wr_len_reg);
    PUTDATA(curr, unsigned int, pi_register.read_pi_status_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_lat_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_pwd_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_pgs_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom1_rls_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_lat_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_pwd_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_pgs_reg);
    PUTDATA(curr, unsigned int, pi_register.pi_bsd_dom2_rls_reg);

    PUTDATA(curr, unsigned int, ri_register.ri_mode);
    PUTDATA(curr, unsigned int, ri_register.ri_config);
    PUTDATA(curr, unsigned int, ri_register.ri_current_load);
    PUTDATA(curr, unsigned int, ri_register.ri_select);
    PUTDATA(curr, unsigned int, ri_register.ri_refresh);
    PUTDATA(curr, unsigned int, ri_register.ri_latency);
    PUTDATA(curr, unsigned int, ri_register.ri_error);
    PUTDATA(curr, unsigned int, ri_register.ri_werror);

    PUTDATA(curr, unsigned int, si_register.si_dram_addr);
    PUTDATA(curr, unsigned int, si_register.si_pif_addr_rd64b);
    PUTDATA(curr, unsigned int, si_register.si_pif_addr_wr64b);
    PUTDATA(curr, unsigned int, si_register.si_stat);

    for (i=0; i < 32;i++)
    {
        // From TLBR
        unsigned int EntryDefined, MyPageMask, MyEntryHi, MyEntryLo0, MyEntryLo1;
        EntryDefined = 1;
        MyPageMask = tlb_e[i].mask << 13;
        MyEntryHi = ((tlb_e[i].vpn2 << 13) | tlb_e[i].asid);
        MyEntryLo0 = (tlb_e[i].pfn_even << 6) | (tlb_e[i].c_even << 3)
         | (tlb_e[i].d_even << 2) | (tlb_e[i].v_even << 1)
           | tlb_e[i].g;
        MyEntryLo1 = (tlb_e[i].pfn_odd << 6) | (tlb_e[i].c_odd << 3)
         | (tlb_e[i].d_odd << 2) | (tlb_e[i].v_odd << 1)
           | tlb_e[i].g;

        PUTDATA(curr, unsigned int, EntryDefined);
        PUTDATA(curr, unsigned int, MyPageMask);
        PUTDATA(curr, unsigned int, MyEntryHi);
        PUTDATA(curr, unsigned int, MyEntryLo0);
        PUTDATA(curr, unsigned int, MyEntryLo1);
    }

    PUTARRAY(PIF_RAM, curr, unsigned char, 0x40);

    PUTARRAY(rdram, curr, unsigned int, SaveRDRAMSize/4);
    PUTARRAY(SP_DMEM, curr, unsigned int, 0x1000/4);
    PUTARRAY(SP_IMEM, curr, unsigned int, 0x1000/4);

    // Write the save state data to the output
    if (!write_func(handle, savestateData, savestateSize))
    {
        main_message(M64MSG_STATUS, OSD_BOTTOM_LEFT, "Couldn't write data to Project64 state file %s.", filepath);
        free(savestateData);
        return 0;
    }

    // assert(savestateData+savestateSize == curr)
    free(savestateData);
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
        goto clean_and_exit;

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
        type = savestates_type_pj64_zip;

    filepath = savestates_generate_path(type);
    if (filepath != NULL)
    {
        switch (type)
        {
            case savestates_type_m64p: ret = savestates_save_m64p(filepath); break;
            case savestates_type_pj64_zip: ret = savestates_save_pj64_zip(filepath); break;
            case savestates_type_pj64_unc: ret = savestates_save_pj64_unc(filepath); break;
            default: ret = 0; break;
        }
        free(filepath);
    }

    savestates_clear_job();
    return ret;
}
