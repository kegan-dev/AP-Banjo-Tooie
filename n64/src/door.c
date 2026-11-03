#include "dualboot.h"
#include "save.h"
#include "dma.h"
#include "bt.h"

// BKBT: the hub door (dual-boot contract 4; dualboot.h, docs/ROADMAP.md in the superproject, step 2).
//
// Out: with the combined client's hub_door setting (shared/include/ap_memory/pc.h), Spiral Mountain's entrance to
// the ruined Lair starts Banjo-Kazooie, to arrive in its Lair. The load the game asked for (BT_MAP_SM -> 0x0AD,
// exit 1) is turned into Spiral Mountain again at the way out of the ruined Lair: the game saves at every scene
// change, this one too, inside the load and before it returns (all 56 blocks of the file and the commit, about a
// second of the CPU's count, before the next post_loop: a trace of save_eeprom_write on mupen64plus, 2026-11-02),
// and the next post_loop starts BK (dualboot_start_bk writes the save, still pending, first). The file gets its link first,
// if it has none (save_data.link, save.h): the number that ties it to its Banjo-Kazooie save, which goes along
// with the three files' links (dualboot_start_bk).
//
// In: a boot that came back through BK's Lair exit has an arrival (dualboot_boot_check), and the link of the save
// it came from. Half a second into the title, it leaves for the file select as the title itself does. Once the
// client's seed has been there a second, the file select loads the file of that link (else the file of the
// client's seed, else the first file of no seed, a new game) straight into the arrival, without a press (the
// direct load, 2026-11-02, the owner's request): the game's own steps of starting a file, as its file select
// takes them (bt/misc_functions.h, the session; bt/maps.h, bt_fn_load_scene_with_save), and the hack's own,
// which main.c takes for a file chosen with A and which the direct load has to take too:
//   - choosing the file (main_bt_file_select_cursor, "select"): door_direct_choose, a copy, KEEP IN STEP;
//   - knowing a new file (pre_load_scene's BT_MAP_CUTSCENE_OPENING, which the direct load never asks for):
//     door_direct_new_file, by the file just read, KEEP IN STEP;
//   - the rest of pre_load_scene and post_load_save: shared, since this is a load from the file select.
// From the file select, not the title: a file loaded straight from the title read as new at the next arrival,
// and the save at the door wiped itself. The game writes a save to its spare area and then invalidates the
// file's old one, and after a load from the title it took the area it had just written for the old one
// (a trace of save_eeprom_write and the SRAM on mupen64plus, 2026-11-02; hack/DUALBOOT.md in the superproject,
// "The hub door"). Through the file select the game knows where its files are. With no file to play the curtain
// lifts and the player chooses with A: the load that file asks for goes to the arrival instead
// (door_arrival_scene): a new file asks (0x0A1, 0), which the hack makes (BT_MAP_SM, 4), a file played only in
// Spiral Mountain (BT_MAP_SM, 4), one that has been to Jinjo Village (0x142, 3) (mupen64plus, 2026-10-31).
//
// The curtain: from the moment Banjo walks through the door until he stands at the destination the picture is
// black and the sound silent, so that the player never sees the title or the file select being worked
// (the owner's wish, 2026-11-02). The game's own osViBlack is held at black (door_vi_black replaces its entry)
// and every audio buffer is zeroed before the audio interface gets it (door_ai_buffer, at the game's only call
// of osAiSetNextBuffer). The curtain lifts once the arrival's scene is loaded, or after ten seconds without a
// client's seed, or when no file can be chosen: the player then sees the title or the file select as before.
//
// Every entry here is called from C the game reaches through its own jal (pre_load_scene, post_load_save,
// post_loop) or is a whole function the game calls (door_vi_black, door_ai_buffer): none is a hook behind the
// *_displaced trampolines of the middle of a function (asm/displaced.asm), whose saved registers lie in the
// argument home area of the C they call, where a callee with calls of its own spills its arguments (an added
// call in the file select's cursor hook hung the game: 2026-10-31).

