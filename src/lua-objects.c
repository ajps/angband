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

static int lua_objects_inventory(lua_State *L)
{
	int idx = luaL_checknumber(L, 1);

	lua_newobject(L, idx);
	return 1;
}

int lua_object_meta_index(lua_State *L)
{
	struct object_udata *obj;
	const char *key;

	debugf("---> lua_object_meta_index()\n");

	obj = luaL_checkudata(L, 1, "object");
	key = luaL_checkstring(L, 2);

	if (streq(key, "use"))
		lua_pushcfunction(L, lua_cmd_use);

	debugf("<--- lua_object_meta_index()\n");

	return 1;
}


luaL_Reg lua_objects_table[] = {
	{ "inventory", lua_objects_inventory },
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
}
