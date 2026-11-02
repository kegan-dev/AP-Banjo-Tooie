#include "dualboot.h"
#include "save.h"
#include "bt.h"

// BKBT: the hub door (dual-boot contract 3; dualboot.h, docs/ROADMAP.md in the superproject, step 2).
//
// Out: with the combined client's hub_door setting (shared/include/ap_memory/pc.h), Spiral Mountain's entrance to
// the ruined Lair starts Banjo-Kazooie, to arrive in its Lair. The load the game asked for (BT_MAP_SM -> 0x0AD,
// exit 1) is turned into Spiral Mountain again at the way out of the ruined Lair: the game saves at every scene
// change (save_eeprom_write sets save_dirty), this one too, and the next post_loop starts BK
// (dualboot_start_bk writes a save still pending first). mupen64plus, 2026-10-31.
//
// In: a boot that came back through BK's Lair exit has an arrival (dualboot_boot_check). It leaves the title for
// the file select as the title itself does, chooses the file of the client's seed (or the first file of no seed,
// a new game) by pressing for the player, and the load that file asks for goes to the arrival instead: a new
// file asks (0x0A1, 0), which the hack makes (BT_MAP_SM, 4), a file played only in Spiral Mountain (BT_MAP_SM,
// 4), one that has been to Jinjo Village (0x142, 3). All found on mupen64plus on 2026-10-31.
//
// Every entry here is called from C the game reaches through its own jal (pre_load_scene, check_tiptoe,
// post_loop): none from a hook behind the *_displaced trampolines (asm/displaced.asm), whose saved registers
// lie in the argument home area of the C they call, where a callee with calls of its own spills its arguments
// (an added call in the file select's cursor hook hung the game: 2026-10-31).

// libultra's OSContPad of pad 0, the game's copy of the controller's last answer, which its controller update
// reads (bt/controllers.h): the button word and the stick's x. A press put there before bt_fn_poll_controls
// is taken as the player's; one put into bt_controllers after the poll is not (2026-10-31).
// The hub_door setting sits in the padding before silo_requirements and moves nothing: the stock Tooie client
// writes the settings at their offsets (client/game.py of the Tooie world: SILO_REQUIREMENTS_BASE 0x26), and the
// combined client writes this byte at +0x25 (worlds/banjo_kazooie_tooie/client/bridge.py, HUB_DOOR_SETTING).
#define DOOR_SETTING_OFFSET(member) \
  (__builtin_offsetof(ap_memory_pc_t, settings.member) - __builtin_offsetof(ap_memory_pc_t, settings))
_Static_assert(DOOR_SETTING_OFFSET(hub_door) == 0x25, "hub_door is not at settings + 0x25");
_Static_assert(DOOR_SETTING_OFFSET(silo_requirements) == 0x26, "hub_door moved silo_requirements");
_Static_assert(DOOR_SETTING_OFFSET(green_relics_boss_requirement) == 0x57, "hub_door moved the settings");

#define DOOR_PAD_BUTTONS (*(vu16*)0x80079B40)
#define DOOR_PAD_STICK_X (*(vs8*)0x80079B42)
#define DOOR_BUTTON_A 0x8000

static u8 door_leaving = 0;      // the door was taken: Banjo-Kazooie on the next post_loop
static u16 door_title_frames = 0;
static u16 door_select_frames = 0;
#ifdef DUALBOOT_DOOR_TEST
static u16 door_test_frames = 0;
#endif

void door_pre_load_scene(u16* scene, u16* exit) {
  if (bt_current_map != BT_MAP_SM || *scene != BT_MAP_0x0AD) return;
  if (!ap_memory.pc.settings.hub_door || !dualboot_bk_present()) return;
  *scene = BT_MAP_SM;
  *exit = DUALBOOT_DOOR_TOOIE_EXIT;
  door_leaving = 1;
}

void door_arrival_scene(u16* scene, u16* exit) {
  if (!dualboot_arrival || bt_current_map != BT_MAP_FILE_SELECT) return;
  dualboot_arrival = 0;
  *scene = dualboot_arrival_map;
  *exit = dualboot_arrival_exit;
}

// The file the arrival plays: the one of the client's seed, else the first of no seed; 0xFF when every file is
// another seed's, and the player then chooses (the cursor hook refuses those, main.c).
static u8 door_target_slot(u32 seed) {
  for (u8 i = 0; i < 3; i++) if (save_data.custom[i].seed == seed) return i;
  for (u8 i = 0; i < 3; i++) if (save_data.custom[i].seed == 0) return i;
  return 0xFF;
}

void door_pad(void) {
  if (!dualboot_arrival || bt_current_map != BT_MAP_FILE_SELECT) return;
  u32 seed = ap_memory.pc.settings.seed;
  // The client writes the seed and then, in the same look, the items (BTClient.py, emu_loader_monitor_task):
  // a second is plenty. Then a press lasts two polls, one every half second, which the menu takes.
  u32 t = door_select_frames;
  if (!seed || t < 60 || (t - 60) % 30 >= 2) return;
  u8 target = door_target_slot(seed);
  if (target == 0xFF) return;
  if (bt_save_slot != target) DOOR_PAD_STICK_X = (bt_save_slot == 0xFF || bt_save_slot < target) ? 80 : -80;
  else DOOR_PAD_BUTTONS |= DOOR_BUTTON_A;
}

void door_loop(void) {
  if (door_leaving) {
    door_leaving = 0;
    dualboot_request_bk_to(DUALBOOT_DOOR_BK_MAP, DUALBOOT_DOOR_BK_EXIT);  // returns only without a BK
  }
  if (dualboot_arrival) {
    if (bt_current_map == BT_MAP_TITLE_SCREEN) {
      // Leave the title as the title does (main.c's pre_load_scene sees the load), half a second in
      if (++door_title_frames == 30) bt_fn_load_scene(BT_MAP_FILE_SELECT, 0, 0);
    } else if (bt_current_map == BT_MAP_FILE_SELECT) {
      door_select_frames = ap_memory.pc.settings.seed ? door_select_frames + 1 : 0;
    }
  }
#ifdef DUALBOOT_DOOR_TEST
  // test build: n frames in Spiral Mountain, then through the door as if Banjo had walked in
  if (BT_IN_GAME && bt_current_map == BT_MAP_SM && !dualboot_arrival && DUALBOOT_BK_LAUNCHES < DUALBOOT_AUTOTEST_TRIPS) {
    if (++door_test_frames == DUALBOOT_DOOR_TEST) bt_fn_load_scene(BT_MAP_0x0AD, 1, 0);
  } else {
    door_test_frames = 0;
  }
#endif
}
