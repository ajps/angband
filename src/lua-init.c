/*
 * File: lua-init.c
 * Purpose: Admin function for hooking lua into the game
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
#include "lualib.h"
#include "lauxlib.h"

/* Should just need one global lua state for the game */
lua_State *L;

/**
 * Memory allocation function for Lua, using standard game allocators.
 */
static void *allocator (void *ud, void *ptr, size_t osize, size_t nsize)
{
	if (nsize == 0) {
		mem_free(ptr);
		return NULL;
	} else {
		return mem_realloc(ptr, nsize);
	}
}

int lua_debug_msg(lua_State *L)
{
	const char *text = luaL_checkstring(L, 1);
	if (text != NULL) 
		msg(text);

 	return 0;
}

luaL_Reg lua_debug_table[] = {
	{ "msg", lua_debug_msg },
	{ NULL, NULL }
};

/**
 * Initialise our Lua bindings, set up the VM state, etc.
 */
void lua_init(void)
{
	L = lua_newstate(allocator, NULL);
	luaL_openlibs(L);

	luaL_setfuncs(L, lua_debug_table, 0);
	lua_setglobal(L, "debug");
}

/**
 * Do any cleanup needed for Lua before the game shuts down.
 */
void lua_cleanup(void)
{
	if (L) {
		lua_close(L);
	}
}
