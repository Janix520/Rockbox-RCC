/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2002 by Ulf Ralberg
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/
#include "thread.h"

typedef union
{
    struct regs_t
    {
        unsigned int  r[7]; /* Registers r8 thru r14 */
        void          *sp;  /* Stack pointer (r15) */
        unsigned int  mach;
        unsigned int  macl;
        unsigned int  sr;   /* Status register */
        void*         pr;   /* Procedure register */
    } regs;
    unsigned int mem[12];
} ctx_t;

typedef struct
{
    int   created;
    int   current;
    ctx_t ctx[MAXTHREADS] __attribute__ ((aligned (32)));
} thread_t;

static thread_t threads = {1, 0};

/*--------------------------------------------------------------------------- 
 * Store non-volatile context.
 *---------------------------------------------------------------------------
 */
static inline void stctx(void* addr)
{
    asm volatile ("add #48, %0\n\t"
                  "sts.l pr,  @-%0\n\t"
                  "stc.l sr,  @-%0\n\t"
                  "sts.l macl,@-%0\n\t"
                  "sts.l mach,@-%0\n\t"
                  "mov.l r15, @-%0\n\t"
                  "mov.l r14, @-%0\n\t"
                  "mov.l r13, @-%0\n\t"
                  "mov.l r12, @-%0\n\t"
                  "mov.l r11, @-%0\n\t"
                  "mov.l r10, @-%0\n\t"
                  "mov.l r9,  @-%0\n\t"
                  "mov.l r8,  @-%0" : : "r" (addr));
}

/*--------------------------------------------------------------------------- 
 * Load non-volatile context.
 *---------------------------------------------------------------------------
 */
static inline void ldctx(void* addr)
{
    asm volatile ("mov.l @%0+,r8\n\t"
                  "mov.l @%0+,r9\n\t"
                  "mov.l @%0+,r10\n\t"
                  "mov.l @%0+,r11\n\t"
                  "mov.l @%0+,r12\n\t"
                  "mov.l @%0+,r13\n\t"
                  "mov.l @%0+,r14\n\t"
                  "mov.l @%0+,r15\n\t"
                  "lds.l @%0+,mach\n\t"
                  "lds.l @%0+,macl\n\t"
                  "ldc.l @%0+,sr\n\t"
                  "mov.l @%0,%0\n\t"
                  "lds %0,pr\n\t"
                  "mov.l %0, @(0, r15)" : "+r" (addr));
}

/*--------------------------------------------------------------------------- 
 * Switch thread in round robin fashion.
 *---------------------------------------------------------------------------
 */
void
switch_thread(void)
{
    int        ct;
    int        nt;
    thread_t*  t = &threads;

    nt = ct = t->current;
    if (++nt >= t->created)
        nt = 0;
    t->current = nt;
    stctx(&t->ctx[ct]);
    ldctx(&t->ctx[nt]);
}

/*--------------------------------------------------------------------------- 
 * Create thread.
 * Return 0 if context area could be allocated, else -1.
 *---------------------------------------------------------------------------
 */
int create_thread(void* fp, void* sp, int stk_size)
{
    thread_t* t = &threads;

    if (t->created >= MAXTHREADS)
        return -1;
    else
    {
        ctx_t* ctxp = &t->ctx[t->created++];
        stctx(ctxp);
        ctxp->regs.sp = (void*)(((unsigned int)sp + stk_size) & ~31);
        ctxp->regs.pr = fp;
    }
    return 0;
}

struct event
{
    int id;
    void *data;
};

struct event_queue
{
    struct event events[16];
    unsigned int read;
    unsigned int write;
};

void queue_init(struct event_queue *q)
{
    q->read = 0;
    q->write = 0;
}

struct event *wait_queue(struct event_queue *q)
{
    while(q->read == q->write)
    {
	switch_thread();
    }

    return &q->events[q->read++];
}

void post_queue(struct event_queue *q, int id, void *data)
{
    q->events[q->write].id = id;
    q->events[q->write].data = data;
    q->write++;
}
