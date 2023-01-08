/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*   Mupen64plus - dd_controller.c                                         *
*   Mupen64Plus homepage: https://mupen64plus.org/                        *
*   Copyright (C) 2015 LuigiBlood                                         *
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

#include "dd_controller.h"

#include <assert.h>
#include <string.h>
#include <time.h>

#define M64P_CORE_PROTOTYPES 1
#include "api/m64p_types.h"
#include "api/callbacks.h"
#include "backends/api/clock_backend.h"
#include "backends/api/storage_backend.h"
#include "device/dd/disk.h"
#include "device/device.h"
#include "device/memory/memory.h"
#include "device/r4300/r4300_core.h"

/* dd commands definition */
#define DD_CMD_NOOP             UINT32_C(0x00000000)
#define DD_CMD_SEEK_READ        UINT32_C(0x00010001)
#define DD_CMD_SEEK_WRITE       UINT32_C(0x00020001)
#define DD_CMD_RECALIBRATE      UINT32_C(0x00030001) // ???
#define DD_CMD_SLEEP            UINT32_C(0x00040000)
#define DD_CMD_START            UINT32_C(0x00050001)
#define DD_CMD_SET_STANDBY      UINT32_C(0x00060000)
#define DD_CMD_SET_SLEEP        UINT32_C(0x00070000)
#define DD_CMD_CLR_DSK_CHNG     UINT32_C(0x00080000)
#define DD_CMD_CLR_RESET        UINT32_C(0x00090000)
#define DD_CMD_READ_VERSION     UINT32_C(0x000A0000)
#define DD_CMD_SET_DISK_TYPE    UINT32_C(0x000B0001)
#define DD_CMD_REQUEST_STATUS   UINT32_C(0x000C0000)
#define DD_CMD_STANDBY          UINT32_C(0x000D0000)
#define DD_CMD_IDX_LOCK_RETRY   UINT32_C(0x000E0000) // ???
#define DD_CMD_SET_YEAR_MONTH   UINT32_C(0x000F0000)
#define DD_CMD_SET_DAY_HOUR     UINT32_C(0x00100000)
#define DD_CMD_SET_MIN_SEC      UINT32_C(0x00110000)
#define DD_CMD_GET_YEAR_MONTH   UINT32_C(0x00120000)
#define DD_CMD_GET_DAY_HOUR     UINT32_C(0x00130000)
#define DD_CMD_GET_MIN_SEC      UINT32_C(0x00140000)
#define DD_CMD_FEATURE_INQ      UINT32_C(0x001B0000)

/* dd status register bitfields definition */
#define DD_STATUS_DATA_RQ       UINT32_C(0x40000000)
#define DD_STATUS_C2_XFER       UINT32_C(0x10000000)
#define DD_STATUS_BM_ERR        UINT32_C(0x08000000)
#define DD_STATUS_BM_INT        UINT32_C(0x04000000)
#define DD_STATUS_MECHA_INT     UINT32_C(0x02000000)
#define DD_STATUS_DISK_PRES     UINT32_C(0x01000000)
#define DD_STATUS_BUSY_STATE    UINT32_C(0x00800000)
#define DD_STATUS_RST_STATE     UINT32_C(0x00400000)
#define DD_STATUS_MTR_N_SPIN    UINT32_C(0x00100000)
#define DD_STATUS_HEAD_RTRCT    UINT32_C(0x00080000)
#define DD_STATUS_WR_PR_ERR     UINT32_C(0x00040000)
#define DD_STATUS_MECHA_ERR     UINT32_C(0x00020000)
#define DD_STATUS_DISK_CHNG     UINT32_C(0x00010000)

