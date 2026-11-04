; BKBT: every trampoline here that calls C keeps the first 0x10 bytes of its frame for that function, the o32
; argument home area, and saves its registers above it. GCC may store a0..a3 there (o32 gives the four slots to
; the callee): main_collected_nest stores a0 at its frame size + 0, and with the saves at 0 (SP), as upstream has
; them, that was the upper half of the saved RA, which the trampoline reloads into AT and jumps through. The jump
; then had the nest's address in AT's upper 32 bits: Project64 jumps by the lower 32, ares raised an address error
; (the VR4300 in 32-bit mode wants the upper half to be the sign extension of the lower), and Tooie froze at every
; nest (found 2026-11-03 from the build's objects; upstream's 4.13.1, built o64, keeps a0 in s1 and has no store
; there). A store into the slot of a1 or a3 would have broken the lower half, on every emulator (the cursor hook's
; added call of 2026-10-31). hack/BT-BUILD.md in the superproject, local change 7.

.align 0x08
inject_draw_objects_displaced:
ADDIU SP, SP, -0x38
J 0x800EB524
SW S0, 0x0018 (SP)

.align 0x08
inject_draw_hud_displaced:
ADDIU SP, SP, -0x38
J 0x800FA510
LUI T6, 0x8013

.align 0x08
inject_spawn_prop_displaced:
ADDIU SP, SP, -0x28
J 0x80108C98
SW RA, 0x001C (SP)

.align 0x08
bt_fn_zoombox_new:
ADDIU V0, R0, 0x0000
J 0x80087B08
SW V0, 0x0010 (SP)

.align 0x08
bt_fn_zoombox_dialog_options:
LUI AT, 0x3ECC
ADDIU AT, AT, 0xCCCD
SW AT, 0x0010 (SP)
ADDIU AT, R0, 0x0001
SW AT, 0x0014 (SP)
J 0x80087B80
SW R0, 0x0018 (SP)

; BKBT: osViBlack's first two instructions (bt/misc_functions.h, BT_OS_VI_BLACK), replaced by door.c's jump
.align 0x08
door_vi_black_displaced:
ADDIU SP, SP, -0x18
J 0x8002FBB8
SW RA, 0x0014 (SP)

.align 0x08
inject_load_scene_displaced:
ADDIU SP, SP, -0x20
J 0x800A72AC
LUI T6, 0x8012

.align 0x08
inject_get_data_displaced:
ADDIU SP, SP, -0x38
J 0x800D6754
SW RA, 0x0014 (SP)

.align 0x08
inject_load_data_displaced:
LUI V0, 0x8013
J 0x800D5B3C
ADDIU V0, V0, 0xB7E6

.align 0x08
ap_increase_health_displaced:
ADDIU SP, SP, -0x20
J 0x800A17B0
SW RA, 0x0014 (SP)

.align 0x08
ap_get_health_displaced:
ADDIU SP, SP, -0x18
J 0x800A1720
SW RA, 0x0014 (SP)

.align 0x08
ap_ground_info_displaced:
ADDIU SP, SP, -0x18
J 0x80096630
SW RA, 0x0014 (SP)

.align 0x08
main_init_nest_displaced:
LW A0, 0x0018 (SP)
J main_init_nest
ADDIU SP, SP, 0x18

.align 0x08
main_train_change_station_displaced:
LH A0, 0x0000 (S0)
LW RA, 0x001C (SP)
LW S0, 0x0014 (SP)
LW S1, 0x0018 (SP)
J main_train_change_station
ADDIU SP, SP, 0x20

.align 0x08
main_bt_paused_displaced:
ADDIU SP, SP, -0x18
SW RA, 0x0010 (SP)
JAL main_bt_paused
NOP
LW RA, 0x0010 (SP)
ADDIU SP, SP, 0x18
LW A2, 0x001C (SP)
ADDIU A1, R0, 0x0007
JR RA
NOP

