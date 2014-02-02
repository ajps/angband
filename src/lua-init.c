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
#include "lua-bindings.h"

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

/* Simple debug helper */
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

	/* Initialise all the tables of commands */
	luaL_newlib(L, lua_debug_table);
	lua_setglobal(L, "debug");

	lua_cmd_init();
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

/**
 * Execute the given lua string in the global lua environment.
 *
 * This passes any errors in the statement being executed out through
 * the msg() system.
 */
void lua_execute(const char *line)
{
	int result = luaL_loadstring(L, line);

	if (result == LUA_OK) {
		result = lua_pcall(L, 0, LUA_MULTRET, 0);
		if (result != LUA_OK) {
			msg("Lua error! ");
			msg(luaL_checkstring(L, 1));
			lua_pop(L, 1);
		}
	} else { 
		msg("Lua compile error! ");
		msg(luaL_checkstring(L, 1));
		lua_pop(L, 1);
	}

	return;
}
