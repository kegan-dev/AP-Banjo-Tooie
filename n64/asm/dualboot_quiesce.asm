; Shared quiesce for the dual-boot launchers (BKBT). Included by dualboot.asm (Tooie -> Banjo-Kazooie)
; and by the Banjo-Kazooie payload in the superproject (hack/bk/asm/payload.asm, Banjo-Kazooie -> Tooie).
;
; Call with interrupts off:  jal dualboot_quiesce  (leaf, no stack; clobbers t0-t9).
; Waits for the RSP, RDP, AI and PI to go idle by themselves, masks the MI interrupt sources, stops
; audio, and leaves the RSP halted with SP_PC = 0 after letting it halt once through the emulator's own
; path. Why each step is there: hack/DUALBOOT.md, "The switch" and "Project64 notes".
;
; The includer defines, before the .include:
;   DUALBOOT_TRACE     1 = write progress markers to 0xA05FFF10 (stage) / 0xA05FFF14 (spin counter)
;   QUIESCE_POKE_CART  PI cart-bus address of 8 readable bytes for the poke (0x10000000 + cart offset)
;
; armips matches `equ` names without regard to case: no label may be spelled like one of the toggles.

; 0 = only wait for idle; 1 = also mask MI interrupts and stop audio; 2 = also blank video (v1 behaviour)
QUIESCE_LEVEL equ 1
; 1 = after refreshing IMEM/DMEM by DMA, also run one `break` on the RSP and wait for it
DUALBOOT_RSPRUN equ 1
; 1 = write DPC_STATUS to clear xbus/freeze/flush
DUALBOOT_DPCRESET equ 1
; 1 = refill IMEM and DMEM by SP DMA (older variant; its scratch page 0x5FF000..0x5FFFFF covers the
; handoff block and the trace words)
DUALBOOT_SPDMA equ 0
; 1 = the SP DMA refresh also covers DMEM (0 = IMEM only)
DUALBOOT_SPDMA_DMEM equ 1

.align 0x08
dualboot_quiesce:
  ; Project64 runs a started RSP task only when it next processes an interrupt-type event, so a CPU
  ; spinning on SP_STATUS with interrupts off would wait forever. A harmless 8-byte PI DMA into scratch
  ; RAM gives it that event. The SP, DP and AI waits below are bounded (the SP one forces a halt on
  ; timeout); the PI waits are not.
  lui   t0, 0xA460
  lui   t1, 0x005F
  ori   t1, t1, 0xFE00          ; scratch: phys 0x5FFE00 (inside the payload window)
  sw    t1, 0x00(t0)            ; PI_DRAM_ADDR
  li    t2, QUIESCE_POKE_CART
  sw    t2, 0x04(t0)            ; PI_CART_ADDR
  ori   t3, zero, 7
  sw    t3, 0x0C(t0)            ; PI_WR_LEN = 8 - 1
dualboot_pi_poke_wait:
  lw    t4, 0x10(t0)
  andi  t4, t4, 3
  bnez  t4, dualboot_pi_poke_wait
  nop
  sw    t2, 0x10(t0)            ; PI_STATUS write (bit 1 clear, so it acknowledges nothing; see DUALBOOT_INTACK)
  ; wait for the RSP to halt on its own (task done), bounded
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 1
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
  lui   t1, 0xA404
  lui   t5, 0x0020              ; ~2M iterations timeout
dualboot_sp_wait:
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  lw    t8, -0xEC(t9)
  addiu t8, t8, 1
  sw    t8, -0xEC(t9)
.endif
  lw    t4, 0x10(t1)            ; SP_STATUS
  andi  t4, t4, 0x0001          ; HALT
  bnez  t4, dualboot_sp_done
  addiu t5, t5, -1
  bnez  t5, dualboot_sp_wait
  nop
  ori   t4, zero, 0x0002
  sw    t4, 0x10(t1)            ; timeout: force SP_STATUS halt
dualboot_sp_done:
  ; wait for the RDP to drain
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 2
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
  lui   t1, 0xA410
  lui   t5, 0x0020
dualboot_dp_wait:
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  lw    t8, -0xEC(t9)
  addiu t8, t8, 1
  sw    t8, -0xEC(t9)
.endif
  lw    t4, 0x0C(t1)            ; DPC_STATUS
  andi  t4, t4, 0x0160          ; CMD busy | pipe busy | DMA busy
  beqz  t4, dualboot_dp_done
  addiu t5, t5, -1
  bnez  t5, dualboot_dp_wait
  nop