// The hub_door setting sits in the padding before silo_requirements and moves nothing: the stock Tooie client
// writes the settings at their offsets (client/game.py of the Tooie world: SILO_REQUIREMENTS_BASE 0x26), and the
// combined client writes this byte at +0x25 (worlds/banjo_kazooie_tooie/client/bridge.py, HUB_DOOR_SETTING).
#define DOOR_SETTING_OFFSET(member) \
  (__builtin_offsetof(ap_memory_pc_t, settings.member) - __builtin_offsetof(ap_memory_pc_t, settings))
_Static_assert(DOOR_SETTING_OFFSET(hub_door) == 0x25, "hub_door is not at settings + 0x25");
_Static_assert(DOOR_SETTING_OFFSET(silo_requirements) == 0x26, "hub_door moved silo_requirements");
_Static_assert(DOOR_SETTING_OFFSET(green_relics_boss_requirement) == 0x57, "hub_door moved the settings");
// link[] comes after custom[], whose offsets the saves of earlier builds have (save.h): 0xB44 bytes before it
_Static_assert(__builtin_offsetof(save_data_t, link) == 0xB44, "save_data's link[] moved custom[]");

// How long the curtain waits for a client's seed before it shows the player why nothing happens
#define DOOR_CURTAIN_WAIT_MS 10000

static u8 door_leaving = 0;      // the door was taken: Banjo-Kazooie on the next post_loop
static u16 door_title_frames = 0;
static u16 door_select_frames = 0; // file select loops with the client's seed there
static u8 door_direct = 0;       // the direct load: DOOR_DIRECT_ASKED, then DOOR_DIRECT_LOADING, 0 when done
static u8 door_direct_slot = 0;
#define DOOR_DIRECT_ASKED 1
#define DOOR_DIRECT_LOADING 2
static u8 door_curtain = 0;      // the picture held black and the sound silent
static u8 door_vi_wanted = 0;    // what the game last asked osViBlack for, which the curtain's lifting gives it
static u32 door_wait_ms = 0;     // time with the curtain up and no client's seed
static u32 door_last_count = 0;
#ifdef DUALBOOT_DOOR_TEST
static u16 door_test_frames = 0;
#endif

// Where the door's state is, for the superproject's check tool (tools/dualboot_door_check.py): the eight bytes
// BKBTDOOR, once in the ROM, followed by the KSEG0 addresses of dualboot_arrival_link (u32; the check writes it on
// a test build's first boot, which arrives with no save behind it, to stand in for one) and of door_curtain (u8).
// Nothing in the hack reads the record (see the BKBTAUTO record in dualboot.c for how it stays in the ROM and why
// the tag is spelled out).
const struct {
  char tag[8];
  u32* link;
  u8* curtain;
} door_tag __attribute__((used)) = {
  {'B', 'K', 'B', 'T', 'D', 'O', 'O', 'R'},
  &dualboot_arrival_link,
  &door_curtain
};

// osViBlack's own code from its third instruction on: its first two (`addiu sp, sp, -0x18`, `sw ra, 0x14(sp)`)
// are replayed there (asm/displaced.asm) and replaced by a jump here (door_hooks)
extern void door_vi_black_displaced(u8 active);

void door_vi_black(u8 active) {
  door_vi_wanted = active;
  door_vi_black_displaced(door_curtain ? 1 : active);
}

// Every buffer the audio interface plays while the curtain is up is zeroed first: through the data cache, whose
// lines are then written back to RAM, where the interface reads it, and dropped, as libultra prepares a buffer
// for a DMA. Only a buffer that is one: in RDRAM, 8-byte aligned, at most 32 KiB (Tooie's are 0xB80 bytes at
// 0x80075AD8, mupen64plus, 2026-11-02). Not through KSEG1: a build that zeroed it uncached stopped mupen64plus's
// recompiler at a door-out in one test (hack/DUALBOOT.md in the superproject, open items).
s32 door_ai_buffer(void* buffer, u32 bytes) {
  u32 a = (u32)buffer;
  if (door_curtain && a >= 0x80000400 && a + bytes <= 0x80800000 && !(a & 7) && bytes <= 0x8000) {
    u32* p = (u32*)buffer;
    for (u32 i = 0; i < bytes / 4; i++) p[i] = 0;
    data_cache_hit_writeback_invalidate(buffer, bytes);
  }
  return bt_fn_ai_set_next_buffer(buffer, bytes);
}

