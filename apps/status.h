/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 Linus Nielsen Feltzing
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#ifndef _STATUS_H
#define _STATUS_H

enum playmode
{
    STATUS_PLAY,
    STATUS_STOP,
    STATUS_PAUSE,
    STATUS_FASTFORWARD,
    STATUS_FASTBACKWARD,
    STATUS_RECORD,
    STATUS_RECORD_PAUSE
};

void status_init(void);
void status_set_playmode(enum playmode mode);
#ifdef HAVE_LCD_BITMAP
extern bool statusbar_enabled;
bool statusbar(bool state);
#endif
void status_draw(void);

#if defined(HAVE_LCD_CHARCELLS)
void status_set_record(bool b);
void status_set_audio(bool b);
void status_set_param(bool b);
void status_set_usb(bool b);
#endif

#endif
