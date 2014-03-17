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

#include "lua.h"

#include "lua-bindings.h"
#include "lua-objects.h"
#include "lua-cmd.h"

#include "obj-util.h"
#include "obj-desc.h"
#include "obj-pval.h"
#include "obj-identify.h"
#include "obj-tval.h"
#include "obj-info.h"
#include "obj-slays.h"
#include "obj-chest.h"
#include "project.h"
#include "squelch.h"

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
static s16b get_known_flag(const object_type *o_ptr, int flag)
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
 * Pushes a table containing flags of the flag type `type` that are
 * known by the player to be on the object `o_ptr`.  `udata_idx` is
 * a stack index for the corresponding userdata.
 */
static int push_flags_table(lua_State *L, int udata_idx, const object_type *o_ptr, int type)
{
	size_t i;
	size_t nflags = 0;
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
				nflags++;
			} else if (flag != 0) {
				lua_pushnumber(L, flag);
				lua_setfield(L, -2, flag_table[i].name);
				nflags++;
			}
		}
	}

	/* Don't cache or return the table if there aren't any flags in it. */
	if (nflags == 0)
		return 0;

	/* Stash a reference in the cache */
	lua_pushnumber(L, type);
	lua_pushvalue(L, -2);
	lua_settable(L, flag_cache_idx);
	lua_remove(L, flag_cache_idx);

	return 1;
}

/**
 * Pushes a table containing information about the light-giving properties of
 * the given object.
 */
