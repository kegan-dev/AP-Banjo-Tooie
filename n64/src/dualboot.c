#include "dualboot.h"

void dualboot_boot_check(void) {
#ifdef DUALBOOT_BOOT_BK
  dualboot_launch_bk();  // test build: go straight to BK at cold boot
#endif
  if (DUALBOOT_HANDOFF[0] == DUALBOOT_MAGIC && DUALBOOT_HANDOFF[1] == DUALBOOT_GAME_KAZOOIE) {
    DUALBOOT_HANDOFF[0] = 0;  // one-shot: the next cold boot is Tooie again
    dualboot_launch_bk();
  }
}

void dualboot_request_bk(void) {
  DUALBOOT_HANDOFF[0] = DUALBOOT_MAGIC;
  DUALBOOT_HANDOFF[1] = DUALBOOT_GAME_KAZOOIE;
  dualboot_launch_bk();
}