static void door_curtain_raise(void) {
  door_curtain = 1;
  door_vi_black_displaced(1);
}

static void door_curtain_lift(void) {
  if (!door_curtain) return;
  door_curtain = 0;
  door_vi_black_displaced(door_vi_wanted);
}

void door_hooks(void) {
  util_inject(UTIL_INJECT_JUMP, BT_OS_VI_BLACK, (u32)door_vi_black, 1);
  util_inject(UTIL_INJECT_FUNCTION, BT_AI_SET_NEXT_BUFFER_CALL, (u32)door_ai_buffer, 0);
  // The VI manager is not running yet: osViSetMode will clear the black bit when the game starts it, and the
  // game's own osViBlack(1) at boot, the curtain's from then on, and door_loop's hold it
  if (dualboot_arrival) door_curtain = 1;
}

// A new link for the file at the door: never 0 (no link), never 0xFFFFFFFF (the SRAM's fill, save_init), never
// another file's (xorshift32 from the CPU's count and the seed)
static u32 door_new_link(void) {
  u32 x = C0_COUNT() ^ ap_memory.pc.settings.seed;
  x |= 1;
  for (;;) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    if (x != 0xFFFFFFFF && x != save_data.link[0] && x != save_data.link[1] && x != save_data.link[2]) return x;
  }
}

// The file select's part of choosing a file, for the direct load: a copy of main_bt_file_select_cursor's
// "select" in its normal cursor state (main.c), which the direct load cannot reach. KEEP THE TWO IN STEP. It
// refuses a file of another seed, and any file without a client, and binds the file to the client's seed.
// (L + R held lets a refused file through there; door_target_slot never chooses one, so nothing here does.)
static u8 door_direct_choose(u8 slot, u32 seed) {
  u32* n64_seed = &save_data.custom[slot].seed;
  if (seed == 0 || (seed != *n64_seed && *n64_seed != 0)) return 0;
  *n64_seed = seed;
  return 1;
}

// post_load_save, in the direct load: whether the file just read has never been started, which the file
// select tells by asking for the opening (main.c's pre_load_scene, BT_MAP_CUTSCENE_OPENING). KEEP THE TWO IN
// STEP. Such a file reads with no Jinjo pattern: the intro sets it ("set after intro cutscene",
// shared/include/bt/save_flags.h), and so does the hack's start of a new file, which skips the intro
// (ap_new_file). The direct load ends here.
u8 door_direct_new_file(void) {
  if (door_direct != DOOR_DIRECT_LOADING) return 0;
  door_direct = 0;
  return !bt_flags.jinjo_pattern;
}

void door_pre_load_scene(u16* scene, u16* exit) {
  if (door_direct == DOOR_DIRECT_ASKED) {
    if (bt_current_map != BT_MAP_FILE_SELECT || *scene != dualboot_arrival_map || *exit != dualboot_arrival_exit) {
      door_direct = 0;  // another load got there first: not the file's
    } else {
      // The game's part, as its file select starts a file (bt/misc_functions.h, the session): the file the
      // load reads (the file select sets it for the file under its cursor), and the actions it runs once it has
      // read it
      door_direct = DOOR_DIRECT_LOADING;
      bt_fn_session_set_slot(door_direct_slot);
      bt_fn_session_new_actions();
      bt_fn_session_queue_mode(0);
      bt_fn_session_queue_random_seed();
    }
  }
  if (bt_current_map != BT_MAP_SM || *scene != BT_MAP_0x0AD) return;
  if (!ap_memory.pc.settings.hub_door || !dualboot_bk_present()) return;
  *scene = BT_MAP_SM;
  *exit = DUALBOOT_DOOR_TOOIE_EXIT;
  door_leaving = 1;
  if (bt_save_slot <= 2 && save_data.link[bt_save_slot] == 0) {
    save_data.link[bt_save_slot] = door_new_link();
    save_dirty = 1;
  }
  door_curtain_raise();
}

