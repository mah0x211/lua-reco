/**
 *  Copyright (C) 2015 Masatoshi Fukunaga
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to
 *  deal in the Software without restriction, including without limitation the
 *  rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 *  sell copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 *  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 *  IN THE SOFTWARE.
 *
 *  reco.c
 *  lua-reco
 *  Created by Masatoshi Teruya on 15/08/31.
 *
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// lua
#include <lauxlib.h>
#include <lua.h>

// helper macros for lua_State
static inline void rawset_func(lua_State *L, const char *k, lua_CFunction v)
{
    lua_pushstring(L, k);
    lua_pushcfunction(L, v);
    lua_rawset(L, -3);
}

static inline void rawset_int(lua_State *L, const char *k, int v)
{
    lua_pushstring(L, k);
    lua_pushinteger(L, v);
    lua_rawset(L, -3);
}

#define MODULE_MT "reco"

static int call_lua(lua_State *L)
{
    int argc           = lua_gettop(L) - 1;
    lua_State *th      = NULL;
    lua_Integer status = 0;
    int rv             = 2;
    int i;
#if LUA_VERSION_NUM >= 504
    int nres = 0;
#endif

    lua_getfield(L, 1, "co");
    // should create new thread
    if (!lua_isthread(L, -1)) {
        lua_pop(L, 1);

CREATE_NEWTHREAD:
        th = lua_newthread(L);
        // retain thread
        lua_setfield(L, 1, "co");
        goto SET_ENTRYFN;
    } else {
        th = lua_tothread(L, -1);
        lua_pop(L, 1);

        // get current status
        switch (lua_status(th)) {
        // push function and arguments
        case 0:
SET_ENTRYFN:
            lua_getfield(L, 1, "fn");
            lua_xmove(L, th, 1);
            lua_xmove(L, th, argc);
            break;

        // push arguments if yield
        case LUA_YIELD:
            lua_xmove(L, th, argc);
            break;

        default:
            goto CREATE_NEWTHREAD;
        }
    }

    // run thread
#if LUA_VERSION_NUM >= 504
    status = lua_resume(th, L, argc, &nres);
#elif LUA_VERSION_NUM >= 502
    status = lua_resume(th, L, argc);
#else
    status = lua_resume(th, argc);
#endif
    lua_pushinteger(L, status);
    lua_setfield(L, 1, "status");

    switch (status) {
    case 0:
    case LUA_YIELD:
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        // move the thread return values to caller
#if LUA_VERSION_NUM >= 504
        lua_xmove(th, L, nres);
        return 1 + nres;
#else
        argc = lua_gettop(th);
        lua_xmove(th, L, argc);
        return 1 + argc;
#endif

    // got error
    // LUA_ERRMEM:
    // LUA_ERRERR:
    // LUA_ERRSYNTAX:
    // LUA_ERRRUN:
    default:
        // remove current thread
        lua_pushnil(L);
        lua_setfield(L, 1, "co");
        lua_settop(L, 0);

        lua_pushboolean(L, 0);
        // move an error message to caller
        lua_xmove(th, L, 1);

        // traceback
#if LUA_VERSION_NUM >= 502
        // get stack trace
        luaL_traceback(L, th, NULL, 0);
        rv++;
#else
        // get debug module
        lua_getfield(th, LUA_GLOBALSINDEX, "debug");
        if (lua_istable(th, -1)) {
            // get traceback function
            lua_getfield(th, -1, "traceback");
            if (lua_isfunction(th, -1)) {
                // call
                lua_call(th, 0, 1);
                // append to message field
                lua_xmove(th, L, 1);
                rv++;
            }
        }
#endif

        return rv;
    }
}

static int new_lua(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);

    lua_createtable(L, 0, 3);
    if (lua_istable(L, -1)) {
        lua_pushvalue(L, 1);
        lua_setfield(L, -2, "fn");

        // alloc thread
        lua_newthread(L);
        lua_setfield(L, -2, "co");
        // status
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "status");
        // set metatable
        luaL_getmetatable(L, MODULE_MT);
        lua_setmetatable(L, -2);

        return 1;
    }

    // got error
    lua_pushnil(L);
    lua_pushstring(L, strerror(errno));

    return 2;
}

LUALIB_API int luaopen_reco(lua_State *L)
{
    struct luaL_Reg mmethod[] = {
        {"__call", call_lua},
        {NULL,     NULL    }
    };
    struct luaL_Reg *ptr = mmethod;

    // create table __metatable
    luaL_newmetatable(L, MODULE_MT);
    // metamethods
    while (ptr->name) {
        rawset_func(L, ptr->name, ptr->func);
        ptr++;
    }
    lua_pop(L, 1);

    // add new function
    lua_newtable(L);
    rawset_func(L, "new", new_lua);
    // add status code
    rawset_int(L, "OK", 0);
    rawset_int(L, "YIELD", LUA_YIELD);
    rawset_int(L, "ERRMEM", LUA_ERRMEM);
    rawset_int(L, "ERRERR", LUA_ERRERR);
    rawset_int(L, "ERRSYNTAX", LUA_ERRSYNTAX);
    rawset_int(L, "ERRRUN", LUA_ERRRUN);

    return 1;
}
