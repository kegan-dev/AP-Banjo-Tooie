; Dual-boot launcher (BKBT). Warm-starts the Banjo-Kazooie that lives at cart offset DUALBOOT_BK_BASE
; in the combined image. That Banjo-Kazooie is not relocated: its libultra forms every cart address as
; osRomBase | offset, so the launcher points osRomBase at it. Runs from the AP payload (0x804xxxxx),
; which the DMA below does not touch. tools/make_dualboot_image.py reads where BK and its payload are,
; and which dual-boot contract this launcher speaks, from the tags at the end of this file, and refuses
; an image that does not agree with them.
;
; Called from C as  void dualboot_launch_bk(void);  Never returns. Nothing here looks whether there is
; a Banjo-Kazooie at DUALBOOT_BK_BASE, and the first thing the quiesce does is a PI DMA from there: the
; one caller, dualboot_start_bk in src/dualboot.c, has looked (the hack ROM also runs by itself).
;
; Sequence: interrupts off (so no thread can start new work), wait for the RSP, RDP, AI and PI to go
; idle by themselves (emulators such as Project64 run RSP tasks on a timer *after* the game starts
; them, so forcibly halting the RSP leaves a task half-done), optionally mask/silence, DMA BK's boot
; segment over the first MiB like IPL3 would, point osRomBase at BK, recreate what BK's CIC-6103 IPL3
; leaves in low RDRAM (the anti-tamper of an unmodified BK reads it back; a BK built from the fork,
; hack/bk/decomp in the superproject, has none), zero the rest of BK's 4 MiB, load the Banjo-Kazooie
; payload (the superproject's hack/bk) into Expansion Pak RAM, tell BK through the handoff block that
; the launcher started it and where the payload is, withdraw the AP anchor, drop the caches, jump to
; BK's entry.
; The way back is not here: Banjo-Kazooie decides when (bkbt_frame in the fork) and calls the payload,
; which launches Tooie (hack/bk/asm/return.asm).
;
; armips matches `equ` names without regard to case: no label may be spelled like one of the toggles.

; Dual-boot contract number. The launcher (the BKBTCONT tag and the launch token below), the Banjo-Kazooie
; payload (its header, hack/bk/asm/payload.asm) and the Banjo-Kazooie fork (bkbt_contract) each carry it,
; and the image builder compares them. The hack's C has a number of its own (DUALBOOT_CONTRACT in
; src/dualboot.h), which no build step can compare with this one; C publishes it as the fourth word of
; its BKBTAUTO record (src/dualboot.c) and the image builder refuses a ROM in which the two differ.
; Change both.
DUALBOOT_CONTRACT equ 3
; what the launcher leaves at handoff + 0x20 for Banjo-Kazooie on every launch: 'B','K','G',contract
DUALBOOT_TOKEN equ (0x424B4700 | DUALBOOT_CONTRACT)

