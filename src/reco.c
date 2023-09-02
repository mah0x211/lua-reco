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

typedef struct {
    int ref_fn;
    int ref_th;
    int ref_res;
    lua_State *th;
    lua_State *res;
} reco_t;

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
    reco_t *r = luaL_checkudata(L, 1, MODULE_MT);
    int argc  = lua_gettop(L) - 1;
    int rc    = 0;
#if LUA_VERSION_NUM >= 504
    int nres = 0;
#endif

    // clear res thread
    lua_settop(r->res, 0);

    // should create new thread
    if (!r->th) {
CREATE_NEWTHREAD:
        // create new thread
        r->th     = lua_newthread(L);
        r->ref_th = luaL_ref(L, LUA_REGISTRYINDEX);
        goto SET_ENTRYFN;
    }

    // get current status
    switch (lua_status(r->th)) {
    case LUA_OK:
SET_ENTRYFN:
        // push function and arguments
        lua_settop(r->th, 0);
        lua_rawgeti(L, LUA_REGISTRYINDEX, r->ref_fn);
        lua_xmove(L, r->th, 1);
        lua_xmove(L, r->th, argc);
        break;

    case LUA_YIELD:
        // push arguments if yield
        lua_xmove(L, r->th, argc);
        break;

    default:
        goto CREATE_NEWTHREAD;
    }

    // run thread
#if LUA_VERSION_NUM >= 504
    rc = lua_resume(r->th, L, argc, &nres);
#elif LUA_VERSION_NUM >= 502
    rc = lua_resume(r->th, L, argc);
#else
    rc = lua_resume(r->th, argc);
#endif

    switch (rc) {
    case LUA_OK:
        // move the return values to res thread
#if LUA_VERSION_NUM >= 504
        lua_xmove(r->th, r->res, nres);
#else
        lua_xmove(r->th, r->res, lua_gettop(r->th));
#endif
        lua_settop(L, 0);
        // return done=true
        lua_pushboolean(L, 1);
        lua_pushinteger(L, rc);
        return 2;

    case LUA_YIELD:
        // move the return values to res thread
#if LUA_VERSION_NUM >= 504
        lua_xmove(r->th, r->res, nres);
#else
        lua_xmove(r->th, r->res, lua_gettop(r->th));
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
        traceback(r->res, r->th);

        // return done=true, error_code
        lua_settop(L, 0);
        lua_pushboolean(L, 1);
        lua_pushinteger(L, rc);
        return 2;
    }
}

static int results_lua(lua_State *L)
{
    reco_t *r = luaL_checkudata(L, 1, MODULE_MT);
    int nres  = lua_gettop(r->res);

    if (nres) {
        lua_xmove(r->res, L, nres);
    }

    return nres;
}

static int reset_lua(lua_State *L)
{
    reco_t *r = luaL_checkudata(L, 1, MODULE_MT);

    // replace function
    if (lua_gettop(L) > 1) {
        luaL_checktype(L, 2, LUA_TFUNCTION);
        lua_settop(L, 2);
        luaL_unref(L, LUA_REGISTRYINDEX, r->ref_fn);
        r->ref_fn = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    // create new execution thread
    luaL_unref(L, LUA_REGISTRYINDEX, r->ref_th);
    r->th     = lua_newthread(L);
    r->ref_th = luaL_ref(L, LUA_REGISTRYINDEX);

    return 0;
}

static int getinfo_lua(lua_State *L)
{
    reco_t *r         = luaL_checkudata(L, 1, MODULE_MT);
    lua_Integer level = luaL_optinteger(L, 2, 1);
    const char *what  = luaL_optstring(L, 3, "nSlutr");
    lua_Debug ar;

    // get info from execution thread
    if (lua_getstack(r->th, level, &ar)) {
#if LUA_VERSION_NUM >= 502
        lua_getinfo(r->th, what, &ar);
#else
        lua_getinfo(r->th, what, &ar);
#endif
        lua_settop(L, 0);
        // push all info to table
        lua_createtable(L, 0, 10);

#define push_string_field(L, field)                                            \
    do {                                                                       \
        if (ar.field) {                                                        \
            lua_pushstring(L, ar.field);                                       \
            lua_setfield(L, -2, #field);                                       \
        }                                                                      \
    } while (0)

#define push_int_field(L, field)                                               \
    do {                                                                       \
        lua_pushinteger(L, ar.field);                                          \
        lua_setfield(L, -2, #field);                                           \
    } while (0)

#define push_bool_field(L, field)                                              \
    do {                                                                       \
        lua_pushboolean(L, ar.field);                                          \
        lua_setfield(L, -2, #field);                                           \
    } while (0)

        push_string_field(L, name);
        push_string_field(L, namewhat);
        push_string_field(L, what);
        push_string_field(L, source);
        push_string_field(L, short_src);
        push_int_field(L, currentline);
        push_int_field(L, linedefined);
        push_int_field(L, lastlinedefined);
        push_int_field(L, nups);

#if LUA_VERSION_NUM >= 502
        push_int_field(L, nparams);
        push_bool_field(L, isvararg);
        push_bool_field(L, istailcall);
#endif

#if LUA_VERSION_NUM >= 504
        push_int_field(L, ftransfer);
        push_int_field(L, ntransfer);
#endif

#undef push_bool_field
#undef push_int_field
#undef push_string_field

        return 1;
    }

    return 0;
}

static int tostring_lua(lua_State *L)
{
    lua_pushfstring(L, MODULE_MT ": %p", lua_topointer(L, 1));
    return 1;
}

static int gc_lua(lua_State *L)
{
    reco_t *r = luaL_checkudata(L, 1, MODULE_MT);

    // unref function, execution thread and response thread
    luaL_unref(L, LUA_REGISTRYINDEX, r->ref_fn);
    luaL_unref(L, LUA_REGISTRYINDEX, r->ref_th);
    luaL_unref(L, LUA_REGISTRYINDEX, r->ref_res);

    return 0;
}

static int new_lua(lua_State *L)
{
    reco_t *r = NULL;

    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_settop(L, 1);

    r  = lua_newuserdata(L, sizeof(reco_t));
    *r = (reco_t){
        .ref_fn  = LUA_NOREF,
        .ref_th  = LUA_NOREF,
        .ref_res = LUA_NOREF,
    };
    luaL_getmetatable(L, MODULE_MT);
    lua_setmetatable(L, -2);
    lua_insert(L, 1);

    // set function
    r->ref_fn  = luaL_ref(L, LUA_REGISTRYINDEX);
    // create new execution thread
    r->th      = lua_newthread(L);
    r->ref_th  = luaL_ref(L, LUA_REGISTRYINDEX);
    // response thread
    r->res     = lua_newthread(L);
    r->ref_res = luaL_ref(L, LUA_REGISTRYINDEX);

    return 1;
}

LUALIB_API int luaopen_reco(lua_State *L)
{
    // create table __metatable
    if (luaL_newmetatable(L, MODULE_MT)) {
        struct luaL_Reg mmethod[] = {
            {"__gc",       gc_lua      },
            {"__tostring", tostring_lua},
            {"__call",     call_lua    },
            {NULL,         NULL        }
        };
        struct luaL_Reg method[] = {
            {"getinfo", getinfo_lua},
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
