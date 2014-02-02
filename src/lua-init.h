
#ifndef LUA_INIT_H
#define LUA_INIT_H

#include "lua.h"

extern lua_State *L;

void lua_init(void);
void lua_cleanup(void);

#endif /* LUA_INIT_H */
