/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2013 Andrew Ryabinin
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

#include "audiohw.h"
#include "df1704.h"
#include "pca9555.h"
#include "i2c-rk27xx.h"
#include "system.h"

void df1704_set_ml_dir(const int dir)
{
    pca9555_write_config(dir<<8, (1<<8));
}

void df1704_set_ml(const int val)
{
    pca9555_write_output(val<<8, 1<<8);
}

void df1704_set_mc(const int val)
{
    pca9555_write_output(val<<1, 1<<1);
}

void df1704_set_md(const int val)
{
    pca9555_write_output(val<<0, 1<<0);
}

static void pop_ctrl(const int val)
{
    pca9555_write_output(val<<5, 1<<5);
}

static void amp_enable(const int val)
{
    pca9555_write_output(val<<3, 1<<3);
}

static void df1704_enable(const int val)
{
    pca9555_write_output(val<<4, 1<<4);
}


void audiohw_postinit(void)
{
    pop_ctrl(0);
    sleep(HZ/4);
    df1704_enable(1);
    amp_enable(1);
    sleep(HZ/100);
    df1704_init();
    sleep(HZ/4);
    pop_ctrl(1);
}

void audiohw_close(void)
{
    df1704_mute();
    pop_ctrl(0);
    sleep(HZ/5);
    amp_enable(0);
    df1704_enable(0);
    sleep(HZ/5);
    pop_ctrl(1);
}
