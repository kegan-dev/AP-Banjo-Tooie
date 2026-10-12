#ifndef BT_CONTROLLERS_H
#define BT_CONTROLLERS_H

typedef struct {
  u8 a : 1;
  u8 b : 1;
  u8 z : 1;
  u8 start : 1;
  u8 dup : 1;
  u8 ddown : 1;
  u8 dleft : 1;
  u8 dright : 1;

  // The struct mirrors the controller's raw button word (libultra OSContPad.button), so this is 0x0080,
  // the bit a controller sets when L, R and Start are all down: it recalibrates its stick and reports
  // that instead of Start (start then reads 0). A console and ares do that; Project64 and mupen64plus
  // report start. The game's own controller code keeps the bit: its update (0x8001608C) stores the raw
  // word of its OSContPad array (0x80079B40, 6 bytes a pad) as held and derives pressed and released
  // from it with no mask (the stores at 0x80016540..0x80016570, read out of RAM on 2026-10-12). Checked
  // on mupen64plus that day by writing raw words into that array: 0x0080 came through in all three.
  // Not checked: a real controller, or ares, delivering the bit.
  // The array is libultra's: osContGetReadData (0x80030014) fills it, called by the game's controller
  // thread (its loop at 0x8001662C) when a read has come back; the reads are started at 0x80016810
  // (osContStartReadData, 0x8002FF90) from the message loop at 0x80014A88. All read out of RAM on
  // 2026-10-12. With a controller plugged in on mupen64plus, held had the pad's state on the first
  // title-screen frame after a boot (same day), so a read had come back before the game's first frame.
  // The arming of the dual-boot chord (dualboot_title_chord in dualboot.c) leans on that; it is not
  // known for a console.
  u8 reset : 1;
  u8 _unused : 1;
  u8 l : 1;
  u8 r : 1;
  u8 cup : 1;
  u8 cdown : 1;
  u8 cleft : 1;
  u8 cright : 1;
} bt_controller_buttons_t;

typedef struct {
  bt_controller_buttons_t held;
  bt_controller_buttons_t held_delayed;
  bt_controller_buttons_t pressed;
  bt_controller_buttons_t released;
  struct {
    float x;
    float y;
  } joystick;
} bt_controller_t;
#define bt_controllers ((bt_controller_t*)0x80079AF8)

#endif // BT_CONTROLLERS_H
