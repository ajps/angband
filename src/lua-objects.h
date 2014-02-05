
#ifndef LUA_OBJECTS_H
#define LUA_OBJECTS_H

struct object_udata {
	int idx;  /* Arguably over-simple representation of an object to start */
};

void lua_objects_init(void);


#endif /* LUA_OBJECTS_H */