; Cart offset of Banjo-Kazooie. 16 MiB aligned, because BK's cart addresses are osRomBase | offset and
; only on such a boundary is that OR an addition for every offset inside a 16 MiB ROM (hack/bk/README.md,
; "Cart offset without relocation": an unaligned BK shows no frame). And BK has to end inside the image,
; which is 64 MiB, the ceiling emulators and flashcarts enforce (docs/ROADMAP.md in the superproject).
DUALBOOT_BK_BASE equ 0x03000000
.if (DUALBOOT_BK_BASE & 0x00FFFFFF) != 0
.error "DUALBOOT_BK_BASE must be 16 MiB aligned: Banjo-Kazooie reaches the cart through osRomBase | offset"
.endif
.if (DUALBOOT_BK_BASE + 0x01000000) > 0x04000000
.error "DUALBOOT_BK_BASE: the 16 MiB of Banjo-Kazooie must end inside the 64 MiB image"
.endif
; The Banjo-Kazooie payload has a 64 KiB slot of its own below BK. It may not start below 0x02200000,
; the cart end of the hack's own .area in build.asm (0x02000000 + 0x00200000), and has to end at or
; below BK. The slot size is the image builder's constant, repeated here only for that test: the
; launcher loads as many bytes as the payload's header says.
DUALBOOT_BK_PAYLOAD equ 0x02200000
DUALBOOT_BK_PAYLOAD_SLOT equ 0x00010000
.if DUALBOOT_BK_PAYLOAD < 0x02200000
.error "DUALBOOT_BK_PAYLOAD lies inside the hack's own .area (build.asm), which ends at cart 0x02200000"
.endif
.if (DUALBOOT_BK_PAYLOAD + DUALBOOT_BK_PAYLOAD_SLOT) > DUALBOOT_BK_BASE
.error "DUALBOOT_BK_PAYLOAD: the 64 KiB payload slot must end at or below DUALBOOT_BK_BASE"
.endif
; what dualboot_quiesce.asm reads its 8-byte PI poke from
QUIESCE_POKE_CART equ (0x10000000 + DUALBOOT_BK_BASE)
; 1 = write progress markers to 0x805FFF10 (stage) / 0x805FFF14 (spin counter) for memory probing
DUALBOOT_TRACE equ 1
; diagnostic: run a one-instruction RSP program (break) before launching, to mimic a 6105 IPL3 having used the RSP
DUALBOOT_RSPPOKE equ 0
; 1 = recreate the CIC-6103 cold-boot residue in RDRAM 0x000..0x3FF (see the block before the jump)
DUALBOOT_RESIDUE equ 1
; 1 = zero the rest of BK's 4 MiB, so BK starts on the RAM an emulator cold boot gives it, not Tooie's
DUALBOOT_RAMCLEAR equ 1
; 1 = load the Banjo-Kazooie payload when the image carries one
DUALBOOT_BKPAYLOAD equ 1
; 1 = acknowledge the PI, DP, SI and AI interrupts before the jump, as the IPL3 does
DUALBOOT_INTACK equ 1
; 1 = zero CP0 Cause, Count and Compare before the jump, as the IPL3 does at its entry
DUALBOOT_CP0RESET equ 1