dualboot_dp_done:
  ; wait until the AI FIFO is no longer full (not until it is idle: a buffer may still be playing when
  ; AI_CONTROL is cleared below; what a console does with the rest of it is untested)
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 3
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
  lui   t1, 0xA450
  lui   t5, 0x0020
dualboot_ai_wait:
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  lw    t8, -0xEC(t9)
  addiu t8, t8, 1
  sw    t8, -0xEC(t9)
.endif
  lw    t4, 0x0C(t1)            ; AI_STATUS
  bgez  t4, dualboot_ai_done    ; bit 31 = FIFO full
  addiu t5, t5, -1
  bnez  t5, dualboot_ai_wait
  nop
dualboot_ai_done:
  ; wait for any PI transfer the game started
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  ori   t8, zero, 4
  sw    t8, -0xF0(t9)           ; stage -> 0xA05FFF10 (uncached)
  sw    zero, -0xEC(t9)         ; counter -> 0xA05FFF14
.endif
  lui   t0, 0xA460
dualboot_pi_wait0:
.if DUALBOOT_TRACE
  lui   t9, 0xA060
  lw    t8, -0xEC(t9)
  addiu t8, t8, 1
  sw    t8, -0xEC(t9)
.endif
  lw    t4, 0x10(t0)
  andi  t4, t4, 3
  bnez  t4, dualboot_pi_wait0
  nop
.if QUIESCE_LEVEL >= 1
  lui   t1, 0xA430
  ori   t2, zero, 0x555
  sw    t2, 0x0C(t1)            ; MI_INTR_MASK: clear SP/SI/AI/VI/PI/DP masks
  lui   t1, 0xA450
  sw    zero, 0x08(t1)          ; AI_CONTROL = 0
.endif
.if QUIESCE_LEVEL >= 2
  lui   t1, 0xA440
  sw    zero, 0x00(t1)          ; VI_CONTROL = 0
.endif
  ; Give BK a cold RSP/RDP. Tooie's CIC-6105 IPL3 ran an RSP program at boot, and Project64 3.0.1's RSP
  ; recompiler crashed on BK's first task unless the RSP first halted through the plugin's own path.
  ; Default (DUALBOOT_SPDMA=0): fill IMEM with `break` by CPU stores and run it once. DUALBOOT_SPDMA=1 is
  ; the older variant that first refreshes IMEM (and DMEM) through SP DMA.
  lui   t1, 0xA404
  lui   t2, 0x00AA
  ori   t2, t2, 0xAAAE          ; SP_STATUS: set HALT, clear BROKE/INTR/SSTEP/INTR_BREAK/SIG0..SIG7
  sw    t2, 0x10(t1)
  sw    zero, 0x1C(t1)          ; SP_SEMAPHORE release
.if DUALBOOT_DPCRESET
  lui   t1, 0xA410
  ori   t2, zero, 0x03D5
  sw    t2, 0x0C(t1)            ; DPC_STATUS: clear xbus/freeze/flush and counters
.endif
.if DUALBOOT_SPDMA
  ; 4 KiB of `break` at uncached 0xA05FF000 (phys 0x5FF000, payload window)
  lui   t1, 0xA060
  addiu t1, t1, -0x1000
  ori   t2, zero, 0x400
  ori   t3, zero, 0x000D
dualboot_fill_break:
  sw    t3, 0(t1)
  addiu t1, t1, 4
  addiu t2, t2, -1
  bnez  t2, dualboot_fill_break
  nop
  ; SP DMA RDRAM 0x5FF000 -> IMEM (0x1000) and -> DMEM (0x0000), 4 KiB each
  lui   t1, 0xA404
  lui   t4, 0x005F
  ori   t4, t4, 0xF000
  ori   t5, zero, 0x1000
  sw    t5, 0x00(t1)            ; SP_MEM_ADDR = IMEM
  sw    t4, 0x04(t1)            ; SP_DRAM_ADDR
  ori   t5, zero, 0x0FFF
  sw    t5, 0x08(t1)            ; SP_RD_LEN = 4 KiB - 1 (RDRAM -> SP mem)
dualboot_spdma_wait0:
  lw    t6, 0x10(t1)
  andi  t6, t6, 0x000C          ; DMA busy | DMA full
  bnez  t6, dualboot_spdma_wait0
  nop
  ; Project64 completes CPU-started SP DMAs only at its next event: poke the PI and re-check busy
  lui   t0, 0xA460
  lui   t6, 0x005F
  ori   t6, t6, 0xFE00
  sw    t6, 0x00(t0)
  li    t6, QUIESCE_POKE_CART
  sw    t6, 0x04(t0)
  ori   t6, zero, 7
  sw    t6, 0x0C(t0)