/* dd bm status register bitfields definition */
/* read flags */
#define DD_BM_STATUS_RUNNING    UINT32_C(0x80000000)
#define DD_BM_STATUS_ERROR      UINT32_C(0x04000000)
#define DD_BM_STATUS_MICRO      UINT32_C(0x02000000) /* ??? */
#define DD_BM_STATUS_BLOCK      UINT32_C(0x01000000)
#define DD_BM_STATUS_C1CRR      UINT32_C(0x00800000)
#define DD_BM_STATUS_C1DBL      UINT32_C(0x00400000)
#define DD_BM_STATUS_C1SNG      UINT32_C(0x00200000)
#define DD_BM_STATUS_C1ERR      UINT32_C(0x00010000) /* Typo ??? */
/* write flags */
#define DD_BM_CTL_START         UINT32_C(0x80000000)
#define DD_BM_CTL_MNGRMODE      UINT32_C(0x40000000)
#define DD_BM_CTL_INTMASK       UINT32_C(0x20000000)
#define DD_BM_CTL_RESET         UINT32_C(0x10000000)
#define DD_BM_CTL_DIS_OR_CHK    UINT32_C(0x08000000) /* ??? */
#define DD_BM_CTL_DIS_C1_CRR    UINT32_C(0x04000000)
#define DD_BM_CTL_BLK_TRANS     UINT32_C(0x02000000)
#define DD_BM_CTL_MECHA_RST     UINT32_C(0x01000000)

#define DD_TRACK_LOCK           UINT32_C(0x60000000)


static uint8_t byte2bcd(int n)
{
    n %= 100;
    return (uint8_t)(((n / 10) << 4) | (n % 10));
}

static uint32_t time2data(int hi, int lo)
{
    return ((uint32_t)byte2bcd(hi) << 24)
         | ((uint32_t)byte2bcd(lo) << 16);
}

static void update_rtc(struct dd_rtc* rtc)
{
    /* update rtc->now */
    time_t now = rtc->iclock->get_time(rtc->clock);
    rtc->now += now - rtc->last_update_rtc;
    rtc->last_update_rtc = now;
}

static void signal_dd_interrupt(struct dd_controller* dd, uint32_t bm_int)
{
    dd->regs[DD_ASIC_CMD_STATUS] |= bm_int;
    r4300_check_interrupt(dd->r4300, CP0_CAUSE_IP3, 1);
}

static void clear_dd_interrupt(struct dd_controller* dd, uint32_t bm_int)
{
    dd->regs[DD_ASIC_CMD_STATUS] &= ~bm_int;
    r4300_check_interrupt(dd->r4300, CP0_CAUSE_IP3, 0);
}

void dd_mecha_int_handler(void* opaque)
{
    struct dd_controller* dd = (struct dd_controller*)opaque;
    signal_dd_interrupt(dd, DD_STATUS_MECHA_INT);
}

void dd_bm_int_handler(void* opaque)
{
    struct dd_controller* dd = (struct dd_controller*)opaque;
    dd_update_bm(dd);
}

static void read_C2(struct dd_controller* dd)
{
    size_t i;

    size_t length = zone_sec_size[dd->bm_zone];
    unsigned int sector = (dd->regs[DD_ASIC_CUR_SECTOR] >> 16) & 0xff;
    sector %= 90;
    size_t offset = 0x40 * (sector - SECTORS_PER_BLOCK);

    DebugMessage(M64MSG_VERBOSE, "read C2: length=%08x, offset=%08x",
            (uint32_t)length, (uint32_t)offset);

    for (i = 0; i < length; ++i) {
        dd->c2s_buf[(offset + i) ^ 3] = 0;
    }
}

static uint8_t* seek_sector(struct dd_controller* dd)
{
    unsigned int head  = (dd->regs[DD_ASIC_CUR_TK] & 0x10000000) >> 28;
    unsigned int track = (dd->regs[DD_ASIC_CUR_TK] & 0x0fff0000) >> 16;
    // XXX: takes into account that for writes dd_update_bm use the previous sector.
    unsigned int sector = ((dd->regs[DD_ASIC_CUR_SECTOR] >> 16) & 0xff) - dd->bm_write;
    unsigned int block = sector / 90;
    sector %= 90;

    uint8_t* sector_base = get_sector_base(dd->disk, head, track, block, sector);
    if (sector_base == NULL) {
        dd->regs[DD_ASIC_BM_STATUS_CTL] |= DD_BM_STATUS_MICRO;
    }

    return sector_base;
}

