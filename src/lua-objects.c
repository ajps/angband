/*
 * File: lua-objects.c
 * Purpose: Implements object Lua userdata and functions for 
 *          getting those from the dungeon.
 *
 * Copyright (c) 2014 Antony Sidwell
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
#include "angband.h"
#include "lua-bindings.h"
#include "lua-objects.h"
#include "lua-cmd.h"

#include "obj-util.h"
#include "obj-desc.h"
#include "obj-pval.h"
#include "obj-identify.h"
#include "obj-tval.h"
#include "project.h"

static struct {
	u16b index;				/* the OF_ index */
	bool pval;				/* is it granular (TRUE) or binary (FALSE) */
	u16b type;				/* OFT_ category */
	const char *name;
} flag_table[] = {
#define OF(flag, pval, x, type, ...) { OF_##flag, pval, type, #flag },
#include "list-object-flags.h"
#undef OF
};

#if 0
static struct {
	u16b index;				/* OFT_ category */
	const char *name;
} flag_types[] = {
#define OF(a, b, c, type, ...) { type, #type },
#include "list-object-flags.h"
#undef OF
};
#endif

#define FLAG_SET MAX_SHORT

/**
 * Gets the value of the given flag on the given item, as known by the player.
 *
 * If the flag is unset (or is unknown by the player), returns 0;
 * If the flag is set then FLAG_SET is returned, unless it's a known pval in
 * which case the actual value is returned.
 *
 * Rules should match object_info_out().
 */
static s16b get_known_flag(object_type *o_ptr, int flag)
{
	bitflag flags[OF_SIZE];
	s16b flag_value = 0;

	/* Find out what we know */
	object_flags_known(o_ptr, flags);
	dedup_hates_flags(flags);

	if (of_has(flags, flag)) {
		if (flag_table[flag].pval && object_this_pval_is_visible(o_ptr, which_pval(o_ptr, flag))) {
			flag_value = o_ptr->pval[which_pval(o_ptr, flag)];
			/* Because there's always one... */
			if (flag == OF_SEARCH) flag_value *= 5;
		} else {
			flag_value = FLAG_SET;
		}
	}

	return flag_value;
}


/**
 * Creates & initialises a new object userdata, pushes it to the stack
 */
static struct object_udata *lua_newobject(lua_State *L, int obj_idx) {
	struct object_udata *obj;

	obj = lua_newuserdata(L, sizeof(struct object_udata));
	luaL_setmetatable(L, "object");
	obj->idx = obj_idx;

	/* Create a table to keep lua objects assoicated with this in */
	lua_newtable(L);
	lua_newtable(L);
	lua_setfield(L, -2, "flags");
	lua_setuservalue(L, -2);

	return obj;
}

/**
 *  Creates an object with the given idx - for dev/testing purposes
 */
static int lua_objects_get_idx(lua_State *L)
{
	int idx = luaL_checknumber(L, 1);

	lua_newobject(L, idx);
	return 1;
}

/**
 * Pushes the nourishment provided by the given object on to the stack.  
 * If the object is not fully known but can be eaten, this is 'true',
 * otherwise it is the number of turns of nourishment or nil is this is none.
 */
static int push_nourishment(lua_State *L, const object_type *o_ptr)
{
	if (tval_can_have_nourishment(o_ptr) && o_ptr->pval[DEFAULT_PVAL]) {
		if (object_is_known(o_ptr)) {
			lua_pushnumber(L, o_ptr->pval[DEFAULT_PVAL] / 2);
		} else {
			lua_pushboolean(L, TRUE);
		}
	} else {
		lua_pushnil(L);
	}

	return 1;
}

/**
 * Pushes a table containing the number of turns it will take
 * to dig the diggable terrain types if the given object is
 * wielded by the player.
 */