.align 0x08
dualboot_launch_bk:
  ; interrupts off
  mfc0  t0, $12
  lui   t1, 0xFFFF
  ori   t1, t1, 0xFFFE
  and   t0, t0, t1
  mtc0  t0, $12
  nop
  nop
  ; private stack inside the payload window (never DMA'd over)
  lui   sp, 0x8060
  addiu sp, sp, -0x400
.if DUALBOOT_RSPPOKE
  lui   t1, 0xA400
  ori   t2, zero, 0x000D
  sw    t2, 0x1000(t1)          ; IMEM[0] = break
  sw    zero, 0x1004(t1)
  lui   t1, 0xA408
  sw    zero, 0x00(t1)          ; SP_PC = 0
  lui   t1, 0xA404
  ori   t2, zero, 0x0001
  sw    t2, 0x10(t1)            ; SP_STATUS: clear halt -> run
.endif
  jal   dualboot_quiesce
  nop
  lui   t0, 0xA460              ; PI base, used below
  ; Banjo-Kazooie's ucode_load (core1 ucode.c) only fetches its rspboot microcode from the ROM when
  ; DMEM[0] == 0xFFFFFFFF and IMEM[0] == 0x000017D7, the values the CIC-6103 IPL3 leaves behind on a
  ; cold boot. Without them the audio/gfx tasks run an all-zero boot ucode and the game parks on a
  ; black screen (LLE RSP; HLE RSPs don't execute rspboot, which is why mupen64plus never noticed).
  lui   t2, 0xA400
  addiu t3, zero, -1
  sw    t3, 0x0000(t2)          ; DMEM[0] = 0xFFFFFFFF
  ori   t3, zero, 0x17D7
  sw    t3, 0x1000(t2)          ; IMEM[0] = 0x000017D7
  ; write back and drop both caches
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 5
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
  lui   t1, 0x8000
  ori   t2, zero, 0x400
dualboot_flush:
  cache 0x00, 0(t1)             ; Index_Invalidate_I
  cache 0x01, 0(t1)             ; Index_Writeback_Invalidate_D
  addiu t1, t1, 0x10
  addiu t2, t2, -1
  bnez  t2, dualboot_flush
  nop
  ; Count this launch (handoff block + 0x08, src/dualboot.h). Through KSEG1, and only now: the C side
  ; wrote its words of the block through KSEG0, and the write-back above has just put that line in RAM.
  lui   t9, 0xA060
  lw    t8, -0xF8(t9)           ; 0xA05FFF08
  addiu t8, t8, 1
  sw    t8, -0xF8(t9)
  ; Withdraw what an earlier launch handed Banjo-Kazooie: the launch token (+0x20) and the address of
  ; the payload (+0x24). Both are written again further down, the address only if this launch loads a
  ; payload, so BK never acts on an older launch's. Here and not earlier for the counter's reason: the C
  ; side has just stored the countdown (+0x28) and the flags (+0x2C) through KSEG0, all four words lie
  ; in the cache line 0x805FFF20, and the write-back above would have put the old line over anything
  ; stored before it. Through KSEG1 like every store to the block from here on, each with a `lui` of
  ; its own so that none leans on a register an earlier step happened to leave. No emulator models the
  ; caches: a mistake in this shows only on a console.
  lui   t9, 0xA060
  sw    zero, -0xE0(t9)         ; 0xA05FFF20
  sw    zero, -0xDC(t9)         ; 0xA05FFF24
  ; DMA BK's boot segment: cart (0x10000000 + BASE + 0x1000) -> RDRAM 0x00000400, 1 MiB + 4 bytes, the
  ; length the 6103 IPL3 asks for (a400050c: lui t2,0x10; addiu t2,t2,3)
  ori   t1, zero, 0x0400
  sw    t1, 0x00(t0)            ; PI_DRAM_ADDR
  li    t2, 0x10000000 + DUALBOOT_BK_BASE + 0x1000
  sw    t2, 0x04(t0)            ; PI_CART_ADDR
  lui   t3, 0x0010
  addiu t3, t3, 3
  sw    t3, 0x0C(t0)            ; PI_WR_LEN: 0x100004 bytes
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 6
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
dualboot_pi_wait1:
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  lw    t8, -0xEC(t9)
  addiu t8, t8, 1
  sw    t8, -0xEC(t9)
.endif
  lw    t4, 0x10(t0)
  andi  t4, t4, 3
  bnez  t4, dualboot_pi_wait1
  nop
  sw    t2, 0x10(t0)            ; PI_STATUS write (bit 1 clear, so it acknowledges nothing; see DUALBOOT_INTACK)
  ; osRomBase (0x80000308) = 0xB0000000 | BASE. This is the whole relocation of Banjo-Kazooie: its
  ; libultra reaches the cart through osPiRawStartDma and osPiRawReadIo only (each twice, boot segment
  ; and core1), both read this word on every call and OR the offset in (retail core1 osPiRawReadIo,
  ; 0x8026AC7C: lui t0,0x8000; lw t0,0x308(t0), read out of a running BK on 2026-10-11), and nothing
  ; in BK writes it (hack/bk/README.md, "Cart offset without relocation"). Tooie's IPL3 left 0xB0000000
  ; here and puts that back when BK returns. Not part of the residue below and not under its toggle:
  ; without this word every cart read of BK lands in Tooie's part of the image. Through KSEG1: 0x308
  ; shares its cache line with osResetType, which the residue block stores through KSEG1, and the first
  ; flush has emptied the data cache, so no cached copy of the line is left to come back over either.
  lui   t9, 0xA000
  li    t8, 0xB0000000 | DUALBOOT_BK_BASE
  sw    t8, 0x0308(t9)          ; osRomBase
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 7
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
.if DUALBOOT_RESIDUE
  ; Banjo-Kazooie's anti-tamper compares state that only a CIC-6103 cold boot leaves behind. The 6103
  ; IPL3 copies its own bytes 0x4C0..0x768 to RDRAM 0x000 and finishes the boot from there; BK later
  ; reads six of those instruction words back through KSEG1 (0x1D0, 0x1D8, 0x200, 0x22C, 0x238, 0x284)
  ; and osCicId (0x310) through KSEG0. After Tooie's 6105 boot all seven differ and BK sabotages itself:
  ; near/far planes forced to 750/1250 (only a slice of the scene draws), every animation 3 s longer,
  ; Banjo at quarter scale, exit pads dead, and more. Sites and values: hack/DUALBOOT.md, "Cold-boot
  ; residue". A BK built from the fork has those checks compiled out and needs none of this; the
  ; unmodified retail BK of the control image does. Either carries its IPL3 untouched, so copy the
  ; block straight from the cart.
  ; Tooie's exception vectors go with it, as on a cold boot; interrupts stay off until BK installs its own.
  sw    zero, 0x00(t0)          ; PI_DRAM_ADDR = 0
  li    t2, 0x10000000 + DUALBOOT_BK_BASE + 0x04C0
  sw    t2, 0x04(t0)            ; PI_CART_ADDR: BK IPL3 0x4C0 (6103 IPL3 a4000458..a40004ac copies 0x4C0..0x768)
  ori   t3, zero, 0x02A8 - 1
  sw    t3, 0x0C(t0)            ; PI_WR_LEN: 0x2A8 bytes
dualboot_pi_wait2:
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  lw    t8, -0xEC(t9)
  addiu t8, t8, 1
  sw    t8, -0xEC(t9)
.endif
  lw    t4, 0x10(t0)
  andi  t4, t4, 3
  bnez  t4, dualboot_pi_wait2
  nop
  ; What the 6103 IPL3 never writes but the 6105 one did: 0x2A8..0x300 and 0x31C..0x338 (the tail of its
  ; own copy to 0x004..0x338, which ends inside osAppNMIBuffer). Zero those and the rest of the page, as
  ; an emulator has them on a cold boot (on a console they hold whatever RDRAM powered up with, so BK
  ; cannot depend on them).
  ; 0x3F0 is not ours to clear: the 6105 IPL3 saves the RDRAM size there on a cold boot only and copies it
  ; to osMemSize on every boot (a4000400..a4000414, a400076c..a4000774), so without it the next reset
  ; would give Tooie, and a relaunched BK, osMemSize = 0.
  lui   t1, 0xA000
  ori   t5, t1, 0x02A8
  ori   t6, t1, 0x0300
dualboot_residue_zero0:
  sw    zero, 0(t5)
  addiu t5, t5, 4
  bne   t5, t6, dualboot_residue_zero0
  nop
  ori   t5, t1, 0x031C
  ori   t6, t1, 0x03F0
dualboot_residue_zero1:
  sw    zero, 0(t5)
  addiu t5, t5, 4
  bne   t5, t6, dualboot_residue_zero1
  nop
  sw    zero, 0x03F4(t1)
  sw    zero, 0x03F8(t1)
  sw    zero, 0x03FC(t1)
  ; OS globals. osTvType, osRomType, osVersion and osMemSize are the console's and stay; osRomBase
  ; was pointed at BK above, outside this toggle.
  ori   t3, zero, 6103
  sw    t3, 0x0310(t1)          ; osCicId = 6103 (6103 IPL3 a40006d8: li t1,6103; sw t1,16(t0))
  sw    zero, 0x030C(t1)        ; osResetType = cold, whichever way Tooie was started
.endif
.if DUALBOOT_RAMCLEAR
  ; Everything from the end of the boot segment to the top of BK's 4 MiB still holds Tooie. BK shows RAM
  ; it never initialised (a few pixels of its jigsaw transition) and at boot scans 0x80000000..0x80400000
  ; in 0x80-byte steps for a Stop 'n' Swop payload (core1 sns.c:64; the last word it reads is the first
  ; of the AP payload, the anchor zeroed below, never the magic). The launcher, its stack and the handoff
  ; block live above 4 MiB.
  ; Tooie's framebuffer is in this range and VI is still scanning it out, so the picture goes black here.
  lui   t5, 0xA010
  ori   t5, t5, 0x0404
  lui   t6, 0xA040
dualboot_zero_ram:
  sw    zero, 0(t5)
  addiu t5, t5, 4
  bne   t5, t6, dualboot_zero_ram
  nop
  ; The IPL3's RDRAM sizing leaves 0xFFFFFFFF in the first two words of every 2 MiB module (6103 IPL3
  ; a4000918..a4000928). Module 0's pair is under the residue; module 1's is in BK's 4 MiB.
  lui   t1, 0xA020
  addiu t3, zero, -1
  sw    t3, 0(t1)
  sw    t3, 4(t1)
.endif
.if DUALBOOT_BKPAYLOAD
  ; The Banjo-Kazooie payload has its slot at DUALBOOT_BK_PAYLOAD and starts with 'BKP2', its entry,
  ; its load address, its length and the contract number (hack/bk/README.md in the superproject). It
  ; holds the way back to Tooie: the launcher loads it where its header says and passes that address
  ; on (handoff + 0x24), and BK calls the entry when it wants to go back. BK itself knows no address
  ; of the payload. An image without one, or with a 'BKPL' payload from before contract 2 (whose
  ; second word was a per-frame handler, not this entry), leaves + 0x24 zero: BK then cannot return.
  li    t1, 0xB0000000 + DUALBOOT_BK_PAYLOAD
  lw    t2, 0x00(t1)            ; PI is idle here, so the cart can be read directly
  li    t3, 0x424B5032          ; 'BKP2'
  bne   t2, t3, dualboot_payload_done
  nop
  lw    t7, 0x08(t1)            ; load address (KSEG0), kept in t7 for the handoff below
  lw    t5, 0x0C(t1)            ; length in bytes
  lui   t6, 0x1FFF
  ori   t6, t6, 0xFFFF
  and   t4, t7, t6
  sw    t4, 0x00(t0)            ; PI_DRAM_ADDR
  li    t2, 0x10000000 + DUALBOOT_BK_PAYLOAD
  sw    t2, 0x04(t0)            ; PI_CART_ADDR
  addiu t5, t5, -1
  sw    t5, 0x0C(t0)            ; PI_WR_LEN
dualboot_pi_wait3:
  lw    t4, 0x10(t0)
  andi  t4, t4, 3
  bnez  t4, dualboot_pi_wait3
  nop
  ; The payload is in RAM now, so its address may go into the handoff (+ 0x24, zeroed above for the
  ; case that this block loads nothing). Through KSEG1: BK reads the block uncached and zeroes this
  ; word once it has taken it.
  lui   t9, 0xA060
  sw    t7, -0xDC(t9)           ; 0xA05FFF24
dualboot_payload_done:
.endif
  ; The launch token (handoff + 0x20), on every launch, with or without a payload and outside its
  ; toggle. Banjo-Kazooie's bkbt_init takes it as the proof that this launcher has just started it, and
  ; only then believes + 0x24 and + 0x2C; it zeroes the token, so a later start that did not come
  ; through here (a BK ROM run by itself, with a payload of an older session still in RAM) finds none.
  ; Through KSEG1, for the reason given at the launch counter.
  lui   t9, 0xA060
  li    t8, DUALBOOT_TOKEN
  sw    t8, -0xE0(t9)           ; 0xA05FFF20
  ; Withdraw the AP anchor (AP_MEMORY_PTR in build.asm, physical 0x400000): Tooie is not running any
  ; more, and a client that finds the old pointer there would take BK's RAM for a live Tooie. Tooie's
  ; boot publishes it again (inject_hooks). Through KSEG1: C stored the pointer through KSEG0 and the
  ; first flush wrote that line back. This late, so that a memory probe keyed on the anchor
  ; (tools/pj64_peek.py in the superproject) can still watch the trace words while the launcher waits.
  lui   t9, 0xA040
  sw    zero, 0x0000(t9)        ; 0xA0400000
.if DUALBOOT_INTACK
  ; 6103 IPL3 a40006a8..a40006cc. No PI DMA follows, so the PI interrupt stays acknowledged. VI is not
  ; acknowledged: it is still running in Tooie's mode and its interrupt is pending at the jump, until BK's
  ; libultra reprograms it; BK drops the stale one.
  lui   t1, 0xA480
  sw    zero, 0x18(t1)          ; SI_STATUS: acknowledge the SI interrupt
  lui   t1, 0xA450
  sw    zero, 0x0C(t1)          ; AI_STATUS: acknowledge the AI interrupt
  lui   t1, 0xA430
  ori   t3, zero, 0x0800
  sw    t3, 0x00(t1)            ; MI_MODE: clear the DP interrupt
  ori   t3, zero, 0x0002
  sw    t3, 0x10(t0)            ; PI_STATUS: bit 1 acknowledges the PI interrupt
.endif
  ; Touch every 4 KiB page of the freshly loaded code with a CPU store. Emulator recompilers that only
  ; watch CPU writes then drop their stale blocks.
  lui   t1, 0x8000
  ori   t1, t1, 0x0400
  ori   t2, zero, 0x100         ; 1 MiB / 4 KiB
dualboot_touch:
  lw    t3, 0(t1)
  sw    t3, 0(t1)
  addiu t1, t1, 0x1000
  addiu t2, t2, -1
  bnez  t2, dualboot_touch
  nop
  ; drop caches again so no stale line covers the fresh code
  lui   t1, 0x8000
  ori   t2, zero, 0x400
dualboot_inval:
  cache 0x00, 0(t1)
  cache 0x01, 0(t1)
  addiu t1, t1, 0x10
  addiu t2, t2, -1
  bnez  t2, dualboot_inval
  nop
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 8
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
.if DUALBOOT_CP0RESET
  ; 6103 IPL3 a4000040..a4000048. Compare = 0 also drops the timer interrupt Tooie's last Compare value
  ; raised while the launcher ran; Count = 0 makes BK's osContInit keep its power-up wait.
  mtc0  zero, $13               ; Cause
  mtc0  zero, $9                ; Count
  mtc0  zero, $11               ; Compare
.endif
  lui   t4, 0x8000
  ori   t4, t4, 0x0400
  jr    t4                      ; BK entry
  nop

.include "dualboot_quiesce.asm"

; Read by tools/make_dualboot_image.py and the check tools in the superproject, which look for each tag
; in the Tooie part of the image and expect it exactly once: the launcher and the image must agree on
; where Banjo-Kazooie and its payload are, and the launcher, the payload and BK on the contract number.
; The contract tag also names the handoff block (DUALBOOT_HANDOFF in src/dualboot.h, addressed above as
; 0xA0600000 - 0x100 + offset). The hack's C has two tagged records of its own, found the same way:
; BKBTAUTO and BKBTARMD (src/dualboot.c).
; The word of the base tag is also where the hack's C takes DUALBOOT_BK_BASE from, through the label on
; it (src/dualboot.c reads it before every launch, to look whether a Banjo-Kazooie is there at all): the
; equ above stays the only definition. C loads it as a word, hence the alignment.
.align 4
dualboot_bk_base_tag:
.ascii "BKBTBASE"
dualboot_bk_base_word:
.word DUALBOOT_BK_BASE
dualboot_bk_payload_tag:
.ascii "BKBTPAYL"
.word DUALBOOT_BK_PAYLOAD
dualboot_contract_tag:
.ascii "BKBTCONT"
.word DUALBOOT_CONTRACT
.word 0x805FFF00
