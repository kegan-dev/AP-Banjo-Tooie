; Dual-boot launcher (BKBT). Warm-starts the relocated Banjo-Kazooie that lives at cart offset
; DUALBOOT_BK_BASE in the combined image. Runs from the AP payload (0x804xxxxx), which the DMA below
; does not touch. DUALBOOT_BK_BASE must equal --base of tools/bk_relocate.py and of
; tools/make_dualboot_image.py; the image builder checks it through the BKBTBASE tag at the end of this file.
;
; Called from C as  void dualboot_launch_bk(void);  Never returns.
;
; Sequence: interrupts off (so no thread can start new work), wait for the RSP, RDP, AI and PI to go
; idle by themselves (emulators such as Project64 run RSP tasks on a timer *after* the game starts
; them, so forcibly halting the RSP leaves a task half-done), optionally mask/silence, DMA BK's boot
; segment over the first MiB like IPL3 would, recreate what BK's CIC-6103 IPL3 leaves in low RDRAM
; (BK's anti-tamper reads it back), zero the rest of BK's 4 MiB, load the Banjo-Kazooie payload (the
; superproject's hack/bk) into Expansion Pak RAM, drop the caches, jump to BK's entry.
; The way back is not here: the BK payload launches Tooie (hack/bk/asm/return.asm).
;
; armips matches `equ` names without regard to case: no label may be spelled like one of the toggles.

DUALBOOT_BK_BASE equ 0x02200000
; The Banjo-Kazooie payload follows the 16 MiB of BK in the image (tools/make_dualboot_image.py checks
; this through the BKBTPAYL tag at the end of this file).
DUALBOOT_BK_PAYLOAD equ (DUALBOOT_BK_BASE + 0x01000000)
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
  ; residue". The relocated BK carries its IPL3 untouched, so copy the block straight from the cart.
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
  ; OS globals. osTvType, osRomType, osRomBase, osVersion and osMemSize are the console's and stay.
  ori   t3, zero, 6103
  sw    t3, 0x0310(t1)          ; osCicId = 6103 (6103 IPL3 a40006d8: li t1,6103; sw t1,16(t0))
  sw    zero, 0x030C(t1)        ; osResetType = cold, whichever way Tooie was started
.endif
.if DUALBOOT_RAMCLEAR
  ; Everything from the end of the boot segment to the top of BK's 4 MiB still holds Tooie. BK shows RAM
  ; it never initialised (a few pixels of its jigsaw transition) and at boot scans 0x80000000..0x80400000
  ; in 0x80-byte steps for a Stop 'n' Swop payload (core1 sns.c:64; the last word it reads is the first
  ; of the AP payload, never the magic). The launcher, its stack and the handoff block live above 4 MiB.
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
  ; The Banjo-Kazooie payload sits right behind BK in the image and starts with 'BKPL', its per-frame
  ; handler, its load address and its length (hack/bk/README.md in the superproject). BK's main loop
  ; calls the handler once the payload is in RAM (tools/bk_relocate.py puts that call in). It holds the
  ; way back to Tooie. An image without a payload leaves BK as it was: no hook, no return.
  li    t1, 0xB0000000 + DUALBOOT_BK_PAYLOAD
  lw    t2, 0x00(t1)            ; PI is idle here, so the cart can be read directly
  li    t3, 0x424B504C          ; 'BKPL'
  bne   t2, t3, dualboot_payload_done
  nop
  lw    t4, 0x08(t1)            ; load address (KSEG0)
  lw    t5, 0x0C(t1)            ; length in bytes
  lui   t6, 0x1FFF
  ori   t6, t6, 0xFFFF
  and   t4, t4, t6
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
dualboot_payload_done:
.endif
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

; Read by tools/make_dualboot_image.py: the launcher, the relocated BK and the image must agree on BASE,
; and the launcher and the image on where the Banjo-Kazooie payload is.
dualboot_bk_base_tag:
.ascii "BKBTBASE"
.word DUALBOOT_BK_BASE
dualboot_bk_payload_tag:
.ascii "BKBTPAYL"
.word DUALBOOT_BK_PAYLOAD
