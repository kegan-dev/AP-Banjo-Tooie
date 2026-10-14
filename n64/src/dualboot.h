#ifndef DUALBOOT_H
#define DUALBOOT_H

#include "util.h"

/* BKBT dual-boot: Banjo-Kazooie sits at a cart offset in the combined image, not relocated; the
   launcher points osRomBase at it. The offset is DUALBOOT_BK_BASE in asm/dualboot.asm, the only copy in
   the hack; C reads it from the word of the launcher's BKBTBASE tag (dualboot_bk_base_word). */

/* Dual-boot contract number, as this C understands the handoff block. The launcher has a number of its
   own (DUALBOOT_CONTRACT in asm/dualboot.asm: its BKBTCONT tag and the launch token), and no build step
   can compare an armips equ with a C macro. So this one is published where a tool sees both: it is the
   fourth word of the BKBTAUTO record (dualboot.c), and the superproject's tools/make_dualboot_image.py
   refuses a ROM in which it differs from the tag. Change both. The Banjo-Kazooie payload (its header,
   hack/bk/asm/payload.asm in the superproject) and the Banjo-Kazooie fork (bkbt_contract,
   hack/bk/decomp in the superproject) carry the number too, and the same tool compares those. */
#define DUALBOOT_CONTRACT 2

/* Handoff block: survives a warm start in either direction (top of the payload window, never DMA'd
   over, above everything Banjo-Kazooie uses). The Banjo-Kazooie side of it is
   hack/bk/decomp/include/bkbt.h in the superproject (the fork's C) and the payload's return launcher
   (hack/bk/asm/return.asm); both run while this payload's data is stale: keep them in step.
   Banjo-Kazooie reads and writes the block uncached only (KSEG1, 0xA05FFF00): the return launcher
   writes it uncached and then writes the caches back, so a dirty line on BK's side would land on top
   of it.
   This side's C uses KSEG0. That is right for the words C stores, +0x00, +0x04, +0x08, +0x0C, +0x28 and
   +0x2C, and for no others. asm/dualboot.asm writes the caches back once and only after that stores
   its own words of those two cache lines (the launch counter, the token, the payload address) through
   KSEG1. The trace words +0x10 and +0x14 are not handled that way: the quiesce and the launcher write
   them uncached from their first instructions on, before the write-back. A C store through KSEG0 into
   that cache line (+0x10..+0x1C, which includes the reserved destination words) would leave the line
   dirty with the trace of an older launch in it, and the write-back would put that over the trace
   written so far. So C must not store there through KSEG0; whoever brings the destination words into
   use stores them through KSEG1.
     +0x00  magic 'BKBT' while a handoff is pending
     +0x04  game to start (DUALBOOT_GAME_*)
     +0x08  Banjo-Kazooie launches since the last cold boot (asm/dualboot.asm counts)
     +0x0C  Tooie launches from Banjo-Kazooie since the last cold boot (the BK payload counts)
     +0x10  launcher trace: stage      +0x14  launcher trace: spin counter
     +0x18  reserved: destination map  +0x1C  reserved: destination exit
     +0x20  launch token (DUALBOOT_TOKEN in asm/dualboot.asm, which writes it on every launch of
            Banjo-Kazooie); BK's bkbt_init takes it as proof that the launcher has just started it, and
            zeroes it. C never touches it
     +0x24  RAM address (KSEG0) of the payload header asm/dualboot.asm loaded in this launch, 0 if
            none; bkbt_init zeroes it. C never touches it
     +0x28  autotest: Banjo-Kazooie main-loop iterations before it returns by itself (0 = stay)
     +0x2C  flags for Banjo-Kazooie, latched by bkbt_init (BKBT_FLAG_* in the fork's bkbt.h: bit 0 =
            L + R + Start also returns outside BK's front end) */
#define DUALBOOT_HANDOFF ((vu32*)0x805FFF00)
#define DUALBOOT_MAGIC 0x424B4254 /* 'BKBT' */
#define DUALBOOT_GAME_TOOIE 1
#define DUALBOOT_GAME_KAZOOIE 2
#define DUALBOOT_BK_LAUNCHES (DUALBOOT_HANDOFF[2])
#define DUALBOOT_BT_LAUNCHES (DUALBOOT_HANDOFF[3])
#define DUALBOOT_BK_COUNTDOWN (DUALBOOT_HANDOFF[10])
#define DUALBOOT_BK_FLAGS_WORD (DUALBOOT_HANDOFF[11])

/* The save device is shared too: the cartridge's 32 KiB of SRAM (cart address 0x08000000). This hack keeps
   save_data at offset 0 (save.c). Banjo-Kazooie, when the launcher started it, keeps its 512 bytes of save
   from this offset on, so this hack must never read, write or clear SRAM from here up; save.c stops the
   build if save_data would reach it. The fork's copy of the number is bkbt_sram_offset in
   hack/bk/decomp/manual_syms.us.v10.txt in the superproject, whose link stops below 0x7000. Nothing
   compares the two: change both. */
#define DUALBOOT_BK_SRAM_OFFSET 0x7000

/* Test builds (EXTRA_CFLAGS, see hack/DUALBOOT.md in the superproject):
     -DDUALBOOT_AUTOTEST            switch to Banjo-Kazooie by itself from the title screen
     -DDUALBOOT_AUTOTEST_TRIPS=n    ... at most n times per cold boot (default 1)
     -DDUALBOOT_AUTOTEST_RETURN=n   Banjo-Kazooie returns by itself after n main-loop iterations
     -DDUALBOOT_BK_FLAGS=n          the flags word handed to Banjo-Kazooie (default 0)
   The first three are published in the ROM for the tools (the BKBTAUTO record in dualboot.c). */
#ifndef DUALBOOT_AUTOTEST_TRIPS
#define DUALBOOT_AUTOTEST_TRIPS 1
#endif
#ifndef DUALBOOT_AUTOTEST_RETURN
#define DUALBOOT_AUTOTEST_RETURN 0
#endif
#ifndef DUALBOOT_BK_FLAGS
#define DUALBOOT_BK_FLAGS 0
#endif

/* Each of the three C entries starts Banjo-Kazooie only if one is there (dualboot.c, dualboot_start_bk);
   in the hack ROM run by itself they do nothing. */
void dualboot_launch_bk(void);       /* asm/dualboot.asm, never returns; dualboot.c is its only caller */
void dualboot_boot_check(void);      /* at payload start: honour a pending handoff */
void dualboot_request_bk(void);      /* record the handoff and switch now */
void dualboot_title_chord(u32 down); /* every title-screen frame: is L + R + Start down on pad 0? */

#endif
