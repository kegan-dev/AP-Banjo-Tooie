#include "cheato.h"
#include "ap.h"
#include "main.h"

// BKBT: Cheato in King Jingaling's zombified throne room (Tooie map 0x19B), for the combined slot (docs/ROADMAP.md in
// the superproject, "Cheato moved"; the owner's decision of 2026-11-14). In the combined slot the hub door takes the
// ruined Lair's place (door.c), where Cheato gives his five rewards. With the combined client's hub_door setting he is
// spawned in the zombified throne room instead, once a visit, as soon as the map is in play (no transition, no
// cutscene, no load). It is the throne room a player reaches: the palace's door in Jinjo Village leads there. Map 0x144,
// the throne room before the King is zapped, is the opening's only (the owner's hand test of 2026-11-14 could not reach
// Cheato there). His code is the game's (BT_OBJ_CHEATO), patched as it loads by pre_object_init (main.c), as in the Lair: with
// cheato_rewards a reward sets its fake flag, which the client sends, and ap_sync_items takes his 5 pages a reward from
// the count. A Tooie slot beside a Banjo-Kazooie slot (the stock client, no hub_door) keeps him in the ruined Lair.
//
// How the ruined Lair spawns him (DUALBOOT_CHEATO_TEST's record, below; tools/dualboot_cheato_check.py, mupen64plus,
// 2026-11-14): prop 0x0135 at (600, 0, -17), y rotation 281, with the setup record below but for its position and its
// setup id (2 there); his code loads with that spawn, and he spawns props 0x0243 of his own at his feet. The same spawn
// in the throne room of the opening (0x144) talked, gave reward 1 with 5 pages and took them, the first time it was
// tried; the zombified one is the same room.
//
// DUALBOOT_CHEATO_TEST=n (a test build, with DUALBOOT_DOOR_TEST): every prop the game spawns on a map and the object
// code each spawn loaded, in a record for the superproject's check (BKBTCHTO below), cleared at every scene load; n
// frames after Cheato is spawned Banjo is moved next to him, once a visit; and the check's reload byte reloads the
// throne room, a second visit.

#define CHEATO_MAP   0x019B     /* BT_MAP_0x19B, JV - Jingaling's Zombified Palace (bt/maps.h) */
#define CHEATO_EXIT  1          /* the throne room's way in from Jinjo Village (the check arrives there) */
#define CHEATO_PROP  0x0135     /* the ruined Lair's spawn of Cheato */
#define CHEATO_YROT  281        /* as in the ruined Lair: facing the room's middle from its right */
// In the zombified throne room, as in 0x144, Banjo arrives at (0, -1000, 2121) from Jinjo Village (exit 1), facing into
// the room, and the zombified King (prop 0x0147) is on his dais at (0, -650, -1750) (the record, 2026-11-14). Cheato
// stands on the floor to the right of the way in, where he is in the ruined Lair too: 600 to the right of the middle.
static bt_s32_xyz_t cheato_pos = {600, -1000, 1500};
// The ruined Lair's setup record of Cheato (02580000ffef 190c 0135 0000 8c800064 00400198), at the throne room's spot
// and with a setup id that no setup of the room has (0x19B's own are 0x001..0x010, 0x144's 0x001..0x012). Static: the
// game may keep the pointer
static bt_obj_setup_t cheato_setup = {
  .pos = {600, -1000, 1500},
  .obj_data = 0x032, ._unknown1 = 0x0C,
  .type = CHEATO_PROP,
  ._unknown2 = 0,
  ._unknown3 = 0x8C800064,
  .id = 0x7FD, ._unknown4 = 0x198,
};
static u8 cheato_spawned = 0;

#ifdef DUALBOOT_CHEATO_TEST
typedef struct {
  u16 map;               /* +0x00 bt_current_map at the spawn */
  u16 id;                /* +0x02 the spawn's id (bt_fn_spawn_prop's first argument) */
  bt_s32_xyz_t pos;      /* +0x04 */
  u32 yrot;              /* +0x10 */
  bt_obj_setup_t setup;  /* +0x14 a copy, zero when the spawn has none */
  u16 has_setup;         /* +0x28 */
  u16 code_type;         /* +0x2A bt_object_t.objType of the object code this spawn loaded, 0 if none */
  u32 instance;          /* +0x2C what the spawn returned */
  u32 init;              /* +0x30 the instance's fn_obj_init */
  s16 data_type;         /* +0x34 the instance's data->type */
  u16 _pad;
} cheato_probe_t;
_Static_assert(sizeof(cheato_probe_t) == 0x38, "the check reads the record at this size");