static void read_sector(struct dd_controller* dd)
{
    size_t i;
    const uint8_t* disk_sec = seek_sector(dd);
    if (disk_sec == NULL) {
        return;
    }

    size_t length = dd->regs[DD_ASIC_HOST_SECBYTE] + 1;

    for (i = 0; i < length; ++i) {
        dd->ds_buf[i ^ 3] = disk_sec[i];
    }
}

static void write_sector(struct dd_controller* dd)
{
    size_t i;
    uint8_t* disk_sec = seek_sector(dd);
    if (disk_sec == NULL) {
        return;
    }

    size_t length = dd->regs[DD_ASIC_HOST_SECBYTE] + 1;

	for (i = 0; i < length; ++i) {
		disk_sec[i] = dd->ds_buf[i ^ 3];
    }

    dd->idisk->save(dd->disk, disk_sec - dd->idisk->data(dd->disk), length);
}

void dd_update_bm(void* opaque)
{
    struct dd_controller* dd = (struct dd_controller*)opaque;

    /* not running */
	if ((dd->regs[DD_ASIC_BM_STATUS_CTL] & DD_BM_STATUS_RUNNING) == 0) {
		return;
    }

    /* clear flags */
    dd->regs[DD_ASIC_CMD_STATUS] &= ~(DD_STATUS_DATA_RQ | DD_STATUS_C2_XFER);

    /* calculate sector and block info for use later */
    unsigned int sector = (dd->regs[DD_ASIC_CUR_SECTOR] >> 16) & 0xff;
    unsigned int block = sector / 90;
    sector %= 90;

    /* handle writes (BM mode 0) */
    if (dd->bm_write) {
        /* first sector : just issue a BM interrupt to get things going */
        if (sector == 0) {
            dd->regs[DD_ASIC_CUR_SECTOR] += 0x10000;
            dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_DATA_RQ;
        }
        /* subsequent sectors: write previous sector */
        else if (sector < SECTORS_PER_BLOCK) {
            write_sector(dd);
            dd->regs[DD_ASIC_CUR_SECTOR] += 0x10000;
            dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_DATA_RQ;
        }
        /* otherwise write last sector */
        else if (sector < SECTORS_PER_BLOCK + 1) {
            write_sector(dd);

            /* continue to next block */
            if (dd->regs[DD_ASIC_BM_STATUS_CTL] & DD_BM_STATUS_BLOCK) {
                // Start at next block sector 1.
                dd->regs[DD_ASIC_CUR_SECTOR] = ((1 - block) * 90 + 1) << 16;
                dd->regs[DD_ASIC_BM_STATUS_CTL] &= ~DD_BM_STATUS_BLOCK;
                dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_DATA_RQ;
            /* quit writing after second block */
            } else {
                dd->regs[DD_ASIC_CUR_SECTOR] += 0x10000;
                dd->regs[DD_ASIC_BM_STATUS_CTL] &= ~DD_BM_STATUS_RUNNING;
            }
        }
        else {
            DebugMessage(M64MSG_ERROR, "DD Write, sector overrun");
        }
    }
    /* handle reads (BM mode 1) */
    else {
        uint8_t dev = dd->disk->development;
        /* track 6 fails to read on retail units (XXX: retail test) */
        if ((((dd->regs[DD_ASIC_CUR_TK] >> 16) & 0x1fff) == 6) && block == 0 && !dev) {
            dd->regs[DD_ASIC_CMD_STATUS] &= ~DD_STATUS_DATA_RQ;
            dd->regs[DD_ASIC_BM_STATUS_CTL] |= DD_BM_STATUS_MICRO;
        }
        /* data sectors : read sector and signal BM interrupt */
        else if (sector < SECTORS_PER_BLOCK) {
            read_sector(dd);
            dd->regs[DD_ASIC_CUR_SECTOR] += 0x10000;
            dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_DATA_RQ;
        }
        /* C2 sectors: do nothing since they're loaded with zeros */
        else if (sector < SECTORS_PER_BLOCK + 3) {
            read_C2(dd);
            dd->regs[DD_ASIC_CUR_SECTOR] += 0x10000;
        }
        /* Last C2 sector: continue to next block, quit after second block */
        else if (sector == SECTORS_PER_BLOCK + 3) {
            read_C2(dd);
            dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_C2_XFER;
            if (dd->regs[DD_ASIC_BM_STATUS_CTL] & DD_BM_STATUS_BLOCK) {
                // Start at next block sector 0.
                dd->regs[DD_ASIC_CUR_SECTOR] = ((1 - block) * 90 + 0) << 16;
                dd->regs[DD_ASIC_BM_STATUS_CTL] &= ~DD_BM_STATUS_BLOCK;
            }
            else {
                dd->regs[DD_ASIC_BM_STATUS_CTL] &= ~DD_BM_STATUS_RUNNING;
            }
        }
        else {
            DebugMessage(M64MSG_ERROR, "DD Read, sector overrun");
        }
    }

    /* Signal a BM interrupt */
    signal_dd_interrupt(dd, DD_STATUS_BM_INT);
}



