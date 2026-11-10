#ifndef AP_MEMORY_PC_H
#define AP_MEMORY_PC_H

#include "items.h"

#define AP_MEMORY_EXIT_MAP_MAX 63

typedef struct {
  u16 on_map;
  u16 og_map;
  u16 to_map;
  u8 og_exit;
  u8 to_exit;
  u8 access_rules[6];
} ap_memory_pc_exit_map_t;

// BKBT: the game each exit_map entry leads into, entry by entry (the doors between the games, n64/src/door.c).
// After exit_map, so that nothing the stock client writes moves: the client finds it at the anchor's exit map
// + 0x372 (AP_MEMORY_EXIT_MAP_MAX * 14), and only the combined Banjo-Kazooie & Tooie client writes it
// (worlds/banjo_kazooie_tooie/client/bridge.py in the superproject). to_game 0 is this game (the stock client
// leaves it 0); AP_MEMORY_EXIT_GAME_BK is Banjo-Kazooie, at the entry's to_map and to_exit in its numbering.
// stay_exit is the exit of the map Banjo is in where the game reloads it before it leaves, so that the file is
// saved there: the spot the door's way back arrives at.
#define AP_MEMORY_EXIT_GAME_BK 1
typedef struct {
  u8 to_game;
  u8 stay_exit;
} ap_memory_pc_exit_game_t;

typedef struct {
  struct {
    u8 death_link_us; // if != n64.death_link_us, send death link and increment
    u8 death_link_ap; // increment to send death link to game
    u8 tag_link_us; // if != n64.tag_link_us, send tag link and increment
    u8 tag_link_ap;  // increment to send tag link to game
    u8 show_message; // only increment if == n64.show_text
  } misc;
  /*
    127 = START
    128 = R
    129 = Z
    130 = C RIGHT
    131 = C UP
    132 = C DOWN
    133 = C LEFT
    134 = B
    135 = A
  */
  u8 message[508];
  u8 signposts[61][150];
  struct {
    u32 seed;
    u8 victory_condition;
    u8 randomize_chuffy;
    u8 randomize_nests;
    u8 randomize_warppads;
    u8 randomize_warpsilos;
    u8 honeyb_rewards;
    u8 cheato_rewards;
    u8 randomize_tickets;
    u8 randomize_green_relics;
    u8 randomize_beans;
    u8 skip_puzzles;
    u8 backdoors;
    u8 gi_open_frontdoor;
    u8 skip_klungo;
    u8 skip_tower_of_tragedy;
    u8 speed_up_minigames;
    u8 dialog_character;
    u8 max_mumbo_tokens;
    u8 signpost_hints;
    u8 extra_cheats;
    u8 automatic_cheats;
    u8 easy_canary;
    u8 jiggy_requirements[11];
    // BKBT: the hub door (n64/src/door.c): not 0 = Spiral Mountain's entrance to the ruined Lair leads into
    // Banjo-Kazooie's Lair. In the padding byte before silo_requirements (+0x25, which nothing else moves),
    // written only by the combined Banjo-Kazooie & Tooie client: the stock client leaves it 0, so a Tooie
    // slot keeps its ruined Lair and Cheato's rewards.
    u8 hub_door;
    u16 silo_requirements[24];
    u8 green_relics_chamber_requirement;
    u8 green_relics_boss_requirement;
  } settings;
  u8 items[AP_ITEM_MAX];
  u8 traps[AP_TRAP_MAX];
  ap_memory_pc_exit_map_t exit_map[AP_MEMORY_EXIT_MAP_MAX];
  ap_memory_pc_exit_game_t exit_game[AP_MEMORY_EXIT_MAP_MAX];  // BKBT: above
} ap_memory_pc_t;

#endif // AP_MEMORY_PC_H
