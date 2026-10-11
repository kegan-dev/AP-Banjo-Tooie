#include "dualboot.h"

static void dualboot_start_bk(void) {
  DUALBOOT_BK_COUNTDOWN = DUALBOOT_AUTOTEST_RETURN;
  dualboot_launch_bk();
}

void dualboot_boot_check(void) {
  u32 pending = DUALBOOT_HANDOFF[0] == DUALBOOT_MAGIC;
  if (!pending) {  // cold boot, or a reset with nothing pending: the counters start again
    DUALBOOT_BK_LAUNCHES = 0;
    DUALBOOT_BT_LAUNCHES = 0;
  }
#ifdef DUALBOOT_BOOT_BK
  dualboot_start_bk();  // test build: go straight to BK at cold boot
#endif
  if (!pending) return;
  DUALBOOT_HANDOFF[0] = 0;  // one-shot: the next cold boot is Tooie again
  // DUALBOOT_GAME_TOOIE: Banjo-Kazooie sent us back, nothing more to do than boot
  if (DUALBOOT_HANDOFF[1] == DUALBOOT_GAME_KAZOOIE) dualboot_start_bk();  // a reset inside Banjo-Kazooie
}

void dualboot_request_bk(void) {
  DUALBOOT_HANDOFF[0] = DUALBOOT_MAGIC;
  DUALBOOT_HANDOFF[1] = DUALBOOT_GAME_KAZOOIE;
  dualboot_start_bk();
}