dualboot_flush0_pi:
  lw    t6, 0x10(t0)
  andi  t6, t6, 3
  bnez  t6, dualboot_flush0_pi
  nop
  sw    t6, 0x10(t0)
  lui   t5, 0x0020
dualboot_flush0_sp:
  lw    t6, 0x10(t1)
  andi  t6, t6, 0x000C
  beqz  t6, dualboot_flush0_done
  addiu t5, t5, -1
  bnez  t5, dualboot_flush0_sp
  nop
dualboot_flush0_done:
.if DUALBOOT_SPDMA_DMEM
  sw    zero, 0x00(t1)          ; SP_MEM_ADDR = DMEM
  sw    t4, 0x04(t1)
  ori   t5, zero, 0x0FFF
  sw    t5, 0x08(t1)
dualboot_spdma_wait1:
  lw    t6, 0x10(t1)
  andi  t6, t6, 0x000C
  bnez  t6, dualboot_spdma_wait1
  nop
  ; Project64 completes CPU-started SP DMAs only at its next event: poke the PI and re-check busy
  lui   t0, 0xA460
  lui   t6, 0x005F
  ori   t6, t6, 0xFE00
  sw    t6, 0x00(t0)
  li    t6, QUIESCE_POKE_CART
  sw    t6, 0x04(t0)
  ori   t6, zero, 7
  sw    t6, 0x0C(t0)
dualboot_flush1_pi:
  lw    t6, 0x10(t0)
  andi  t6, t6, 3
  bnez  t6, dualboot_flush1_pi
  nop
  sw    t6, 0x10(t0)
  lui   t5, 0x0020
dualboot_flush1_sp:
  lw    t6, 0x10(t1)
  andi  t6, t6, 0x000C
  beqz  t6, dualboot_flush1_done
  addiu t5, t5, -1
  bnez  t5, dualboot_flush1_sp
  nop
dualboot_flush1_done:
.endif
.endif
.if DUALBOOT_RSPRUN
.if DUALBOOT_SPDMA == 0
  ; fill ALL of IMEM with `break` by CPU stores, so wherever the recompiler's stale control state
  ; resumes, it halts cleanly through the plugin's own path (no SP DMA involved)
  lui   t1, 0xA400
  ori   t1, t1, 0x1000
  ori   t6, zero, 0x000D
  ori   t2, zero, 0x400
dualboot_imem_break:
  sw    t6, 0(t1)
  addiu t1, t1, 4
  addiu t2, t2, -1
  bnez  t2, dualboot_imem_break
  nop
  lui   t1, 0xA404
.endif
  ; run the RSP on `break` at PC 0 and let it halt itself
  lui   t2, 0xA408
  sw    zero, 0x00(t2)          ; SP_PC = 0
  ori   t5, zero, 0x0105
  sw    t5, 0x10(t1)            ; SP_STATUS: clear HALT, clear BROKE, SET_INTR_BREAK -> run; the break then ends the task the way the emulator expects
  ; Project64 only runs it when it processes an event: poke the PI again (8-byte DMA)
  lui   t0, 0xA460
  lui   t4, 0x005F
  ori   t4, t4, 0xFE00
  sw    t4, 0x00(t0)
  li    t4, QUIESCE_POKE_CART
  sw    t4, 0x04(t0)
  ori   t4, zero, 7
  sw    t4, 0x0C(t0)
dualboot_pi_poke2_wait:
  lw    t6, 0x10(t0)
  andi  t6, t6, 3
  bnez  t6, dualboot_pi_poke2_wait
  nop
  sw    t4, 0x10(t0)            ; PI_STATUS = 7: resets the (idle) PI controller and acknowledges its interrupt
  lui   t5, 0x0020
dualboot_sp_wait2:
  lw    t6, 0x10(t1)
  andi  t6, t6, 0x0001          ; HALT
  bnez  t6, dualboot_sp_done2
  addiu t5, t5, -1
  bnez  t5, dualboot_sp_wait2
  nop
dualboot_sp_done2:
.endif
  lui   t1, 0xA404
  lui   t2, 0x00AA
  ori   t2, t2, 0xAAAE
  sw    t2, 0x10(t1)            ; SP_STATUS: halt + clear everything again
  sw    zero, 0x1C(t1)
  lui   t2, 0xA408
  sw    zero, 0x00(t2)          ; SP_PC = 0
  jr    ra
  nop