static int push_light(lua_State *L, const object_type *o_ptr)
{
	int radius = 0, refuel_turns = 0;
	bool uses_fuel = FALSE;

	if (!obj_known_light(o_ptr, 0, &radius, &uses_fuel, &refuel_turns))
		return 0;

	/* Our return value */
	lua_newtable(L);
	lua_pushnumber(L, radius);
	lua_setfield(L, -2, "radius");
	lua_pushboolean(L, uses_fuel);
	lua_setfield(L, -2, "uses_fuel");

	if (uses_fuel) {
		lua_pushnumber(L, o_ptr->timeout);
		lua_setfield(L, -2, "fuel");
	}

	lua_pushnumber(L, refuel_turns);
	lua_setfield(L, -2, "max_refuel");

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

	if (get_known_flag(o_ptr, OF_SHOW_MULT)) {
		/* Includes shooting power as part of the multplier, if present */
		lua_pushnumber(L, (o_ptr->sval % 10) + get_known_flag(o_ptr, OF_MIGHT));
		lua_setfield(L, -2 , "shooting_multipler");
	}

	if (range) {
		lua_pushnumber(L, range);
		lua_setfield(L, -2, "range");
	}

	lua_pushnumber(L, break_chance);
	lua_setfield(L, -2, "breakage_chance");

	if (get_known_flag(o_ptr, OF_SHOW_DICE)) {
		int dd, ds;
		if (object_attack_plusses_are_visible(o_ptr)) {
			dd = o_ptr->dd;
			ds = o_ptr->ds;
		} else {
			dd = o_ptr->kind->dd;
			ds = o_ptr->kind->ds;
		}

		lua_pushnumber(L, dd);
		lua_setfield(L, -2, "dd");
		lua_pushnumber(L, ds);
		lua_setfield(L, -2, "ds");
	}

	/* Show weapon bonuses */
	if ((tval_is_weapon(o_ptr) || o_ptr->to_d || o_ptr->to_h) &&
			object_attack_plusses_are_visible(o_ptr)) {
		lua_pushnumber(L, o_ptr->to_h);
		lua_setfield(L, -2, "to_hit");
		lua_pushnumber(L, o_ptr->to_d);
		lua_setfield(L, -2, "to_damage");
	}

	lua_pushboolean(L, impactful);
	lua_setfield(L, -2, "impactful");

	if (obj_desc_show_armor(o_ptr)) {
		if (object_defence_plusses_are_visible(o_ptr) || object_was_sensed(o_ptr))
			lua_pushnumber(L, o_ptr->ac);
		else
			lua_pushnumber(L, o_ptr->kind->ac);
		
		lua_setfield(L, -2, "ac");
	}

	if (object_defence_plusses_are_visible(o_ptr) && o_ptr->to_a) {
		lua_pushnumber(L, o_ptr->to_a);
		lua_setfield(L, -2, "ac_bonus");
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

static int push_charges(lua_State *L, const object_type *o_ptr)
{
	bool aware = object_flavor_is_aware(o_ptr) || (o_ptr->ident & IDENT_STORE);

	/* Wands and Staffs have charges */
	if (aware && tval_can_have_charges(o_ptr))
		lua_pushnumber(L, o_ptr->pval[DEFAULT_PVAL]);
	else
		lua_pushnil(L);

	return 1;
}

static int push_num_charging(lua_State *L, const object_type *o_ptr)
{
	/* Would be nice to push nil if the item couldn't be charging */
	lua_pushnumber(L, number_charging(o_ptr));
	return 1;
}

static int push_chest(lua_State *L, const object_type *o_ptr)
{
	if (!object_is_known(o_ptr)) {
		lua_pushstring(L, "unknown");
	}
	else if (!o_ptr->pval[DEFAULT_PVAL])
		lua_pushstring(L, "empty");
	else if (!is_locked_chest(o_ptr))
	{
		if (chest_trap_type(o_ptr) != 0)
			lua_pushstring(L, "disarmed");
		else
			lua_pushstring(L, "unlocked");
	}
	else
	{
		/* Describe the traps */
		switch (chest_trap_type(o_ptr))
		{
			case 0:
				lua_pushstring(L, "locked");
				break;
			case CHEST_LOSE_STR:
			case CHEST_LOSE_CON:
//				lua_pushstring("trapped");
				lua_pushstring(L, "Poison Needle");
				break;
			case CHEST_POISON:
			case CHEST_PARALYZE:
//				lua_pushstring("trapped");
				lua_pushstring(L, "Gas Trap");
				break;
			case CHEST_EXPLODE:
//				lua_pushstring("trapped");
				lua_pushstring(L, "Explosion Device");
				break;
			case CHEST_SUMMON:
//				lua_pushstring("trapped");
				lua_pushstring(L, "Summoning Runes");
				break;
			default:
//				lua_pushstring("trapped");
				lua_pushstring(L, "Multiple Traps");
				break;
		}
	}
	return 1;
}

static int push_pseudo(lua_State *L, const object_type *o_ptr)
{
	const char *inscrip_text[] =
	{
		NULL,
		"strange",
		"average",
		"magical",
		"splendid",
		"excellent",
		"special",
		"unknown"
	};

	int feel = object_pseudo(o_ptr);

	/* No pseudo if we already know about it. */
	if (object_is_known(o_ptr))
		return 0;
	if (feel)
	{
		/* cannot tell excellent vs strange vs splendid until wield */
		if (!object_was_worn(o_ptr) && o_ptr->ego)
			lua_pushstring(L, "ego");
		else
			lua_pushstring(L, inscrip_text[feel]);
	}
	else if (o_ptr->ident & IDENT_EMPTY)
		lua_pushstring(L, "empty");
	else if (object_was_worn(o_ptr))
	{
		if (wield_slot(o_ptr) == INVEN_WIELD || wield_slot(o_ptr) == INVEN_BOW)
			lua_pushstring(L, "wielded");
		else
			lua_pushstring(L, "worn");
	}
	else if (object_was_fired(o_ptr))
		lua_pushstring(L,  "fired");
	else if (!object_flavor_is_aware(o_ptr) && object_flavor_was_tried(o_ptr))
		lua_pushstring(L, "tried");
	else
		lua_pushnil(L);
	return 1;
}

const char *origins[ORIGIN_MAX] = { "none", "floor", "drop", "chest", 
	"drop_special", "drop_pit", "drop_vault", "special", "pit", "vault",
	"labyrinth", "cavern", "rubble", "mixed", "stats", "acquire", "drop_breed",
	"drop_summon", "store", "stolen", "birth", "drop_unknown", "cheat",
	"drop_poly", "drop_wizard" 
};

static int push_origin(lua_State *L, const object_type *o_ptr)
{
	lua_pushstring(L, origins[o_ptr->origin]);
	return 1;
}

static int push_origin_depth(lua_State *L, const object_type *o_ptr)
{
	lua_pushnumber(L, o_ptr->origin_depth);
	return 1;
}

static int push_name(lua_State *L, const object_type *o_ptr)
{
	char o_name[80];
	object_desc(o_name, sizeof(o_name), o_ptr, ODESC_PREFIX);
	lua_pushstring(L, o_name);
	return 1;
}

static int push_money(lua_State *L, const object_type *o_ptr)
{
	if (!tval_is_money(o_ptr))
		return 0;

	lua_pushnumber(L, o_ptr->pval[DEFAULT_PVAL]);
	return 1;
}

static int push_inscription(lua_State *L, const object_type *o_ptr)
{
	if (!o_ptr->note)
		return 0;

	lua_pushstring(L, quark_str(o_ptr->note));
	return 1;
}

static int push_is_known(lua_State *L, const object_type *o_ptr)
{
	lua_pushboolean(L, object_is_known(o_ptr));
	return 1;
}

static int push_type(lua_State *L, const object_type *o_ptr)
{
	 lua_pushstring(L, o_ptr->kind->name);
	 return 1;
}

static int push_is_squelched(lua_State *L, const object_type *o_ptr)
{
	lua_pushboolean(L, squelch_item_ok(o_ptr));
	 return 1;
}

static int push_flavor_known(lua_State *L, const object_type *o_ptr)
{
	lua_pushboolean(L, object_flavor_is_aware(o_ptr));
	 return 1;
}

static int push_slays(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_SLAY);
}
	
static int push_resists(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_LRES);
}
	