static int push_digging(lua_State *L, const object_type *o_ptr)
{
	int i;
	player_state st;

	object_type inven[INVEN_TOTAL];

	int sl = wield_slot(o_ptr);

	bitflag f[OF_SIZE];

	int chances[DIGGING_MAX]; /* These are out of 1600 */
	static const char *names[4] = { "rubble", "magma", "quartz", "granite" };

	object_flags_known(o_ptr, f);

	if (sl < 0 || (sl != INVEN_WIELD && !of_has(f, OF_TUNNEL))) {
		lua_pushnil(L);
		return 1;
	}

	memcpy(inven, player->inventory, INVEN_TOTAL * sizeof(object_type));

	/*
	 * Hack -- if we examine a ring that is worn on the right finger,
	 * we shouldn't put a copy of it on the left finger before calculating
	 * digging skills.
	 */
	if (o_ptr != &player->inventory[INVEN_RIGHT])
		inven[sl] = *o_ptr;

	calc_bonuses(inven, &st, TRUE);
	calc_digging_chances(&st, chances);

	/* Our return value */
	lua_newtable(L);

	for (i = DIGGING_RUBBLE; i <= DIGGING_GRANITE; i++)
	{
		int chance = MIN(1600, chances[i]);
		int deciturns = chance ? (16000 / chance) : 0;

		if (chance > 0) {
			lua_pushnumber(L, deciturns / 10);
			lua_setfield(L, -2, names[i]);
		}
	}

	return 1;
}


/**
 * Pushes a table containing flags of the flag type `type` that are 
 * known by the player to be on the object `o_ptr`.  `udata_idx` is
 * a stack index for the corresponding userdata.
 */
static int push_flags_table(lua_State *L, int udata_idx, object_type *o_ptr, int type)
{
	size_t i;
	int flag_cache_idx;

	/* Grab the cache table, leave just that on the stack */
	lua_getuservalue(L, udata_idx);
	lua_getfield(L, -1, "flags");
 	lua_remove(L, -2);  
	flag_cache_idx = lua_gettop(L);

	/* Look for a table for the required flag type */
	lua_pushnumber(L, type); 
	lua_gettable(L, -2); 

	if (lua_istable(L, -1)) {
		/* remove the cache table to return the retrieved flags  */
		lua_remove(L, flag_cache_idx);
		return 1;
	} else {
		lua_pop(L, 1); /* nil */
	}

	/* Our return value */
	lua_newtable(L);

	/* Create new table of values */
	for (i = 0; i < OF_MAX; i++) {
		s16b flag = 0;

		if (flag_table[i].type == type) {
			flag = get_known_flag(o_ptr, flag_table[i].index);

			if (flag == FLAG_SET) {
				lua_pushboolean(L, TRUE);
				lua_setfield(L, -2, flag_table[i].name);
			} else if (flag != 0) {
				lua_pushnumber(L, flag);				
				lua_setfield(L, -2, flag_table[i].name);
			}
		}
	}

	/* Stash a reference in the cache */
	lua_pushnumber(L, type);
	lua_pushvalue(L, -2);
	lua_settable(L, flag_cache_idx); 
	lua_remove(L, flag_cache_idx);

	return 1;
}

/**
 * This is called whenever you attempt to index an object, e.g. obj.use() 
 * would call this to find the value of 'use', and then attempt to
 * call it as a function with no arguments.
 *
 * TODO: use methods to identify what we are allowed to do to an object,
 *       e.g. if we aren't carrying the object, make .drop return nil so
 *       that we can't call it.
 */