void init_dd(struct dd_controller* dd,
             void* clock, const struct clock_backend_interface* iclock,
             const uint32_t* rom, size_t rom_size,
             struct dd_disk* disk, const struct storage_backend_interface* idisk,
             struct r4300_core* r4300)
{
    dd->rtc.clock = clock;
    dd->rtc.iclock = iclock;

    dd->rom = rom;
    dd->rom_size = rom_size;

    dd->disk = disk;
    dd->idisk = idisk;

    dd->r4300 = r4300;
}

void poweron_dd(struct dd_controller* dd)
{
    memset(dd->regs, 0, DD_ASIC_REGS_COUNT*sizeof(dd->regs[0]));
    memset(dd->c2s_buf, 0, 0x400);
    memset(dd->ds_buf, 0, 0x100);
    memset(dd->ms_ram, 0, 0x40);

    dd->bm_write = 0;
    dd->bm_reset_held = 0;
    dd->bm_zone = 0;

    dd->rtc.now = 0;
    dd->rtc.last_update_rtc = 0;

    dd->regs[DD_ASIC_ID_REG] = 0x00030000;
    dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_RST_STATE;
    if (dd->idisk != NULL) {
        dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_DISK_PRES;
        if (dd->disk->development)
            dd->regs[DD_ASIC_ID_REG] = 0x00040000;
    }
}

void read_dd_regs(void* opaque, uint32_t address, uint32_t* value)
{
    struct dd_controller* dd = (struct dd_controller*)opaque;

    if (address < MM_DD_REGS || address >= MM_DD_MS_RAM) {
        DebugMessage(M64MSG_ERROR, "Unknown access in DD registers MMIO space %08x", address);
        *value = 0;
        return;
    }

    uint32_t reg = dd_reg(address);

    /* disk presence test */
    if (reg == DD_ASIC_CMD_STATUS) {
        if (dd->idisk != NULL) {
            dd->regs[reg] |= DD_STATUS_DISK_PRES;
        }
        else {
            dd->regs[reg] &= ~DD_STATUS_DISK_PRES;
        }
    }

    *value = dd->regs[reg];
    DebugMessage(M64MSG_VERBOSE, "DD REG: %08X -> %08x", address, *value);

    /* post read update. Not part of the returned value */
    switch(reg)
    {
    case DD_ASIC_CMD_STATUS: {
            /* acknowledge BM interrupt */
            if (dd->regs[DD_ASIC_CMD_STATUS] & DD_STATUS_BM_INT) {
                clear_dd_interrupt(dd, DD_STATUS_BM_INT);
                add_interrupt_event(&dd->r4300->cp0, DD_BM_INT, 8020 + (((dd->regs[DD_ASIC_CUR_TK] & 0x0fff0000) >> 16) / 56));
            }
        } break;
    }
}

