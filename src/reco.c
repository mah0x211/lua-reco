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

#ifndef LUA_OK
# define LUA_OK 0
#endif

#define MODULE_MT "reco"

// helper macros for lua_State
static inline int getmodule_metatable(lua_State *L)
{
#if LUA_VERSION_NUM >= 503
    return luaL_getmetatable(L, MODULE_MT);
#else
    luaL_getmetatable(L, MODULE_MT);
    return lua_type(L, -1) != LUA_TNIL;
#endif
}

static inline void checkself(lua_State *L)
{
    int top = lua_gettop(L);
    int t   = lua_type(L, 1);

    if (!lua_getmetatable(L, 1) || !getmodule_metatable(L) ||
        !lua_rawequal(L, -1, -2)) {
        char buf[255] = {0};
        snprintf(buf, 255, MODULE_MT " expected, got %s", lua_typename(L, t));
        luaL_argerror(L, 1, buf);
    }
    lua_settop(L, top);
}

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

static inline int rawget(lua_State *L, int idx, const char *k)
{
    lua_pushstring(L, k);
    lua_rawget(L, idx);
    return lua_type(L, -1);
}

static inline lua_State *rawget_thread(lua_State *L, int idx, const char *k,
                                       int allow_nil)
{
    int t         = rawget(L, idx, k);
    lua_State *th = NULL;

    switch (t) {
    case LUA_TTHREAD:
        th = lua_tothread(L, -1);
    case LUA_TNIL:
        if (th || allow_nil) {
            lua_pop(L, 1);
            return th;
        }

    default:
        luaL_error(L, "invalid value of '%s' field: %s expected, got %s", k,
                   lua_typename(L, LUA_TTHREAD), lua_typename(L, t));
        return NULL;
    }
}

static inline void rawget_function(lua_State *L, int idx, const char *k)
{
    int t = rawget(L, idx, k);
    if (t != LUA_TFUNCTION) {
        luaL_error(L, "invalid value of '%s' field: %s expected, got %s", k,
                   lua_typename(L, LUA_TFUNCTION), lua_typename(L, t));
    }
}

static inline int traceback(lua_State *L, lua_State *th)
{
#if LUA_VERSION_NUM >= 502
    // push thread stack trace to dst state
    luaL_traceback(L, th, lua_tostring(th, -1), 0);
    return 1;

#else
    int top = lua_gettop(th);

    // get debug.traceback function
    lua_pushliteral(th, "debug");
    lua_rawget(th, LUA_GLOBALSINDEX);
    if (lua_type(th, -1) == LUA_TTABLE) {
        lua_pushliteral(th, "traceback");
        lua_rawget(th, -2);
        if (lua_type(th, -1) == LUA_TFUNCTION) {
            // call debug.traceback function
            lua_pushvalue(th, top);
            if (lua_pcall(th, 1, 1, 0) == 0) {
                lua_xmove(th, L, 1);
                return 1;
            }
        }
    }

    lua_settop(th, top);
    lua_xmove(th, L, 1);
    return 1;

#endif
}