int lua_object_meta_index(lua_State *L)
{
	struct object_udata *obj;
	object_type *o_ptr = NULL;
	const char *key;

	obj = luaL_checkudata(L, 1, "object");
	key = luaL_checkstring(L, 2);

	o_ptr = object_from_item_idx(obj->idx);
	/* TODO: check for a valid object? */

	/* METHODS */
	if (streq(key, "use")) {
		lua_pushcfunction(L, lua_cmd_use);
		return 1;
	} else if (streq(key, "drop")) {
		lua_pushcfunction(L, lua_cmd_drop);
		return 1;
	} 

	/* PROPERTIES */
	else if (streq(key, "name")) {
		char o_name[80];
		object_desc(o_name, sizeof(o_name), o_ptr, ODESC_PREFIX);
		lua_pushstring(L, o_name);
		return 1;
	} else if (streq(key, "is_known")) {
		lua_pushboolean(L, object_is_known(o_ptr));
		return 1;
	} else if (streq(key, "effect")) {
		/* An effect object?  A number? */
	} else if (streq(key, "type")) {
	} else if (streq(key, "combat")) {
		/* too_heavy, blows?, range, damage, breakage, thrown_effect */
	} else if (streq(key, "nourishment")) {
		return push_nourishment(L, o_ptr);
	} else if (streq(key, "digging")) {
		return push_digging(L, o_ptr);
	} else if (streq(key, "slays")) {
		return push_flags_table(L, 1, o_ptr, OFT_SLAY);
	} else if (streq(key, "resists")) {
		return push_flags_table(L, 1, o_ptr, OFT_LRES);
	} else if (streq(key, "stats")) {
		return push_flags_table(L, 1, o_ptr, OFT_STAT);
	} else if (streq(key, "abilities")) {
		return push_flags_table(L, 1, o_ptr, OFT_PVAL);
	} else if (streq(key, "kills")) {
		return push_flags_table(L, 1, o_ptr, OFT_KILL);
	} else if (streq(key, "brands")) {
		return push_flags_table(L, 1, o_ptr, OFT_BRAND);
	} else if (streq(key, "sustains")) {
		return push_flags_table(L, 1, o_ptr, OFT_SUST);
	} else if (streq(key, "vulnerable")) {
		return push_flags_table(L, 1, o_ptr, OFT_VULN);
	} else if (streq(key, "ignores")) {
		return push_flags_table(L, 1, o_ptr, OFT_IGNORE);
	} else if (streq(key, "hates")) {
		return push_flags_table(L, 1, o_ptr, OFT_HATES);
	} else if (streq(key, "curses")) {
		return push_flags_table(L, 1, o_ptr, OFT_CURSE);
	} else if (streq(key, "bad")) {
		return push_flags_table(L, 1, o_ptr, OFT_BAD);
	} else if (streq(key, "protects")) {
		return push_flags_table(L, 1, o_ptr, OFT_PROT);
	} else if (streq(key, "misc_magic")) {
		return push_flags_table(L, 1, o_ptr, OFT_MISC);
	} else if (streq(key, "knowledge")) {
		return push_flags_table(L, 1, o_ptr, OFT_INT);
	}

	return 0;
}

/*
 * Just useful for debugging purposes to print a bit more info as a
 * description of the object.
 */
int lua_object_meta_tostring(lua_State *L)
{
	struct object_udata *obj;
	object_type *o_ptr = NULL;
	char o_name[80];
	char string[160] = "object [";

	obj = luaL_checkudata(L, 1, "object");
	o_ptr = object_from_item_idx(obj->idx);

	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_PREFIX);
	strnfmt(string, sizeof(string), "object [%p] %s", obj, o_name);

	lua_pushstring(L, string);

	return 1;
}

luaL_Reg lua_objects_table[] = {
	{ "get_idx", lua_objects_get_idx },
	{ NULL, NULL }
};

void lua_objects_init(void)
{
	int i;

	luaL_newlib(L, lua_objects_table);
	lua_setglobal(L, "objects");

	luaL_newmetatable(L, "object");
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, lua_object_meta_index);
	lua_settable(L, -3);

	lua_pushstring(L, "__tostring");
	lua_pushcfunction(L, lua_object_meta_tostring);
	lua_settable(L, -3);

	/* For our purposes, all resists are alike */
	for (i = 0; i < OF_MAX; i ++) {
		if (flag_table[i].type == OFT_HRES) flag_table[i].type = OFT_LRES;
	}
}