static int push_stats(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_STAT);
}
	
static int push_abilities(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_PVAL);
}
	
static int push_kills(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_KILL);
}

static int push_brands(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_BRAND);
}

static int push_sustains(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_SUST);
}

static int push_vulnerable(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_VULN);
}

static int push_ignores(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_IGNORE);
}

static int push_hates(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_HATES);
}

static int push_curses(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_CURSE);
}

static int push_bad(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_BAD);
}

static int push_protects(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_PROT);
}

static int push_misc_magic(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_MISC);
}

static int push_knowledge(lua_State *L, const object_type *o_ptr) 
{
	return push_flags_table(L, 1, o_ptr, OFT_INT);
}

typedef int (*object_property)(lua_State *L, const object_type *o_ptr);

static struct {
	const char *key;
	object_property fn;
} properties[] = {
	{ "name", push_name },
	{ "origin", push_origin },
	{ "origin_depth", push_origin_depth },
	{ "combat", push_combat },
	{ "light", push_light },
	{ "digging", push_digging },
	{ "chest", push_chest },
	{ "nourishment", push_nourishment },
	{ "pseudo_id", push_pseudo },
	{ "charges", push_charges },
	{ "charging", push_num_charging },
	{ "money", push_money },
	{ "inscription", push_inscription },
	{ "is_known", push_is_known },
	{ "type", push_type },
	{ "is_squelched", push_is_squelched },
	{ "flavor_known", push_flavor_known },
	// { "effect", push_effect },

	/* Flags */
	{ "slays", push_slays },
	{ "resists", push_resists },
	{ "stats", push_stats },
	{ "abilities", push_abilities },
	{ "kills", push_kills },
	{ "brands", push_brands },
	{ "sustains", push_sustains },
	{ "vulnerable", push_vulnerable },
	{ "ignores", push_ignores },
	{ "hates", push_hates },
	{ "curses", push_curses },
	{ "bad", push_bad },
	{ "protects", push_protects },
	{ "misc_magic", push_misc_magic },
	{ "knowledge", push_knowledge },
};