void door_arrival_scene(u16* scene, u16* exit) {
  if (!dualboot_arrival || bt_current_map != BT_MAP_FILE_SELECT) return;
  dualboot_arrival = 0;
  *scene = dualboot_arrival_map;
  *exit = dualboot_arrival_exit;
}

// The file the arrival plays: the one of the save's link, else the one of the client's seed, else the first of
// no seed; never one of another seed, which the cursor hook refuses (main.c). 0xFF when there is none, and the
// player then chooses.
static u8 door_target_slot(u32 seed) {
  u32 link = dualboot_arrival_link;
  for (u8 i = 0; i < 3; i++) {
    u32 s = save_data.custom[i].seed;
    if (link && save_data.link[i] == link && (s == seed || s == 0)) return i;
  }
  for (u8 i = 0; i < 3; i++) if (save_data.custom[i].seed == seed) return i;
  for (u8 i = 0; i < 3; i++) if (save_data.custom[i].seed == 0) return i;
  return 0xFF;
}

// The file select of an arrival: once the client's seed has been there a second (the client writes the seed and
// then, in the same look, the items: BTClient.py, emu_loader_monitor_task), the file straight into the arrival;
// the request comes back through pre_load_scene (door_pre_load_scene) when the game loads it, once the file
// select has faded. With no file to play the curtain lifts and the player chooses.
static void door_select(void) {
  u32 seed = ap_memory.pc.settings.seed;
  door_select_frames = seed ? door_select_frames + 1 : 0;
  if (door_direct || door_select_frames < 60) return;
  u8 slot = door_target_slot(seed);
  if (slot == 0xFF || !door_direct_choose(slot, seed)) {
    door_curtain_lift();
    return;
  }
  door_direct_slot = slot;
  door_direct = DOOR_DIRECT_ASKED;
  // as the gcfrontend overlay asks for it when the file select starts a file (bt/maps.h)
  bt_fn_load_scene_with_save(dualboot_arrival_map, dualboot_arrival_exit, 1);
}

void door_loop(void) {
  if (door_leaving) {
    door_leaving = 0;
    dualboot_request_bk_to(DUALBOOT_DOOR_BK_MAP, DUALBOOT_DOOR_BK_EXIT);  // returns only without a BK
    door_curtain_lift();
  }
  u32 count = C0_COUNT();
  u32 ms = door_last_count ? (count - door_last_count) / TICKS_PER_MILLISECOND : 0;
  door_last_count = count;
  if (door_curtain) {
    // osViSetMode, which the game calls as it starts its VI, clears the black bit without osViBlack
    door_vi_black_displaced(1);
    if (dualboot_arrival && !ap_memory.pc.settings.seed) {
      door_wait_ms += ms;
      if (door_wait_ms >= DOOR_CURTAIN_WAIT_MS) door_curtain_lift();
    } else if (!dualboot_arrival && BT_IN_GAME && bt_current_map == dualboot_arrival_map) {
      door_curtain_lift();  // the arrival's scene is loaded: the game shows it as it would
    }
  }
  if (dualboot_arrival) {
    if (bt_current_map == BT_MAP_TITLE_SCREEN) {
      // Leave the title as the title does (main.c's pre_load_scene sees the load), half a second in
      if (++door_title_frames == 30) bt_fn_load_scene(BT_MAP_FILE_SELECT, 0, 0);
    } else if (bt_current_map == BT_MAP_FILE_SELECT) {
      door_select();
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
