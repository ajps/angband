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
#include "cmd-core.h"
#include "lua-objects.h"

/** 
 * Handles extracting a direction from the lua stack.
 * 
 * Like other luaL_check* functions, this is designed to be used
 * for parameters, and throws an error  if it doesn't find a valid direction.
 */
static int luaL_checkdirection(lua_State *L, int index, bool allow_target)
{
	const char *param;
	int dir;
	
	/* Check there is a value at the given point in the stack */
	luaL_checkany(L, index);

	/* We'll treat a direction number as a string for simplicity. */
	param = lua_tostring(L, index);

	if (param) {
		if (streq(param, "SW") || streq(param, "1")) 
			dir = DIR_SW;
		else if (streq(param, "S") || streq(param, "2")) 
			dir = DIR_S;
		else if (streq(param, "SE") || streq(param, "3")) 
			dir = DIR_SE;
		else if (streq(param, "W") || streq(param, "4")) 
			dir = DIR_W;
		else if (streq(param, "E") || streq(param, "6")) 
			dir = DIR_E;
		else if (streq(param, "NW") || streq(param, "7")) 
			dir = DIR_NW;
		else if (streq(param, "N") || streq(param, "8")) 
			dir = DIR_N;
		else if (streq(param, "NE") || streq(param, "9")) 
			dir = DIR_NE;		
		else if (allow_target && (streq(param, "*") || streq(param, "5")) )
			dir = DIR_TARGET;		
		else
			return luaL_error(L, "%s is not a valid direction", param);

	} else {
		return luaL_error(L, "Direction command requires a direction");
	}

	return dir;
}

/**
 * Pushes a command with a direction to the game
 */
static int push_direction_cmd(lua_State *L, cmd_code code)
{
	int dir;

	dir = luaL_checkdirection(L, 1, FALSE);
	cmdq_push(code);
	cmd_set_arg_direction(cmdq_peek(), "direction", dir);

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

int lua_cmd_upstairs(lua_State *L)
{	
	cmdq_push(CMD_GO_UP);
	return 0;
}

int lua_cmd_downstairs(lua_State *L)
{	
	cmdq_push(CMD_GO_DOWN);
	return 0;
}

int lua_cmd_search(lua_State *L)
{	
	cmdq_push(CMD_SEARCH);
	return 0;
}

int lua_cmd_hold(lua_State *L)
{	
	cmdq_push(CMD_HOLD);
	return 0;
}

int lua_cmd_save(lua_State *L)
{	
	cmdq_push(CMD_SAVE);
	return 0;
}

/*
 * cmd.run_to({x = <x>, y = <y>})
 */
int lua_cmd_run_to(lua_State *L)
{	
	int x, y;

	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "x");
	if (lua_isnumber(L, -1)) {
		x = lua_tonumber(L, -1);
		lua_pop(L, 1);
	} else 	{
		return luaL_error(L, "No x co-ordinate supplied in table");
	}

	lua_getfield(L, 1, "y");
	if (lua_isnumber(L, -1)) {
		y = lua_tonumber(L, -1);
		lua_pop(L, 1);
	} else 	{
		return luaL_error(L, "No x co-ordinate supplied in table");
	}

	cmdq_push(CMD_PATHFIND);
	cmd_set_arg_point(cmdq_peek(), "point", x, y);
	return 0;
}


int lua_cmd_use(lua_State *L)
{	
	struct object_udata *object;
	int target = DIR_UNKNOWN;

	object = luaL_checkudata(L, 1, "object");

	/* Currently, we'll mimick the shortcomings of game-cmd.c exactly and
	   not handle Identify scrolls, rods, etc */
	if (lua_gettop(L) > 1) {
		target = luaL_checkdirection(L, 1, TRUE);
	}

	cmdq_push(CMD_USE);
	cmd_set_arg_item(cmdq_peek(), "item", object->idx);
	cmd_set_arg_target(cmdq_peek(), "target", target);

	return 0;
}

/** 
 * cmd.drop(object [, number])
 */
int lua_cmd_drop(lua_State *L)
{	
	struct object_udata *object;
	int number = 1;

	object = luaL_checkudata(L, 1, "object");

	/* number to drop is optional, defaults to 1 */
	if (lua_gettop(L) > 1)
		number = luaL_checknumber(L, 2);

	cmdq_push(CMD_DROP);
	cmd_set_arg_item(cmdq_peek(), "item", object->idx);
	cmd_set_arg_number(cmdq_peek(), "quantity", number);

	return 0;
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
	{ "upstairs", lua_cmd_upstairs },
	{ "downstairs", lua_cmd_downstairs },
	{ "search", lua_cmd_search },
	{ "hold", lua_cmd_hold },
	{ "save", lua_cmd_save },
	{ "run_to", lua_cmd_run_to },
	{ "use", lua_cmd_use },
	{ "drop", lua_cmd_drop },
	{ NULL, NULL }
};


void lua_cmd_init(void)
{
	luaL_newlib(L, lua_cmd_table);
	lua_setglobal(L, "cmd");
}
