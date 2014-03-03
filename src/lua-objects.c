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
#include "obj-info.h"
#include "project.h"
#include "obj-slays.h"

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

static struct {
	u16b index;				/* the SLAY_ index */
	const char *name;       /* Name of the slay */
	const char *flag_name;  /* Name of the corresponding objet flag */
} slay_table[] = {
	#define SLAY(idx, flag, c, d, e, f, g, h, i, j) \
		{ SL_##idx, #idx, #flag },
	#include "list-slays.h"
	#undef SLAY
};

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
 * Pushes a table containing a whole bundle of combat-related info about
 * the given object.
 */
static int push_combat(lua_State *L, const object_type *o_ptr)
{
	int range, break_chance;
	bool impactful, thrown_effect, too_heavy;

	bool nonweap_slay = FALSE;
	int normal_damage;
	int slay_damage[SL_MAX];
	int slays[SL_MAX];
	int num_slays;
	int i;
	struct blow_info blow_info[STAT_RANGE * 2]; /* (Very) theoretical max */
	int num_entries = 0;

	/* Our return value */
	lua_newtable(L);

	obj_known_misc_combat(o_ptr, &thrown_effect, &range, &impactful, &break_chance, &too_heavy);

	lua_pushboolean(L, thrown_effect);
	lua_setfield(L, -2, "thrown_effect");

	lua_pushboolean(L, impactful);
	lua_setfield(L, -2, "impactful");

	lua_pushnumber(L, break_chance);
	lua_setfield(L, -2, "breakage_chance");

	if (range) {
		lua_pushnumber(L, range);
		lua_setfield(L, -2, "range");
	}

	num_entries = obj_known_blows(o_ptr, STAT_RANGE * 2, blow_info);

	if (!num_entries) {
		/* No blows with this object means all the following melee
		   info is meaningless or misleading - dont add it */
		return 1;
	}

	lua_pushnumber(L, (lua_Number) blow_info[0].centiblows / 100);
	lua_setfield(L, -2, "current_blows");
	
	if (num_entries > 1) {
		lua_newtable(L);
		for (i = 0; i < num_entries; i++) {
			lua_pushnumber(L, i); /* array key */
			lua_newtable(L); 
			lua_pushnumber(L, (lua_Number) blow_info[i].str_plus);
			lua_setfield(L, -2, "str_plus");
			lua_pushnumber(L, (lua_Number) blow_info[i].dex_plus);
			lua_setfield(L, -2, "dex_plus");
			lua_pushnumber(L, (lua_Number) blow_info[0].centiblows / 100);
			lua_setfield(L, -2, "blows");
			lua_settable(L, -3);
		}
		lua_setfield(L, -2, "extra_blows");
	}

	num_slays = obj_known_damage(o_ptr, &normal_damage, slays, slay_damage, &nonweap_slay);

	lua_pushnumber(L, (lua_Number) normal_damage / 10);
	lua_setfield(L, -2, "avg_damage");

	lua_pushboolean(L, nonweap_slay);
	lua_setfield(L, -2, "nonweapon_slays");

	if (num_slays) {
		lua_newtable(L);
		for (i = 0; i < num_slays; i++) {
			lua_pushnumber(L, (lua_Number) slay_damage[i] / 10);
			lua_setfield(L, -2, slay_table[slays[i]].flag_name + 3);
		}
		lua_setfield(L, -2, "slay_damage");
	}

	return 1;
}

/**
 * Pushes the nourishment provided by the given object on to the stack.  
 * If the object is not fully known but can be eaten, this is 'true',
 * otherwise it is the number of turns of nourishment or nil is this is none.
 */
static int push_nourishment(lua_State *L, const object_type *o_ptr)
{
	int nourishment = obj_known_food(o_ptr);

	if (nourishment == OBJ_KNOWN_PRESENT) {
		lua_pushboolean(L, TRUE);
	} else if (nourishment) {
		lua_pushnumber(L, nourishment);
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
	int deciturns[DIGGING_MAX];
	static const char *names[4] = { "rubble", "magma veins", "quartz veins", "granite" };

	/* Get useful info or return nothing */
	if (!obj_known_digging(o_ptr, deciturns)) return 0;

	/* Our return value */
	lua_newtable(L);

	for (i = DIGGING_RUBBLE; i < DIGGING_DOORS; i++)
	{
		if (deciturns[i] > 0) {
			lua_pushnumber(L, deciturns[i] / 10);
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
		return push_combat(L, o_ptr);
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
