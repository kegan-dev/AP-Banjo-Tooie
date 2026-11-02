#ifndef BT_AUDIO_H
#define BT_AUDIO_H

typedef u32 (*bt_fnt_play_sound)(u16 id, s64 volume, u16 _unknown, s32 _unknown2);
#define bt_fn_play_sound ((bt_fnt_play_sound)0x800FC508)

// BKBT: libultra's osAiSetNextBuffer(buffer, bytes), which hands the audio interface its next buffer
// (it writes AI_DRAM_ADDR and AI_LEN, 0xA4500000 and 0xA4500004), and the game's only call of it, in its
// audio thread (`jal 0x8002E3C0`, delay slot `move a1, t6`). Found in RAM on mupen64plus at the title
// screen, 2026-11-02 (the superproject's hack/DUALBOOT.md, "The hub door"). door.c's curtain hooks the call.
typedef s32 (*bt_fnt_ai_set_next_buffer)(void* buffer, u32 bytes);
#define bt_fn_ai_set_next_buffer ((bt_fnt_ai_set_next_buffer)0x8002E3C0)
#define BT_AI_SET_NEXT_BUFFER_CALL 0x80012A4C

enum {
  BT_SOUND_COLLECTED_ITEM1,
  BT_SOUND_COLLECTED_NOTE,
  BT_SOUND_COLLECTED_JINJO,
  BT_SOUND_COLLECTED_RED_FEATHER,
  BT_SOUND_COLLECTED_BLUE_EGG,
  BT_SOUND_COLLECTED_JIGGY,
  BT_SOUND_COLLECTED_GOLD_FEATHER,
  BT_SOUND_EXTRA_LIFE,
  BT_SOUND_COLLECTED_HEALTH,
  BT_SOUND_COLLECTED_HONEYCOMB_PIECE,
  BT_SOUND_EXTRA_HEALTH,
  BT_SOUND_DIED,
  BT_SOUND_TRANSFORM,
  BT_SOUND_WONDER_WING,
  BT_SOUND_COLLECTED_ITEM2,
  BT_SOUND_WRONG,
  BT_SOUND_SUCCESS,
  BT_SOUND_COMPLETED_JINJO_FAMILY,
  BT_SOUND_GAME_OVER,
  BT_SOUND_COLLECTED_TREBLE_CLEF = 20,
  BT_SOUND_DINGPOT_REFILL,
  BT_SOUND_WON,
  BT_SOUND_LOST,
  BT_SOUND_JIGGY_SPAWNED,
  BT_SOUND_TIME_UP,
  BT_TRACK_MUMBO_SKULL,
  BT_SOUND_TRIUMPHANT_FANFARE,
  BT_SOUND_SPARKLE,
  BT_SOUND_PUZZLE_FADE_IN,
  BT_SOUND_PUZZLE_FADE_OUT,
  BT_TRACK_TURBO_TRAINERS,
  BT_TRACK_WADERS,
  BT_TRACK_PAUSE = 36,
  BT_SOUND_COLLECTED_TURBO_TRAINERS = 43,
  BT_SOUND_JIGGYWIGGY_CHALLENGE_START = 46, // loading into puzzle
  BT_SOUND_INCREASE,
  BT_SOUND_DECREASE,
  BT_SOUND_JIGGYWIGGY_CHALLENGE_BEGIN, // pieces scattering
  BT_TRACK_SPIRAL_MOUNTAIN_CAVE,
};

#endif // BT_AUDIO_H