.align 0x08
main_draw_warp_pad_displaced:
ADDIU SP, SP, -0x30
SD RA, 0x0010 (SP)
SD AT, 0x0018 (SP)
SD A0, 0x0020 (SP)
SD A1, 0x0028 (SP)
JAL main_draw_warp_pad
NOP
LD AT, 0x0010 (SP)
LD RA, 0x0018 (SP)
LD A0, 0x0020 (SP)
LD A1, 0x0028 (SP)
ADDIU SP, SP, 0x30
; displaced code
ADDIU SP, SP, -0x18
SW RA, 0x0014 (SP)
JR AT
SW A0, 0x0018 (SP)

.align 0x08
main_warp_pad_update_displaced:
ADDIU SP, SP, -0x30
SD RA, 0x0010 (SP)
SD AT, 0x0018 (SP)
SD A0, 0x0020 (SP)
SD A1, 0x0028 (SP)
JAL main_warp_pad_update
NOP
LD AT, 0x0010 (SP)
LD RA, 0x0018 (SP)
LD A0, 0x0020 (SP)
LD A1, 0x0028 (SP)
ADDIU SP, SP, 0x30
BNEZ V0, .+16
NOP
JR RA
NOP
; displaced code
ADDIU SP, SP, -0x30
SW RA, 0x0024 (SP)
JR AT
SW S0, 0x0020 (SP)

.align 0x08
main_warp_silo_check_displaced:
ADDIU SP, SP, -0x28
SD RA, 0x0010 (SP)
SD AT, 0x0018 (SP)
SD A0, 0x0020 (SP)
JAL main_warp_silo_check
NOP
LD AT, 0x0010 (SP)
LD RA, 0x0018 (SP)
LD A0, 0x0020 (SP)
ADDIU SP, SP, 0x28
BNEZ V0, .+16
NOP
JR RA
ADDIU V0, R0, 0x0000
; displaced code
ADDIU SP, SP, -0x28
SW S3, 0x0020 (SP)
JR AT
SW S2, 0x001C (SP)

.align 0x08
main_warp_silo_failed_displaced:
ADDIU SP, SP, -0x38
SD RA, 0x0010 (SP)
SD AT, 0x0018 (SP)
SD A0, 0x0020 (SP)
SD A1, 0x0028 (SP)
SD A2, 0x0030 (SP)
JAL main_warp_silo_failed
NOP
LD AT, 0x0010 (SP)
LD RA, 0x0018 (SP)
LD A0, 0x0020 (SP)
LD A1, 0x0028 (SP)
LD A2, 0x0030 (SP)
ADDIU SP, SP, 0x38
BNEZ V0, .+16
NOP
JR RA
NOP
; displaced code
ADDIU SP, SP, -0x28
SW RA, 0x0024 (SP)
JR AT
SW S0, 0x0020 (SP)

.align 0x08
main_bt_pause_state_change_displaced:
ADDIU SP, SP, -0x40
SD RA, 0x0010 (SP)
SD AT, 0x0018 (SP)
SD A0, 0x0020 (SP)
SD A1, 0x0028 (SP)
SD A2, 0x0030 (SP)
SD A3, 0x0038 (SP)
JAL main_bt_pause_state_change
NOP
LD AT, 0x0010 (SP)
LD RA, 0x0018 (SP)
LD A0, 0x0020 (SP)
LD A1, 0x0028 (SP)
LD A2, 0x0030 (SP)
LD A3, 0x0038 (SP)
ADDIU SP, SP, 0x40
BNEZ V0, .+16
NOP
JR RA
NOP
; displaced code
ADDIU SP, SP, -0x30
SW S0, 0x0018 (SP)
JR AT
SLTIU AT, A3, 0x0006