void write_dd_regs(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    unsigned int head, track, old_track, cycles;
    struct dd_controller* dd = (struct dd_controller*)opaque;

    if (address < MM_DD_REGS || address >= MM_DD_MS_RAM) {
        DebugMessage(M64MSG_ERROR, "Unknown access in DD registers MMIO space %08x", address);
        return;
    }

    uint32_t reg = dd_reg(address);

    assert(mask == ~UINT32_C(0));

    DebugMessage(M64MSG_VERBOSE, "DD REG: %08X <- %08x", address, value);

    switch (reg)
    {
    case DD_ASIC_DATA:
        dd->regs[DD_ASIC_DATA] = value;
        break;

    case DD_ASIC_CMD_STATUS:
        update_rtc(&dd->rtc);
        const struct tm* tm = localtime(&dd->rtc.now);

        /* base cycle count */
        cycles = 2000;

        switch ((value >> 16) & 0xff)
        {
        /* No-op */
        case 0x00:
            break;

        /* Seek track */
        case 0x01:
        case 0x02:
            /* base timing cycle count for Seek track CMD */
            cycles = 248250;
            /* get old track for calculating extra cycles */
            old_track = (dd->regs[DD_ASIC_CUR_TK] & 0x0fff0000) >> 16;
            /* update track */
            dd->regs[DD_ASIC_CUR_TK] = dd->regs[DD_ASIC_DATA];
            /* lock track */
            dd->regs[DD_ASIC_CUR_TK] |= DD_TRACK_LOCK;
            dd->bm_write = (value >> 17) & 0x1;
            /* update bm_zone */
            head  = (dd->regs[DD_ASIC_CUR_TK] & 0x10000000) >> 28;
            track = (dd->regs[DD_ASIC_CUR_TK] & 0x0fff0000) >> 16;
            dd->bm_zone = (get_zone_from_head_track(head, track) - head) + 8*head;
            /* calculate track to track head movement timing */
            cycles += 4825 * abs(track - old_track);
            break;

        /* Rezero / Start (Seek to track 0) */
        case 0x03:
        case 0x05:
            /* both commands do the exact same thing */
            /* base timing cycle count for Seek track CMD */
            cycles = 248250;
            /* get old track for calculating extra cycles */
            old_track = (dd->regs[DD_ASIC_CUR_TK] & 0x0fff0000) >> 16;
            /* update track to 0 */
            dd->regs[DD_ASIC_CUR_TK] = 0;
            /* lock track */
            dd->regs[DD_ASIC_CUR_TK] |= DD_TRACK_LOCK;
            dd->bm_write = 1;
            /* update bm_zone */
            head = (dd->regs[DD_ASIC_CUR_TK] & 0x10000000) >> 28;
            track = (dd->regs[DD_ASIC_CUR_TK] & 0x0fff0000) >> 16;
            dd->bm_zone = (get_zone_from_head_track(head, track) - head) + 8 * head;
            /* calculate track to track head movement timing */
            cycles += 4825 * abs(track - old_track);
            break;

        /* Sleep / Brake */
        case 0x04:
            if (dd->regs[DD_ASIC_DATA] == 0)
            {
                DebugMessage(M64MSG_VERBOSE, "Disk drive motor put to sleep mode");
            }
            else
            {
                DebugMessage(M64MSG_VERBOSE, "Disk drive motor put to brake mode");
            }
            break;

        /* Set standby delay */
        case 0x06:
            if ((dd->regs[DD_ASIC_DATA] & 0x01000000) == 0)
                DebugMessage(M64MSG_VERBOSE, "Set disk drive standby delay to %u seconds", (dd->regs[DD_ASIC_DATA] >> 16) & 0xff);
            else
                DebugMessage(M64MSG_VERBOSE, "Disable disk drive standby delay");
            break;

        /* Set sleep delay */
        case 0x07:
            if ((dd->regs[DD_ASIC_DATA] & 0x01000000) == 0)
                DebugMessage(M64MSG_VERBOSE, "Set disk drive sleep delay to %u seconds", (dd->regs[DD_ASIC_DATA] >> 16) & 0xff);
            else
                DebugMessage(M64MSG_VERBOSE, "Disable disk drive sleep delay");
            break;

        /* Clear Disk change flag */
        case 0x08:
            dd->regs[DD_ASIC_CMD_STATUS] &= ~DD_STATUS_DISK_CHNG;
            break;

        /* Clear reset flag */
        case 0x09:
            dd->regs[DD_ASIC_CMD_STATUS] &= ~DD_STATUS_RST_STATE;
            dd->regs[DD_ASIC_CMD_STATUS] &= ~DD_STATUS_DISK_CHNG;
            break;

        /* Read ASIC version */
        case 0x0a:
            if (dd->regs[DD_ASIC_DATA] == 0)
            {
                dd->regs[DD_ASIC_DATA] = 0x01140000;
                if (dd->disk->development)
                    dd->regs[DD_ASIC_DATA] |= 0x10000000;
            }
            else
            {
                dd->regs[DD_ASIC_DATA] = 0x53000000;
            }
            break;

        /* Set Disk type */
        case 0x0b:
            DebugMessage(M64MSG_VERBOSE, "Setting disk type %u", (dd->regs[DD_ASIC_DATA] >> 16) & 0xf);
            break;

        /* Request controller status */
        case 0x0c:
            dd->regs[DD_ASIC_DATA] = 0;
            break;

        /* Standby */
        case 0x0d:
            DebugMessage(M64MSG_VERBOSE, "Disk drive motor put to standby mode");
            break;

        /* Retry index lock */
        case 0x0e:
            DebugMessage(M64MSG_VERBOSE, "Retry disk track lock");
            break;

        /* Read RTC in ASIC_DATA (BCD format) */
        case 0x12:
            dd->regs[DD_ASIC_DATA] = time2data(tm->tm_year, tm->tm_mon + 1);
            break;
        case 0x13:
            dd->regs[DD_ASIC_DATA] = time2data(tm->tm_mday, tm->tm_hour);
            break;
        case 0x14:
            dd->regs[DD_ASIC_DATA] = time2data(tm->tm_min, tm->tm_sec);
            break;

        /* Feature inquiry */
        case 0x1b:
            dd->regs[DD_ASIC_DATA] = 0x00030000;
            break;

        default:
            DebugMessage(M64MSG_WARNING, "DD ASIC CMD not yet implemented (%08x)", value);
        }

        /* Signal a MECHA interrupt */
        cp0_update_count(dd->r4300);
        add_interrupt_event(&dd->r4300->cp0, DD_MC_INT, cycles);
        break;

    case DD_ASIC_BM_STATUS_CTL:
        /* set sector */
        dd->regs[DD_ASIC_CUR_SECTOR] = (value & 0x00ff0000);
        if (dd->regs[DD_ASIC_CUR_SECTOR] != 0 && dd->regs[DD_ASIC_CUR_SECTOR] != 0x005a0000) {
            DebugMessage(M64MSG_ERROR, "Start sector not aligned %08x", dd->regs[DD_ASIC_CUR_SECTOR]);
        }

        /* clear MECHA interrupt */
        if (value & DD_BM_CTL_MECHA_RST) {
            dd->regs[DD_ASIC_CMD_STATUS] &= ~DD_STATUS_MECHA_INT;
            remove_event(&dd->r4300->cp0.q, DD_MC_INT);
        }
        /* start block transfer */
        if (value & DD_BM_CTL_BLK_TRANS) {
            dd->regs[DD_ASIC_BM_STATUS_CTL] |= DD_BM_STATUS_BLOCK;
        }
        /* handle reset */
        if (value & DD_BM_CTL_RESET) {
            dd->bm_reset_held = 1;
        }
        if (!(value & DD_BM_CTL_RESET) && dd->bm_reset_held) {
            dd->bm_reset_held = 0;
            dd->regs[DD_ASIC_CMD_STATUS] &= ~(DD_STATUS_DATA_RQ
                                            | DD_STATUS_C2_XFER
                                            | DD_STATUS_BM_ERR
                                            | DD_STATUS_BM_INT);
            dd->regs[DD_ASIC_BM_STATUS_CTL] = 0;
            dd->regs[DD_ASIC_CUR_SECTOR] = 0;
            remove_event(&dd->r4300->cp0.q, DD_BM_INT);
        }

        /* clear DD interrupt if both MECHA and BM are cleared */
        if ((dd->regs[DD_ASIC_CMD_STATUS] & (DD_STATUS_BM_INT | DD_STATUS_MECHA_INT)) == 0) {
            clear_dd_interrupt(dd, DD_STATUS_BM_INT);
        }

        /* start transfer */
        if (value & DD_BM_CTL_START) {
            if (dd->bm_write && (value & DD_BM_CTL_MNGRMODE)) {
                DebugMessage(M64MSG_WARNING, "Attempt to write disk with BM mode 1");
            }
            if (!dd->bm_write && !(value & DD_BM_CTL_MNGRMODE)) {
                DebugMessage(M64MSG_WARNING, "Attempt to read disk with BM mode 0");
            }
            dd->regs[DD_ASIC_BM_STATUS_CTL] |= DD_BM_STATUS_RUNNING;
            add_interrupt_event(&dd->r4300->cp0, DD_BM_INT, 12500);
        }
        break;

    case DD_ASIC_HARD_RESET:
        if (value != 0xaaaa0000) {
            DebugMessage(M64MSG_WARNING, "Unexpected hard reset value %08x", value);
        }
        dd->regs[DD_ASIC_CMD_STATUS] |= DD_STATUS_RST_STATE;
        break;

    case DD_ASIC_HOST_SECBYTE:
        dd->regs[DD_ASIC_HOST_SECBYTE] = (value >> 16) & 0xff;
        if ((dd->regs[DD_ASIC_HOST_SECBYTE] + 1) != zone_sec_size[dd->bm_zone]) {
            DebugMessage(M64MSG_WARNING, "Sector size %u set different than expected %u",
                    dd->regs[DD_ASIC_HOST_SECBYTE] + 1, zone_sec_size[dd->bm_zone]);
        }
        break;

    case DD_ASIC_SEC_BYTE:
        dd->regs[DD_ASIC_SEC_BYTE] = (value >> 24) & 0xff;
        if (dd->regs[DD_ASIC_SEC_BYTE] != SECTORS_PER_BLOCK + 4) {
            DebugMessage(M64MSG_WARNING, "Sectors per block %u set different than expected %u",
                    dd->regs[DD_ASIC_SEC_BYTE] + 1, SECTORS_PER_BLOCK + 4);
        }
        break;

    case DD_ASIC_CUR_TK: /* fallthrough */
    case DD_ASIC_CUR_SECTOR:
        DebugMessage(M64MSG_WARNING, "Trying to write to read-only registers: %08x <- %08x", address, value);
        break;

    default:
        dd->regs[reg] = value;
    }
}


