/*
 * File: obj-info.h
 * Purpose: Object description code.
 *
 * Copyright (c) 2010 Andi Sidwell
 * Copyright (c) 2004 Robert Ruehlmann
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#ifndef OBJECT_INFO_H
#define OBJECT_INFO_H

#include "z-textblock.h"

/*
 * Describes the number of blows possible for given stat bonuses
 */
struct blow_info {
	int str_plus;
	int dex_plus;  
	int centiblows;
};

/* Denotes the property being present, but specifics being unknown */
#define OBJ_KNOWN_PRESENT -1

textblock *object_info(const object_type *o_ptr, oinfo_detail_t mode);
textblock *object_info_ego(struct ego_item *ego);
void object_info_spoil(ang_file *f, const object_type *o_ptr, int wrap);
void object_info_chardump(ang_file *f, const object_type *o_ptr, int indent, int wrap);

int obj_known_blows(const object_type *o_ptr, int max_num, struct blow_info possible_blows[]);
int obj_known_damage(const object_type *o_ptr, int *normal_damage, int slay_list[], int slay_damage[], bool *nonweap_slay);
void obj_known_misc_combat(const object_type *o_ptr, bool *thrown_effect, int *range, bool *impactful, int *break_chance, bool *too_heavy);
bool obj_known_digging(const object_type *o_ptr, int deciturns[]);
int obj_known_food(const object_type *o_ptr);
bool obj_known_light(const object_type *o_ptr, oinfo_detail_t mode, int *rad, bool *uses_fuel, int *refuel_turns);
bool obj_known_effect(const object_type *o_ptr, int *effect, bool *aimed, int *min_recharge, int *max_recharge, int *failure_chance);

#endif /* OBJECT_INFO_H */
