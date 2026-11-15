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
#define DUALBOOT_CONTRACT 5

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
   that cache line (+0x10..+0x1C, which includes the destination words) would leave the line dirty with
   the trace of an older launch in it, and the write-back would put that over the trace written so far.
   So C must not store there through KSEG0: it reads and writes the destination words through KSEG1
   only (DUALBOOT_HANDOFF_UNCACHED), as BK does.
     +0x00  magic 'BKBT' while a handoff is pending
     +0x04  game to start (DUALBOOT_GAME_*)
     +0x08  Banjo-Kazooie launches since the last cold boot (asm/dualboot.asm counts)
     +0x0C  Tooie launches from Banjo-Kazooie since the last cold boot (the BK payload counts)
     +0x10  launcher trace: stage      +0x14  launcher trace: spin counter
     +0x18  destination (contract 3): DUALBOOT_DEST_TAG | the map to arrive at in the game started, in
            that game's numbering; 0 for none (the chord, the autotest, the relaunch). The C of the game
            being left writes it, through KSEG1, on every launch; the game started reads it and zeroes it
            at boot, and believes it only with its proof of a launch (Tooie: the magic; BK: the token)
     +0x1C  destination's entrance, with +0x18 (0 with no destination)
     +0x20  launch token (DUALBOOT_TOKEN in asm/dualboot.asm, which writes it on every launch of
            Banjo-Kazooie); BK's bkbt_init takes it as proof that the launcher has just started it, and
            zeroes it. C never touches it
     +0x24  RAM address (KSEG0) of the payload header asm/dualboot.asm loaded in this launch, 0 if
            none; bkbt_init zeroes it. C never touches it
     +0x28  autotest: Banjo-Kazooie main-loop iterations before it returns by itself (0 = stay)
     +0x2C  flags for Banjo-Kazooie, latched by bkbt_init (BKBT_FLAG_* in the fork's bkbt.h: bit 0 =
            L + R + Start also returns outside BK's front end)
     +0x30  link (contract 4): with a destination, the link of the file that goes through the door
            (save_data.link, save.h): to Banjo-Kazooie this file's, back from it the one its save
            carries; 0 with none. Like the destination: written through KSEG1 by the game being left on
            every launch, read and zeroed by the game started, believed only with its proof of a launch
     +0x34, +0x38, +0x3C  to Banjo-Kazooie with a destination: the links of this game's three files
            (0 for none), so that it knows which of its saves no file holds any more; 0 otherwise.
            The same rules; this line, +0x30..+0x3C, is KSEG1 only on both sides */
#define DUALBOOT_HANDOFF ((vu32*)0x805FFF00)
#define DUALBOOT_MAGIC 0x424B4254 /* 'BKBT' */
#define DUALBOOT_GAME_TOOIE 1
#define DUALBOOT_GAME_KAZOOIE 2
#define DUALBOOT_BK_LAUNCHES (DUALBOOT_HANDOFF[2])
#define DUALBOOT_BT_LAUNCHES (DUALBOOT_HANDOFF[3])
#define DUALBOOT_BK_COUNTDOWN (DUALBOOT_HANDOFF[10])
#define DUALBOOT_BK_FLAGS_WORD (DUALBOOT_HANDOFF[11])
/* The destination words, through KSEG1 only (above) */
#define DUALBOOT_HANDOFF_UNCACHED ((vu32*)0xA05FFF00)
#define DUALBOOT_DEST_MAP (DUALBOOT_HANDOFF_UNCACHED[6])
#define DUALBOOT_DEST_EXIT (DUALBOOT_HANDOFF_UNCACHED[7])
#define DUALBOOT_DEST_TAG 0x44530000 /* 'DS' in the high half; the map in the low */
/* The links (contract 4), through KSEG1 only (above): the file at the door, and the three files' links */
#define DUALBOOT_LINK (DUALBOOT_HANDOFF_UNCACHED[12])
#define DUALBOOT_FILE_LINK(i) (DUALBOOT_HANDOFF_UNCACHED[13 + (i)])

/* The hub door (docs/ROADMAP.md in the superproject, "The way there", step 2): Spiral Mountain's entrance to
   the ruined Lair (BT_MAP_SM -> 0x0AD) leads into Banjo-Kazooie's Lair, MAP_69_GL_MM_LOBBY at
   WARP_GL_MM_LOBBY_12_ENTRANCE (the fork's include/enums.h), and BK's Lair exit back to Spiral Mountain at
   the entrance out of the ruined Lair, exit 2 (the game asks (0x0AF, 2) when Banjo walks out of 0x0AD:
   mupen64plus, 2026-10-31; he then walks in by himself and stands at (0, 1784, -3520)). door.c. */
#define DUALBOOT_DOOR_BK_MAP 0x69
#define DUALBOOT_DOOR_BK_EXIT 0x12
#define DUALBOOT_DOOR_TOOIE_EXIT 2

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
     -DDUALBOOT_DOOR_TEST=n         the hub door without a player (door.c): the first boot arrives as if
                                    through the door, and n frames after Banjo is in Spiral Mountain the
                                    game is sent through it; with DUALBOOT_BK_FLAGS bit 1 BK takes its
                                    Lair exit DUALBOOT_AUTOTEST_RETURN iterations after it is in the Lair;
                                    at most DUALBOOT_AUTOTEST_TRIPS trips per cold boot
     -DDUALBOOT_CROSS_TEST=n        with DUALBOOT_DOOR_TEST: n frames after Banjo is in a map with an exit_map
                                    entry into Banjo-Kazooie, the game takes it as that entry's exit guard would
                                    (door.c), within the same trips
     -DDUALBOOT_CHEATO_TEST=n       with DUALBOOT_DOOR_TEST: the spawns of each map in a record for the check
                                    (BKBTCHTO, cheato.c), n frames after Cheato is spawned in the throne room Banjo
                                    is moved next to him, and the check's reload byte loads the throne room again
   The first three are published in the ROM for the tools (the BKBTAUTO record in dualboot.c; a door test
   build says autotest 2, a cross test build 3). */
#ifndef DUALBOOT_AUTOTEST_TRIPS
#define DUALBOOT_AUTOTEST_TRIPS 1
#endif
#ifndef DUALBOOT_AUTOTEST_RETURN
#define DUALBOOT_AUTOTEST_RETURN 0
#endif
#ifndef DUALBOOT_BK_FLAGS
#define DUALBOOT_BK_FLAGS 0
#endif

/* Each of the C entries that start Banjo-Kazooie does so only if one is there (dualboot.c,
   dualboot_start_bk); in the hack ROM run by itself they do nothing. */
void dualboot_launch_bk(void);       /* asm/dualboot.asm, never returns; dualboot.c is its only caller */
void dualboot_boot_check(void);      /* at payload start: honour a pending handoff */
void dualboot_request_bk(void);      /* record the handoff and switch now */
void dualboot_request_bk_to(u32 map, u32 exit); /* ... arriving at (map, exit) in Banjo-Kazooie */
u32 dualboot_bk_present(void);       /* is there a Banjo-Kazooie at its cart offset? */
void dualboot_title_chord(u32 down); /* every title-screen frame: is L + R + Start down on pad 0? */
/* The arrival this boot came with (dualboot_boot_check): 1 and its map and entrance in Tooie's numbering,
   or 0. Zero again at every boot (the boot stub reloads the payload's data). */
extern u8 dualboot_arrival;
extern u16 dualboot_arrival_map;
extern u16 dualboot_arrival_exit;
extern u32 dualboot_arrival_link;    /* the link of the save that came back (+0x30), 0 for none */

/* The doors' code (door.c): the hub door, and since contract 5 the doors between the games, exit_map entries whose
   exit_game says Banjo-Kazooie (shared/include/ap_memory/pc.h). exits_can_pass hands such an entry to
   door_cross_pass, which keeps it for the load the guard then asks for, and says 0 when there is no Banjo-Kazooie
   to go to. */
u8 door_cross_pass(const ap_memory_pc_exit_map_t* entry, const ap_memory_pc_exit_game_t* game);
void door_pre_load_scene(u16* scene, u16* exit); /* the first thing pre_load_scene does */
void door_arrival_scene(u16* scene, u16* exit);  /* the last thing pre_load_scene does */
void door_loop(void);                            /* post_loop */
void door_hooks(void);                           /* inject_hooks: the curtain's two hooks */
u8 door_direct_load(void);     /* is this load a door arrival's direct load (from the title or the file select)? */
u8 door_direct_new_file(void); /* post_load_save: did a door arrival's direct load read a new file? */

#endif