void read_dd_rom(void* opaque, uint32_t address, uint32_t* value)
{
    struct dd_controller* dd = (struct dd_controller*)opaque;
	uint32_t addr = dd_rom_address(address);

    *value = dd->rom[addr];

    DebugMessage(M64MSG_VERBOSE, "DD ROM: %08X -> %08x", address, *value);
}

void write_dd_rom(void* opaque, uint32_t address, uint32_t value, uint32_t mask)
{
    DebugMessage(M64MSG_VERBOSE, "DD ROM: %08X <- %08x & %08x", address, value, mask);
}

unsigned int dd_dom_dma_read(void* opaque, const uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    struct dd_controller* dd = (struct dd_controller*)opaque;
    uint8_t* mem;
    size_t i;

    DebugMessage(M64MSG_VERBOSE, "DD DMA read dram=%08x  cart=%08x length=%08x",
            dram_addr, cart_addr, length);

    if (cart_addr == MM_DD_DS_BUFFER) {
        cart_addr = (cart_addr - MM_DD_DS_BUFFER) & 0x3fffff;
        mem = dd->ds_buf;
    }
    else if (cart_addr == MM_DD_MS_RAM) {
        /* MS is not emulated, we silence warnings for now */
        /* Recommended Count Per Op = 1, this seems to break very easily */
        return (length * 63) / 25;
    }
    else {
        DebugMessage(M64MSG_ERROR, "Unknown DD dma read dram=%08x  cart=%08x length=%08x",
            dram_addr, cart_addr, length);

        /* Recommended Count Per Op = 1, this seems to break very easily */
        return (length * 63) / 25;
    }

    for (i = 0; i < length; ++i) {
        mem[(cart_addr + i) ^ S8] = dram[(dram_addr + i) ^ S8];
    }

    /* Recommended Count Per Op = 1, this seems to break very easily */
    return (length * 63) / 25;
}