static int call_lua(lua_State *L)
{
    checkself(L);
    int argc       = lua_gettop(L) - 1;
    lua_State *th  = rawget_thread(L, 1, "th", 1);
    lua_State *res = rawget_thread(L, 1, "res", 0);
    int rc         = 0;
#if LUA_VERSION_NUM >= 504
    int nres = 0;
#endif

    // clear res thread
    lua_settop(res, 0);

    // should create new thread
    if (!th) {
CREATE_NEWTHREAD:
        // retain thread
        lua_pushliteral(L, "th");
        th = lua_newthread(L);
        lua_rawset(L, 1);
        goto SET_ENTRYFN;
    }

    // get current status
    switch (lua_status(th)) {
    case LUA_OK:
SET_ENTRYFN:
        // push function and arguments
        rawget_function(L, 1, "fn");
        lua_xmove(L, th, 1);
        lua_xmove(L, th, argc);
        break;

    case LUA_YIELD:
        // push arguments if yield
        lua_xmove(L, th, argc);
        break;

    default:
        goto CREATE_NEWTHREAD;
    }

    // run thread
#if LUA_VERSION_NUM >= 504
    rc = lua_resume(th, L, argc, &nres);
#elif LUA_VERSION_NUM >= 502
    rc = lua_resume(th, L, argc);
#else
    rc = lua_resume(th, argc);
#endif

    switch (rc) {
    case LUA_OK:
        // move the return values to res thread
#if LUA_VERSION_NUM >= 504
        lua_xmove(th, res, nres);
#else
        lua_xmove(th, res, lua_gettop(th));
#endif
        lua_settop(L, 0);
        // return done=true
        lua_pushboolean(L, 1);
        lua_pushinteger(L, rc);
        return 2;

    case LUA_YIELD:
        // move the return values to res thread
#if LUA_VERSION_NUM >= 504
        lua_xmove(th, res, nres);
#else
        lua_xmove(th, res, lua_gettop(th));
#endif
        // return done=false
        lua_settop(L, 0);
        lua_pushboolean(L, 0);
        lua_pushinteger(L, rc);
        return 2;

    // got error
    // LUA_ERRMEM:
    // LUA_ERRERR:
    // LUA_ERRSYNTAX:
    // LUA_ERRRUN:
    default:
        // push stacktrace to res thread
        traceback(res, th);

        // remove unrunnable thread
        lua_pushliteral(L, "th");
        lua_pushnil(L);
        lua_rawset(L, 1);

        // return done=true, error_code
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        lua_pushinteger(L, rc);
        return 2;
    }
}

static int results_lua(lua_State *L)
{
    checkself(L);
    lua_State *res = rawget_thread(L, 1, "res", 0);
    int nres       = lua_gettop(res);

    if (nres) {
        lua_xmove(res, L, nres);
    }

    return nres;
}

static int reset_lua(lua_State *L)
{
    checkself(L);

    // replace function
    if (lua_gettop(L) > 1) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_settop(L, 2);
        lua_pushliteral(L, "fn");
        lua_pushvalue(L, 2);
        lua_rawset(L, 1);
    }
    // create new execution thread
    lua_pushliteral(L, "th");
    lua_newthread(L);
    lua_rawset(L, 1);

    return 0;
}

static int tostring_lua(lua_State *L)
{
    checkself(L);
    lua_pushfstring(L, MODULE_MT ": %p", lua_topointer(L, 1));
    return 1;
}

static int new_lua(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);

    lua_createtable(L, 0, 3);
    // set function
    lua_pushliteral(L, "fn");
    lua_pushvalue(L, 1);
    lua_rawset(L, -3);
    // create new execution thread
    lua_pushliteral(L, "th");
    lua_newthread(L);
    lua_rawset(L, -3);
    // response thread
    lua_pushliteral(L, "res");
    lua_newthread(L);
    lua_rawset(L, -3);

    // set metatable
    luaL_getmetatable(L, MODULE_MT);
    lua_setmetatable(L, -2);

    return 1;
}

LUALIB_API int luaopen_reco(lua_State *L)
{
    // create table __metatable
    if (luaL_newmetatable(L, MODULE_MT)) {
        struct luaL_Reg mmethod[] = {
            {"__call",     call_lua    },
            {"__tostring", tostring_lua},
            {NULL,         NULL        }
        };
        struct luaL_Reg method[] = {
            {"reset",   reset_lua  },
            {"results", results_lua},
            {NULL,      NULL       }
        };
        struct luaL_Reg *ptr = mmethod;

        // metamethods
        while (ptr->name) {
            rawset_func(L, ptr->name, ptr->func);
            ptr++;
        }
        lua_pushliteral(L, "__index");
        lua_createtable(L, 0, 1);
        ptr = method;
        while (ptr->name) {
            rawset_func(L, ptr->name, ptr->func);
            ptr++;
        }
        lua_rawset(L, -3);
        lua_pop(L, 1);
    }

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
#ifdef LUA_ERRFILE
    rawset_int(L, "ERRFILE", LUA_ERRFILE);
#endif

    return 1;
}
