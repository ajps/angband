
#ifndef LUA_INIT_H
#define LUA_INIT_H

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

extern lua_State *L;

/* function tables */
void lua_cmd_init();

void lua_init(void);
void lua_cleanup(void);
void lua_execute(const char *line);


#endif /* LUA_INIT_H */