unsigned int dd_dom_dma_write(void* opaque, uint8_t* dram, uint32_t dram_addr, uint32_t cart_addr, uint32_t length)
{
    struct dd_controller* dd = (struct dd_controller*)opaque;
    unsigned int cycles;
    const uint8_t* mem;
    size_t i;

    DebugMessage(M64MSG_VERBOSE, "DD DMA write dram=%08x  cart=%08x length=%08x",
            dram_addr, cart_addr, length);

    if (cart_addr < MM_DD_ROM) {
        if (cart_addr == MM_DD_C2S_BUFFER) {
            /* C2 sector buffer */
            cart_addr = (cart_addr - MM_DD_C2S_BUFFER);
            mem = (const uint8_t*)&dd->c2s_buf;
        }
        else if (cart_addr == MM_DD_DS_BUFFER) {
            /* Data sector buffer */
            cart_addr = (cart_addr - MM_DD_DS_BUFFER);
            mem = (const uint8_t*)&dd->ds_buf;
        }
        else {
            DebugMessage(M64MSG_ERROR, "Unknown DD dma write dram=%08x  cart=%08x length=%08x",
                dram_addr, cart_addr, length);

            /* Recommended Count Per Op = 1, this seems to break very easily */
            return (length * 63) / 25;
        }

        /* Recommended Count Per Op = 1, this seems to break very easily */
        cycles = (length * 63) / 25;
    }
    else {
        /* DD ROM */
        cart_addr = (cart_addr - MM_DD_ROM);
        mem = (const uint8_t*)dd->rom;

        /* Recommended Count Per Op = 1, this seems to break very easily */
        cycles = (length * 63) / 25;
    }

    for (i = 0; i < length; ++i) {
        dram[(dram_addr + i) ^ S8] = mem[(cart_addr + i) ^ S8];
    }

    invalidate_r4300_cached_code(dd->r4300, R4300_KSEG0 + dram_addr, length);
    invalidate_r4300_cached_code(dd->r4300, R4300_KSEG1 + dram_addr, length);

    return cycles;
}