.align 0x08
main_collected_nest_displaced:
ADDIU SP, SP, -0x40
SD RA, 0x0010 (SP)
SD AT, 0x0018 (SP)
SD A0, 0x0020 (SP)
SD A1, 0x0028 (SP)
SD A2, 0x0030 (SP)
SD A3, 0x0038 (SP)
JAL main_collected_nest
NOP
LD AT, 0x0010 (SP)
LD RA, 0x0018 (SP)
LD A0, 0x0020 (SP)
LD A1, 0x0028 (SP)
LD A2, 0x0030 (SP)
LD A3, 0x0038 (SP)
ADDIU SP, SP, 0x40
BNEZ V0, .+16
NOP
JR RA
NOP
; displaced code
ADDIU SP, SP, -0x20
SW RA, 0x0014 (SP)
JR AT
SW A1, 0x0024 (SP)

.align 0x08
main_bt_pause_load_menu_displaced:
ADDIU SP, SP, -0x40
SD RA, 0x0010 (SP)
SD AT, 0x0018 (SP)
SD A0, 0x0020 (SP)
SD A1, 0x0028 (SP)
SD A2, 0x0030 (SP)
SD A3, 0x0038 (SP)
JAL main_bt_pause_load_menu
NOP
LD AT, 0x0010 (SP)
LD RA, 0x0018 (SP)
LD A0, 0x0020 (SP)
LD A1, 0x0028 (SP)
LD A2, 0x0030 (SP)
LD A3, 0x0038 (SP)
ADDIU SP, SP, 0x40
BNEZ V0, .+16
NOP
JR RA
NOP
; displaced code
ADDIU SP, SP, -0x20
SW RA, 0x001C (SP)
JR AT
SW S0, 0x0018 (SP)

.align 0x08
main_bt_file_select_cursor_displaced:
ADDIU SP, SP, -0x28
SW RA, 0x0010 (SP)
SW AT, 0x0014 (SP)
SW A0, 0x0018 (SP)
SW A1, 0x001C (SP)
SW A2, 0x0020 (SP)
SW A3, 0x0024 (SP)
JAL main_bt_file_select_cursor
NOP
LW AT, 0x0010 (SP)
LW RA, 0x0014 (SP)
LW A0, 0x0018 (SP)
LW A1, 0x001C (SP)
LW A2, 0x0020 (SP)
LW A3, 0x0024 (SP)
ADDIU SP, SP, 0x28
BNEZ V0, .+16
NOP
JR RA
NOP
; displaced code
ADDIU SP, SP, -0x28
SW S0, 0x0018 (SP)
JR AT
ADDIU AT, R0, 0x0007

.align 0x08
main_bt_file_select_bottom_text_displaced:
ADDIU SP, SP, -0x28
SW RA, 0x0010 (SP)
SW AT, 0x0014 (SP)
SW A0, 0x0018 (SP)
SW A1, 0x001C (SP)
SW A2, 0x0020 (SP)
SW A3, 0x0024 (SP)
JAL main_bt_file_select_bottom_text
NOP
LW AT, 0x0010 (SP)
LW RA, 0x0014 (SP)
LW A0, 0x0018 (SP)
LW A1, 0x001C (SP)
LW A2, 0x0020 (SP)
LW A3, 0x0024 (SP)
ADDIU SP, SP, 0x28
BNEZ V0, .+16
NOP
JR RA
NOP
; displaced code
ADDIU SP, SP, -0x30
SW S2, 0x001C (SP)
JR AT
OR S2, A0, R0

.align 0x08
exits_can_pass_displaced:
ADDIU A0, SP, 0x0036
ADDIU A1, SP, 0x003A
ADDIU A2, R0, 0x0000
ADDIU SP, SP, -0x18
SD RA, 0x0010 (SP)
JAL exits_can_pass
NOP
LD RA, 0x0010 (SP)
ADDIU SP, SP, 0x18
BNEZ V0, .+16
ADDU RA, RA, V0
LW T9, 0x003C (SP) ; vanilla
ADDIU V0, R0, 0x000B
JR RA
NOP
