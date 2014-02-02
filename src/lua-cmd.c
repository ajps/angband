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
 * Handles extracting a direction from the lua stack, pushes the
 * correct command & direction to the game.
 */
static int push_direction_cmd(lua_State *L, cmd_code code)
{
	const char *param;
	int dir;
	
	/* We need at least one parameter - we'll ignore extras */
	luaL_checkany(L, 1);

	/* We'll treat a direction number as a string for simplicity. */
	param = lua_tostring(L, 1);

	if (param) {
		if (streq(param, "SW") || streq(param, "1")) 
			dir = 1;
		else if (streq(param, "S") || streq(param, "2")) 
			dir = 2;
		else if (streq(param, "SE") || streq(param, "3")) 
			dir = 3;
		else if (streq(param, "W") || streq(param, "4")) 
			dir = 4;
		else if (streq(param, "E") || streq(param, "6")) 
			dir = 6;
		else if (streq(param, "NW") || streq(param, "7")) 
			dir = 7;
		else if (streq(param, "N") || streq(param, "8")) 
			dir = 8;
		else if (streq(param, "NE") || streq(param, "9")) 
			dir = 9;		
		else
			return luaL_error(L, "%s is not a valid direction", param);

	} else {
		return luaL_error(L, "Direction command requires a direction");
	}

	cmdq_push(code);
	cmd_set_arg_direction(cmdq_peek(), 0, dir);

	return 0;
}

int lua_cmd_walk(lua_State *L) 
{
	return push_direction_cmd(L, CMD_WALK);
}

int lua_cmd_run(lua_State *L)
{
	return push_direction_cmd(L, CMD_RUN);
}

int lua_cmd_jump(lua_State *L)
{
	return push_direction_cmd(L, CMD_JUMP);
}

int lua_cmd_open(lua_State *L)
{
	return push_direction_cmd(L, CMD_OPEN);
}

int lua_cmd_close(lua_State *L)
{
	return push_direction_cmd(L, CMD_CLOSE);
}

int lua_cmd_tunnel(lua_State *L)
{
	return push_direction_cmd(L, CMD_TUNNEL);
}

int lua_cmd_disarm(lua_State *L)
{	
	return push_direction_cmd(L, CMD_DISARM);
}

int lua_cmd_alter(lua_State *L)
{	
	return push_direction_cmd(L, CMD_ALTER);
}


luaL_Reg lua_cmd_table[] = {
	{ "walk", lua_cmd_walk },
	{ "run", lua_cmd_run },
	{ "jump", lua_cmd_jump },
	{ "open", lua_cmd_open },
	{ "close", lua_cmd_close },
	{ "tunnel", lua_cmd_tunnel },
	{ "disarm", lua_cmd_disarm },
	{ "alter", lua_cmd_alter },
	{ NULL, NULL }
};


void lua_cmd_init(void)
{
	luaL_newlib(L, lua_cmd_table);
	lua_setglobal(L, "cmd");
}
