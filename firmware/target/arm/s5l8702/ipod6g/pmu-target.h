/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id: pmu-target.h 24721 2010-02-17 15:54:48Z theseven $
 *
 * Copyright © 2009 Michael Sparmann
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#ifndef __PMU_TARGET_H__
#define __PMU_TARGET_H__

#include <stdbool.h>
#include "config.h"

#include "pcf5063x.h"

/* undocummented PMU registers */
#define PCF50635_REG_INT6        0x85
#define PCF50635_REG_INT6M       0x86
#define PCF50635_REG_GPIOSTAT    0x87  /* bit1: GPIO2 status (TBC) */

/* LDOs */
#define LDO_UNK1        1   /* TBC: SoC voltage (USB) */
#define LDO_UNK2        2   /* TBC: SoC voltage (I/O) */
#define LDO_LCD         3
#define LDO_CODEC       4
#define LDO_UNK5        5   /* TBC: nano3g NAND */
#define LDO_CWHEEL      6
#define LDO_ACCY        7   /* HCLDO */

/* Other LDOs:
 *  AUTOLDO: Hard Disk
 *  DOWN1: CPU
 *  DOWN2: SDRAM
 *  MEMLDO: SDRAM self-refresh (TBC)
 *
 * EXTON inputs:
 *  EXTON1: button/holdswitch related (TBC)
 *  EXTON2: USB Vbus (High when present)
 *  EXTON3: ACCESORY (Low when present)
 *
 * GPIO:
 *  GPIO1: input, Mikey (jack remote ctrl) interrupt (TBC)
 *  GPIO2: input, hold switch (TBC)
 *  GPIO3: output, unknown
 */


unsigned char pmu_read(int address);
int pmu_write(int address, unsigned char val);
int pmu_read_multiple(int address, int count, unsigned char* buffer);
int pmu_write_multiple(int address, int count, unsigned char* buffer);
int pmu_read_adc(unsigned int adc);
int pmu_read_battery_voltage(void);
int pmu_read_battery_current(void);
void pmu_init(void);
void pmu_ldo_on_in_standby(unsigned int ldo, int onoff);
void pmu_ldo_set_voltage(unsigned int ldo, unsigned char voltage);
void pmu_ldo_power_on(unsigned int ldo);
void pmu_ldo_power_off(unsigned int ldo);
void pmu_set_wake_condition(unsigned char condition);
void pmu_enter_standby(void);
void pmu_read_rtc(unsigned char* buffer);
void pmu_write_rtc(unsigned char* buffer);
void pmu_hdd_power(bool on);

#ifdef BOOTLOADER
unsigned char pmu_rd(int address);
int pmu_wr(int address, unsigned char val);
int pmu_rd_multiple(int address, int count, unsigned char* buffer);
int pmu_wr_multiple(int address, int count, unsigned char* buffer);
#endif

#endif /* __PMU_TARGET_H__ */