static struct {
	const char *key;
	lua_CFunction fn;
} methods[] = {
	{ "use", lua_cmd_use },
	{ "drop", lua_cmd_drop },
};

/**
 * This is called whenever you attempt to index an object, e.g. obj.use()
 * would call this to find the value of 'use', and then attempt to
 * call it as a function with no arguments.
 *
 * TODO: use methods to identify what we are allowed to do to an object,
 *       e.g. if we aren't carrying the object, make .drop return nil so
 *       that we can't call it.
 */
static int lua_object_meta_index(lua_State *L)
{
	struct object_udata *obj;
	object_type *o_ptr = NULL;
	const char *key;
	size_t i;

	obj = luaL_checkudata(L, 1, "object");
	key = luaL_checkstring(L, 2);
	o_ptr = object_from_item_idx(obj->idx);

	/* TODO: check for a valid object? */

	for (i = 0; i < N_ELEMENTS(methods); i++) {
		if (streq(key, methods[i].key)) {
			lua_pushcfunction(L, methods[i].fn);
			return 1;
		}
	}

	for (i = 0; i < N_ELEMENTS(properties); i++) {
		if (streq(key, properties[i].key)) {
			return properties[i].fn(L, o_ptr);
		}
	}

	return 0;
}

static int object_next(lua_State *L)
{
	struct object_udata *obj;
	object_type *o_ptr = NULL;
	const char *key;
	int nresults = 0;
	size_t i, next = 0;
	int top = 0;

	obj = luaL_checkudata(L, 1, "object");
	o_ptr = object_from_item_idx(obj->idx);
	
	/* Find our starting position */
	if (lua_isnil(L, 2) || lua_isnone(L, 2)) {
		next = 0;
	} else if (lua_isstring(L, 2)) {
		key = lua_tostring(L, 2);

		for (i = 0; i < N_ELEMENTS(properties); i++) {
			if (streq(key, properties[i].key)) {
				next = i + 1;
			}
		}
	} else {
		return luaL_error(L, "Not a valid key");
	}

	/* Then find the next key, value */
	top = lua_gettop(L);
	for (i = next; i < N_ELEMENTS(properties); i++) {
		nresults = properties[i].fn(L, o_ptr);

		/* Need to have a non-nil result to return a key, value pair */
		if (nresults > 0 && !lua_isnil(L, -1)) {
			lua_pushstring(L, properties[i].key);
			lua_insert(L, -2);
			return 2;
		}

		/* Clear any rubbish that might be on the stack. */
		lua_settop(L, top);
	}

	/* If we've got this far, we're done. */
	lua_pushnil(L);
	return 1;
}

/**
 * Returns an iterator that iterates over the object's properties, but not
 * its methods.
 */
static int lua_object_meta_pairs(lua_State *L)
{
	struct object_udata *obj;
	object_type *o_ptr = NULL;

	obj = luaL_checkudata(L, 1, "object");
	o_ptr = object_from_item_idx(obj->idx);

	/* TODO: check for a valid object? */

	lua_pushcfunction(L, object_next);
	lua_pushvalue(L, 1);
	lua_pushnil(L);

	return 3;
}

/*
 * Just useful for debugging purposes to print a bit more info as a
 * description of the object.
 */
static int lua_object_meta_tostring(lua_State *L)
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

	lua_pushstring(L, "__pairs");
	lua_pushcfunction(L, lua_object_meta_pairs);
	lua_settable(L, -3);

	/* For our purposes, all resists are alike */
	for (i = 0; i < OF_MAX; i ++) {
		if (flag_table[i].type == OFT_HRES) flag_table[i].type = OFT_LRES;
	}
}
