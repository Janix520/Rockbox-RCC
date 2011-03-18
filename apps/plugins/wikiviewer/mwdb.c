/***************************************************************************
 *
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 *
 *
 * Copyright (C) 2006 Frederik M.J. Vestre
 * Copyright (C) 2006 Adam Gashlin
 *
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

#include "mallocer.h" /*mallocer resetable memory allocator*/
#include "inflate.h"
#include "btsearch.h"
#include <inttypes.h>

/*Remember to require this:
extern struct plugin_api *rb;

Matthias Larisch: This is already done in btsearch.h. maybe it should be moved
*/

unsigned int mwdb_p_xtpt;

static int mwdb_nprintf(const char *fmt, ...)
{
    char p_buf[50];
    bool ok;
    va_list ap;

    va_start(ap, fmt);
    ok = rb->vsnprintf(p_buf,sizeof(p_buf), fmt, ap);
    va_end(ap);

    rb->lcd_putsxy(1,mwdb_p_xtpt, (unsigned char *)p_buf);
    rb->lcd_update();

    mwdb_p_xtpt+=rb->font_get(FONT_UI)->height;
    if(mwdb_p_xtpt>LCD_HEIGHT-rb->font_get(FONT_UI)->height)
    {
        mwdb_p_xtpt=0;
        rb->lcd_clear_display();
    }
    return 1;
}

int mwdb_findarticle(const char* filename,
                     char* artnme,
                     uint16_t artnmelen,
                     uint32_t * res_lo,
                     uint32_t * res_hi,
                     bool progress,
                     bool promptname,
                     bool casesense){
    int fd;
    char fnbuf[rb->strlen(filename)+6];

    if(promptname) {
        if (rb->kbd_input(artnme,artnmelen)) {
            artnme[0]='\0';
            return false;  /* proper abort on keyboard abort */
        }
    }

    rb->snprintf(fnbuf,rb->strlen(filename)+6,"%s.wwi",filename);
    fd = rb->open(fnbuf, 0);

    if (fd==-1)
        return false;

    if(progress){
        mwdb_p_xtpt=0;
        rb->lcd_clear_display();
        mwdb_nprintf("Searching...");
    }

    search_btree(fd,artnme,rb->strlen(artnme),0,res_lo,res_hi,casesense);
    rb->close(fd);

    if(*res_hi==0){ /* not found */
        return false;
    }else if(progress)
        mwdb_nprintf("Found:%d%d",*res_lo,*res_hi);

    return true;
}


int mwdb_loadarticle(const char * filename,
                     void * scratchmem,
                     void * articlemem,
                     uint32_t articlememlen,
                     uint32_t *articlelen,
                     uint32_t res_lo,
                     uint32_t res_hi) {
    char fnbuf[rb->strlen(filename)+6];

    mwdb_p_xtpt=0;
    rb->lcd_clear_display();
    mwdb_nprintf("Loading %d%d...",res_lo,res_hi);

    rb->snprintf(fnbuf,rb->strlen(filename)+6,"%s%lu.wwa",filename,res_lo);

    *articlelen=decompress(fnbuf,articlemem,articlememlen,res_hi,scratchmem);

    ((unsigned char*)articlemem)[*articlelen]='\0';

    return true;
}