static cheato_probe_t cheato_probes[128];
static u32 cheato_probe_count = 0;
// the spawns in progress, innermost last: a spawn's object code can spawn others before the spawn returns
static s32 cheato_probe_stack[8];
static u32 cheato_probe_depth = 0;
static u16 cheato_test_frames = 0;
static u8 cheato_reload = 0;     // the check sets it: the throne room is loaded again (a second visit)

// The record, for the superproject's check: the eight bytes BKBTCHTO, then the KSEG0 addresses of the spawns, of their
// count and of the reload byte, and the spawns' room (the tag spelled out, as door.c's BKBTDOOR is, so that it is in
// the ROM once).
const struct {
  char tag[8];
  cheato_probe_t* probes;
  u32* count;
  u8* reload;
  u32 size;
} cheato_probe_tag __attribute__((used)) = {
  {'B', 'K', 'B', 'T', 'C', 'H', 'T', 'O'},
  cheato_probes,
  &cheato_probe_count,
  &cheato_reload,
  sizeof(cheato_probes) / sizeof(*cheato_probes)
};
#endif

void cheato_pre_spawn_prop(u16 id, bt_s32_xyz_t* pos, u32 yrot, bt_obj_setup_t* setup) {
#ifdef DUALBOOT_CHEATO_TEST
  s32 index = cheato_probe_count < sizeof(cheato_probes) / sizeof(*cheato_probes) ? (s32)cheato_probe_count++ : -1;
  if (cheato_probe_depth < sizeof(cheato_probe_stack) / sizeof(*cheato_probe_stack)) {
    cheato_probe_stack[cheato_probe_depth] = index;
  }
  cheato_probe_depth++;
  if (index < 0) return;
  cheato_probe_t* p = &cheato_probes[index];
  p->map = bt_current_map;
  p->id = id;
  p->pos = *pos;
  p->yrot = yrot;
  if (setup) p->setup = *setup;
  else {
    bt_obj_setup_t none = {0, };
    p->setup = none;
  }
  p->code_type = 0;
  p->instance = 0;
  p->init = 0;
  p->data_type = 0;
#endif
}

void cheato_post_spawn_prop(bt_obj_setup_t* setup, bt_obj_instance_t* obj) {
#ifdef DUALBOOT_CHEATO_TEST
  if (!cheato_probe_depth) return;
  s32 index = --cheato_probe_depth < sizeof(cheato_probe_stack) / sizeof(*cheato_probe_stack)
    ? cheato_probe_stack[cheato_probe_depth] : -1;
  if (index < 0) return;
  cheato_probe_t* p = &cheato_probes[index];
  p->has_setup = setup != 0;    /* inject_spawn_prop passes 0 for a spawn with no setup of its own */
  p->instance = (u32)obj;
  if (obj) {
    p->init = (u32)obj->fn_obj_init;
    if (obj->data) p->data_type = obj->data->type;
  }
#endif
}

void cheato_object_init(bt_object_t* obj) {
#ifdef DUALBOOT_CHEATO_TEST
  if (!cheato_probe_depth || cheato_probe_depth > sizeof(cheato_probe_stack) / sizeof(*cheato_probe_stack)) return;
  s32 index = cheato_probe_stack[cheato_probe_depth - 1];
  if (index >= 0) cheato_probes[index].code_type = obj->objType;
#endif
}

void cheato_load_scene(void) {
#ifdef DUALBOOT_CHEATO_TEST
  cheato_probe_count = 0;
  cheato_probe_depth = 0;
  cheato_test_frames = 0;
#endif
  cheato_spawned = 0;
}

void cheato_loop(void) {
  if (!BT_IN_GAME || bt_current_map != CHEATO_MAP || !ap_memory.pc.settings.hub_door) return;
  if (!cheato_spawned) {
    if (bt_temp_flags.in_cutscene || !bt_fn_transition_done() || bt_loading_map.loading) return;
    cheato_spawned = 1;
    bt_s32_xyz_t pos = cheato_pos;
    bt_fn_spawn_prop(CHEATO_PROP, &pos, CHEATO_YROT, &cheato_setup);
    return;
  }
#ifdef DUALBOOT_CHEATO_TEST
  if (cheato_test_frames < DUALBOOT_CHEATO_TEST && ++cheato_test_frames == DUALBOOT_CHEATO_TEST) {
    bt_xyz_t next_to = {.x = cheato_pos.x - 150, .y = cheato_pos.y, .z = cheato_pos.z};
    bt_fn_character_move_to(&next_to, 0.1, 0);
  }
  if (cheato_reload && !bt_loading_map.loading) {
    cheato_reload = 0;
    bt_fn_load_scene(CHEATO_MAP, CHEATO_EXIT, 0);
  }
#endif
}
