#ifndef CHEATO_H
#define CHEATO_H

#include "util.h"
#include "bt.h"

// BKBT: Cheato in King Jingaling's zombified throne room, for the combined slot (cheato.c).
void cheato_pre_spawn_prop(u16 id, bt_s32_xyz_t* pos, u32 yrot, bt_obj_setup_t* setup);  /* first in pre_spawn_prop */
void cheato_post_spawn_prop(bt_obj_setup_t* setup, bt_obj_instance_t* obj);  /* first in post_spawn_prop */
void cheato_object_init(bt_object_t* obj);              /* first in pre_object_init */
void cheato_load_scene(void);                           /* pre_load_scene */
void cheato_loop(void);                                 /* post_loop */

#endif // CHEATO_H
