#include "dualboot.h"
#include "dma.h"

#ifdef DUALBOOT_AUTOTEST
#define DUALBOOT_AUTOTEST_ON 1
#else
#define DUALBOOT_AUTOTEST_ON 0
#endif

// The test settings of this build and the contract number this C was compiled with, for the
// superproject's tools, which cannot tell either from the code (the image builder copies the settings
// into the image and compares the number with the launcher's BKBTCONT tag, see dualboot.h; the
// round-trip check takes an image that does not leave Tooie by itself only when it is told to press
// the chord there, --tooie-chord-after, and refuses that option on an image that does). They look for
// the eight bytes BKBTAUTO in the ROM, expect them exactly once and read the four big-endian words
// behind them. Nothing in the hack reads the record: external linkage and `used` keep it in the object,
// and the bundle is linked without section garbage collection. The tag is spelled as characters so that
// no string literal can put a second copy of it into the ROM.
const struct {
  char tag[8];
  u32 autotest;   // 1 = built with -DDUALBOOT_AUTOTEST
  u32 trips;      // DUALBOOT_AUTOTEST_TRIPS
  u32 bk_return;  // DUALBOOT_AUTOTEST_RETURN
  u32 contract;   // DUALBOOT_CONTRACT, the C copy
} dualboot_test_settings __attribute__((used)) = {
  {'B', 'K', 'B', 'T', 'A', 'U', 'T', 'O'},
  DUALBOOT_AUTOTEST_ON, DUALBOOT_AUTOTEST_TRIPS, DUALBOOT_AUTOTEST_RETURN, DUALBOOT_CONTRACT
};

// Banjo-Kazooie's cart offset, DUALBOOT_BK_BASE: the word of the BKBTBASE tag in asm/dualboot.asm, which
// has the only definition
extern const u32 dualboot_bk_base_word;

// The first word of a big-endian N64 ROM header (the PI bus settings the console's boot code reads from
// it). Banjo-Kazooie NTSC-U 1.0 and the fork's build both start with it (the first four bytes of the
// two ROM files, 2026-10-12).
#define DUALBOOT_ROM_HEADER 0x80371240

// The only caller of the launcher: the chord, the autotest and the relaunch at boot all end here. It
// looks first whether a Banjo-Kazooie is there at all, by the header word at its cart offset, read
// through the PI like every cart word the hack reads (io_read; 0x10000000 + offset is the cart address
// the launcher gives the PI for the same ROM, asm/dualboot.asm). The hack ROM also runs by itself (the
// .patch, hack/test_bt_boot.sh in the superproject), and there the launcher's first PI DMA would read
// from beyond the ROM. A real controller can produce the chord (bt/controllers.h), so this is not a
// path only a test reaches.
// Returns only when there is no Banjo-Kazooie, and then has stored nothing: no handoff is pending.
// `handoff` records the launch first, so that a reset inside Banjo-Kazooie comes back to it
// (dualboot_boot_check).
static void dualboot_start_bk(u32 handoff) {
  if (io_read(0x10000000 + dualboot_bk_base_word) != DUALBOOT_ROM_HEADER) return;
  if (handoff) {
    DUALBOOT_HANDOFF[0] = DUALBOOT_MAGIC;
    DUALBOOT_HANDOFF[1] = DUALBOOT_GAME_KAZOOIE;
  }
  // Through the cache, like every C store to the block: the launcher writes the caches back before it
  // adds its own words through KSEG1 (the token and the payload address share a cache line with these two)
  DUALBOOT_BK_COUNTDOWN = DUALBOOT_AUTOTEST_RETURN;
  DUALBOOT_BK_FLAGS_WORD = DUALBOOT_BK_FLAGS;
  dualboot_launch_bk();
}

void dualboot_boot_check(void) {
  u32 pending = DUALBOOT_HANDOFF[0] == DUALBOOT_MAGIC;
  if (!pending) {  // cold boot, or a reset with nothing pending: the counters start again
    DUALBOOT_BK_LAUNCHES = 0;
    DUALBOOT_BT_LAUNCHES = 0;
  }
#ifdef DUALBOOT_BOOT_BK
  dualboot_start_bk(0);  // test build: go straight to BK at cold boot
#endif
  if (!pending) return;
  DUALBOOT_HANDOFF[0] = 0;  // one-shot: the next cold boot is Tooie again
  // DUALBOOT_GAME_TOOIE: Banjo-Kazooie sent us back, nothing more to do than boot
  if (DUALBOOT_HANDOFF[1] != DUALBOOT_GAME_KAZOOIE) return;
  dualboot_start_bk(0);  // a reset inside Banjo-Kazooie
  // Still here: the block asked for a Banjo-Kazooie that this ROM does not have (RAM left by another
  // image, the block is above everything a reset clears). The magic is gone; the game word follows, so
  // that the block reads as it does after a return from Banjo-Kazooie and names the game that runs.
  DUALBOOT_HANDOFF[1] = DUALBOOT_GAME_TOOIE;
}

void dualboot_request_bk(void) {
  dualboot_start_bk(1);
}

// Whether the title-screen chord may switch (dualboot_title_chord). Zero at every Tooie boot: the boot
// stub loads the whole payload, its data included, from the cart again (boot/boot.c).
static u8 dualboot_chord_armed = 0;

// The chord on the title screen: main.c calls this on every title-screen frame and says whether
// L + R + Start is down on pad 0. Armed like the chord in Banjo-Kazooie (bkbt_frame in the fork): it can
// switch only after a title-screen frame since boot on which it was not down. The game's own pressed
// bits will not do: they are an edge against the game's state, which is empty after every boot, so a
// chord still held when Tooie came up after a return from Banjo-Kazooie read as a new press and started
// Banjo-Kazooie again before Tooie had presented a frame (mupen64plus with a plugged controller,
// 2026-10-12). The held bits do: on the first title-screen frame they already have the pad's state
// (bt/controllers.h).
void dualboot_title_chord(u32 down) {
  if (!down) {
    dualboot_chord_armed = 1;
    return;
  }
  if (!dualboot_chord_armed) return;
  // One look at the cart per press: without a Banjo-Kazooie the request comes back, and the chord then
  // has to be let go before it asks again.
  dualboot_chord_armed = 0;
  dualboot_request_bk();
}

// Where that flag is, for the superproject's check tools: the eight bytes BKBTARMD, once in the ROM,
// followed by the flag's address (KSEG0, big-endian); the flag is one byte. Nothing in the hack reads
// the record (see the BKBTAUTO record for how it stays in the ROM and why the tag is spelled out).
// A tool that drives the pad by writing RAM can do so only when the emulator presents a frame, and Tooie
// has drawn its first title-screen frame by then with nothing held: the flag was 1 on the first frame
// presented after a cold boot and after a return from Banjo-Kazooie (mupen64plus, no controller
// plugged, 2026-10-12), so a chord such a tool "holds from Tooie's first frame" is a new press and
// switches. To stand in for a chord held since before the boot, the tool clears the flag together with
// its first write of the pad. That is the state a controller held through the boot leaves: with a
// plugged controller the flag was 0 and held was the chord on that first frame, and Tooie stayed until
// the chord was let go and pressed again (same day). The flag also shows a tool what the hack made of
// the pad: it goes to 1 on a title-screen frame without the chord and back to 0 when the chord fires.
const struct {
  char tag[8];
  u8* armed;
} dualboot_chord_tag __attribute__((used)) = {
  {'B', 'K', 'B', 'T', 'A', 'R', 'M', 'D'},
  &dualboot_chord_armed
};
