/*
 * File: lua-cmd.c
 * Purpose: Implements game commands from Lua (cmd.walk(), etc.)
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
#include "game-cmd.h"

/**
 * Implements cmd.walk(1), etc.
 */
int lua_cmd_walk(lua_State *L)
{
	int n = luaL_checknumber(L, 1);

	if (n < 1 || n > 9 || n == 5) {
		return luaL_error(L, "%d is not a valid direction", n);
	}

	cmdq_push(CMD_WALK);
	cmd_set_arg_direction(cmdq_peek(), 0, n);

 	return 0;
}

luaL_Reg lua_cmd_table[] = {
	{ "walk", lua_cmd_walk },
	{ NULL, NULL }
};


void lua_cmd_init(void)
{
	luaL_newlib(L, lua_cmd_table);
	lua_setglobal(L, "cmd");
}
