#ifndef DUALBOOT_H
#define DUALBOOT_H

#include "util.h"

/* BKBT dual-boot: a relocated Banjo-Kazooie sits at a cart offset in the combined image. The offset is
   DUALBOOT_BK_BASE in asm/dualboot.asm, the only copy in the hack. */

/* Handoff block: survives a warm start in either direction (top of the payload window, never DMA'd
   over, above everything Banjo-Kazooie uses). Shared with the Banjo-Kazooie payload in the superproject
   (hack/bk/asm/payload.asm), which runs while this payload's data is stale: keep the two in step.
     +0x00  magic 'BKBT' while a handoff is pending
     +0x04  game to start (DUALBOOT_GAME_*)
     +0x08  Banjo-Kazooie launches since the last cold boot (asm/dualboot.asm counts)
     +0x0C  Tooie launches from Banjo-Kazooie since the last cold boot (the BK payload counts)
     +0x10  launcher trace: stage      +0x14  launcher trace: spin counter
     +0x18  reserved: destination map  +0x1C  reserved: destination exit
     +0x28  autotest: Banjo-Kazooie main-loop iterations before it returns by itself (0 = stay) */
#define DUALBOOT_HANDOFF ((vu32*)0x805FFF00)
#define DUALBOOT_MAGIC 0x424B4254 /* 'BKBT' */
#define DUALBOOT_GAME_TOOIE 1
#define DUALBOOT_GAME_KAZOOIE 2
#define DUALBOOT_BK_LAUNCHES (DUALBOOT_HANDOFF[2])
#define DUALBOOT_BT_LAUNCHES (DUALBOOT_HANDOFF[3])
#define DUALBOOT_BK_COUNTDOWN (DUALBOOT_HANDOFF[10])

/* Test builds (EXTRA_CFLAGS, see hack/DUALBOOT.md in the superproject):
     -DDUALBOOT_AUTOTEST            switch to Banjo-Kazooie by itself from the title screen
     -DDUALBOOT_AUTOTEST_TRIPS=n    ... at most n times per cold boot (default 1)
     -DDUALBOOT_AUTOTEST_RETURN=n   Banjo-Kazooie returns by itself after n main-loop iterations */
#ifndef DUALBOOT_AUTOTEST_TRIPS
#define DUALBOOT_AUTOTEST_TRIPS 1
#endif
#ifndef DUALBOOT_AUTOTEST_RETURN
#define DUALBOOT_AUTOTEST_RETURN 0
#endif

void dualboot_launch_bk(void);   /* asm/dualboot.asm, never returns */
void dualboot_boot_check(void);  /* at payload start: honour a pending handoff */
void dualboot_request_bk(void);  /* record the handoff and switch now */

#endif
