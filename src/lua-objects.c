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

/**
 * Creates & initialises a new object userdata, pushes it to the stack
 */
static struct object_udata *lua_newobject(lua_State *L, int obj_idx) {
	struct object_udata *obj;

	obj = lua_newuserdata(L, sizeof(struct object_udata));
	luaL_setmetatable(L, "object");
	obj->idx = obj_idx;

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
	luaL_newlib(L, lua_objects_table);
	lua_setglobal(L, "objects");

	luaL_newmetatable(L, "object");
	lua_pushstring(L, "__index");
	lua_pushcfunction(L, lua_object_meta_index);
	lua_settable(L, -3);

	lua_pushstring(L, "__tostring");
	lua_pushcfunction(L, lua_object_meta_tostring);
	lua_settable(L, -3);
}
