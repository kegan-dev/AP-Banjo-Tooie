#ifndef BT_FN_INJECTED_H
#define BT_FN_INJECTED_H

typedef u32 (*bt_fnt_init)(u32);
#define bt_fn_init ((bt_fnt_init)0x80012214)

typedef u32 (*bt_fnt_loop)(void);
#define bt_fn_loop ((bt_fnt_loop)0x800123F4)

typedef u32 (*bt_fnt_load_save)(u32);
#define bt_fn_load_save ((bt_fnt_load_save)0x80087A40)

// BKBT: libultra's osViBlack(u8 active). It sets or clears VI_STATE_BLACK (0x20) in *__osViNext, the pointer
// at 0x80041544, and the VI manager's swap (0x800340B0) then shows nothing. Tooie calls it from three places
// in its code (0x8001504C, 0x800152CC, 0x800155A4) and two in an overlay. Found in RAM on mupen64plus at the
// title screen, 2026-11-02 (the superproject's hack/DUALBOOT.md, "The hub door"). door.c replaces its entry,
// so call it through door_vi_black_displaced (asm/displaced.asm).
#define BT_OS_VI_BLACK 0x8002FBB0

// BKBT: the game's session, the struct at 0x8012B3F0 (bt_save_slot is its +0x01), as a file select starts a
// game. Found on mupen64plus on 2026-11-02 by a trace of the struct and of the load request (bt/maps.h,
// bt_fn_load_scene_with_save) while the file select started a new file and a saved one: both times the file's
// slot was set as the cursor came to it, then, at the A, the list of actions was made, the two actions queued,
// and the load with save asked for (the superproject's hack/DUALBOOT.md, "The hub door").
//   0x800D37C0 (s8 slot): +0x00 (the file the next load reads; 0xFF once it is read) and +0x01 = slot. The file
//     select calls it (0x801B1580) for the file under the cursor.
//   0x800D39A8 (): +0x0C = a new list of 20-byte actions (0x800B2DC0(20, 1)), which the load runs once the
//     file is read and frees (0x800D3C70).
//   0x800D39DC (s32 mode): queues +0x02 = mode (0x800D39D0); a file select's start queues 0.
//   0x800D3B24 (): queues the game's random seed (0x8012C7F0, 0x800DC324) = the CPU's count (0x800343B0).
typedef void (*bt_fnt_session_set_slot)(s32 slot);
#define bt_fn_session_set_slot ((bt_fnt_session_set_slot)0x800D37C0)
typedef void (*bt_fnt_session_new_actions)(void);
#define bt_fn_session_new_actions ((bt_fnt_session_new_actions)0x800D39A8)
typedef void (*bt_fnt_session_queue_mode)(s32 mode);
#define bt_fn_session_queue_mode ((bt_fnt_session_queue_mode)0x800D39DC)
typedef void (*bt_fnt_session_queue_random_seed)(void);
#define bt_fn_session_queue_random_seed ((bt_fnt_session_queue_random_seed)0x800D3B24)

typedef u32 (*bt_fnt_object_init)(bt_object_t*, u32, u32, u32);
#define bt_fn_object_init ((bt_fnt_object_init)0x80082088)

typedef u32 (*bt_fnt_set_bit)(bt_save_flags_t*, u16, u8);
#define bt_fn_set_bit ((bt_fnt_set_bit)0x800D9F18)

typedef u32 (*bt_fnt_set_save_bit)(u16);
#define bt_fn_set_save_bit ((bt_fnt_set_save_bit)0x800DA544)

typedef u32 (*bt_fnt_get_bit)(bt_save_flags_t*, u16);
#define bt_fn_get_bit ((bt_fnt_get_bit)0x800D9E60)

typedef u32 (*bt_fnt_get_save_bit)(u16);
#define bt_fn_get_save_bit ((bt_fnt_get_save_bit)0x800DA298)

typedef bt_obj_instance_t* (*bt_fnt_spawn_prop)(u16 id, bt_s32_xyz_t* pos, u16 yrot, bt_obj_setup_t* setup);
#define bt_fn_spawn_prop ((bt_fnt_spawn_prop)0x80108C90)

typedef bool (*bt_fnt_transition_done)();
#define bt_fn_transition_done ((bt_fnt_transition_done)0x800C95D4)

typedef bool (*bt_fnt_sparkle)(bt_xyz_t*, u16 type);
#define bt_fn_sparkle ((bt_fnt_sparkle)0x800888C8)

typedef bool (*bt_fnt_unk_update_obj)(bt_obj_instance_t*);
#define bt_fn_unk_update_obj ((bt_fnt_unk_update_obj)0x801015D0)

typedef int (*bt_fnt_save_get_bits)(u16 offset, u16 bits);
#define bt_fn_save_get_bits ((bt_fnt_save_get_bits)0x800DA564)

typedef int (*bt_fnt_save_set_bits)(u16 offset, u16 value, u16 bits);
#define bt_fn_save_set_bits ((bt_fnt_save_set_bits)0x800DA7A8)

typedef bool (*bt_fnt_poll_controls)();
#define bt_fn_poll_controls ((bt_fnt_poll_controls)0x8001608C)

#endif // BT_FN_INJECTED_H
