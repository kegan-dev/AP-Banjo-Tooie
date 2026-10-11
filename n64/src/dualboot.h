#ifndef DUALBOOT_H
#define DUALBOOT_H

#include "util.h"

/* BKBT dual-boot: a relocated Banjo-Kazooie sits at a cart offset in the combined image. The offset is
   DUALBOOT_BK_BASE in asm/dualboot.asm, the only copy in the hack. */

/* Handoff block: survives a warm start (inside the payload window, never DMA'd over). */
#define DUALBOOT_HANDOFF ((vu32*)0x805FFF00)
#define DUALBOOT_MAGIC 0x424B4254 /* 'BKBT' */
#define DUALBOOT_GAME_TOOIE 1
#define DUALBOOT_GAME_KAZOOIE 2

void dualboot_launch_bk(void);   /* asm/dualboot.asm, never returns */
void dualboot_boot_check(void);  /* at payload start: honour a pending handoff */
void dualboot_request_bk(void);  /* record the handoff and switch now */

#endif
