/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <haka/lua/object.h>
#include <haka/config.h>
#include <haka/types.h>
#include <haka/compiler.h>
#include <haka/error.h>
#include <haka/log.h>
#include <haka/lua/object.h>
#include <haka/lua/state.h>
#include <haka/lua/luautils.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#ifdef HAKA_DEBUG
#define OBJECT_TABLE "__c_obj_ref"
#endif

extern void lua_clear_swigdata(lua_State *L, int idx);

#ifdef HAKA_FFI
static void lua_clear_cdata(lua_State *L, int idx)
{
	assert(lua_iscdata(L, idx));
	void **cdata = (void **)lua_topointer(L, idx);
	*cdata = NULL;
}
#endif

void lua_object_initialize(lua_State *L)
{
#ifdef HAKA_DEBUG
	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, OBJECT_TABLE);
#endif
}

const struct lua_object lua_object_init = LUA_OBJECT_INIT;

void lua_object_release(struct lua_object *obj)
{
	assert(obj);

	if (obj->ref.state && lua_state_isvalid(obj->ref.state)) {
		lua_State *L = obj->ref.state->L;

		LUA_STACK_MARK(L);

		lua_ref_push(L, &obj->ref);
#ifdef HAKA_FFI
		if (lua_iscdata(L, -1)) {
			lua_clear_cdata(L, -1);
		} else {
			lua_clear_swigdata(L, -1);
		}
#else
		lua_clear_swigdata(L, -1);
#endif
		lua_pop(L, 1);

		lua_ref_clear(L, &obj->ref);

#ifdef HAKA_DEBUG
		lua_getfield(L, LUA_REGISTRYINDEX, OBJECT_TABLE);

		/* Erase object address */
		lua_pushlightuserdata(L, obj);
		lua_pushnil(L);
		lua_rawset(L, -3);

		lua_pop(L, 1); /* pop OBJECT_TABLE */
#endif
		LUA_STACK_CHECK(L, 0);

		*obj = lua_object_init;
	}
}

void lua_object_register(lua_State *L, struct lua_object *obj, int index, bool lua_own)
{
	LUA_STACK_MARK(L);

	if (index < 0) {
		index = lua_gettop(L)+index+1;
	}

#ifdef HAKA_DEBUG
	lua_getfield(L, LUA_REGISTRYINDEX, OBJECT_TABLE);

	/* Ensure we are not registering the object twice */
	lua_pushlightuserdata(L, obj);
	lua_rawget(L, -2);
	assert(lua_rawequal(L, -1, index) || lua_isnil(L, -1));
	lua_pop(L, 1);

	/* Save object address so we can do the above assert later */
	lua_pushlightuserdata(L, obj);
	lua_pushvalue(L, index);
	lua_rawset(L, -3);

	lua_pop(L, 1); /* pop OBJECT_TABLE */
#endif

	if (!lua_own) {
		if (!lua_ref_isvalid(&obj->ref)) {
			lua_ref_get(L, &obj->ref, index, false);
		}

		assert(obj->ref.state == lua_state_get(L));
	}


	LUA_STACK_CHECK(L, 0);
}

bool lua_object_push(lua_State *L, struct lua_object *obj, bool lua_own)
{
	LUA_STACK_MARK(L);

	if (!obj) {
		lua_pushnil(L);
		return true;
	}

	if (lua_ref_isvalid(&obj->ref)) {
		lua_ref_push(L, &obj->ref);
		assert(!lua_isnil(L, -1));

		if (lua_own) {
			lua_ref_clear(L, &obj->ref);
		}

		LUA_STACK_CHECK(L, 1);
		return true;
	}
	else {
		return false;
	}
}
