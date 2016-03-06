/*
 *  Copyright (C) 2015 Masatoshi Teruya
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 *  THE SOFTWARE.
 *
 *  reco.c
 *  lua-reco
 *  Created by Masatoshi Teruya on 15/08/31.
 *
 */


#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
// lua
#include <lua.h>
#include <lauxlib.h>


// helper macros for lua_State
#define lstate_fn2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushcfunction(L,v); \
    lua_rawset(L,-3); \
}while(0)


#define lstate_num2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushinteger(L,v); \
    lua_rawset(L,-3); \
}while(0)


#define MODULE_MT   "reco"


static int call_lua( lua_State *L )
{
    int argc = lua_gettop( L ) - 1;
    lua_State *th = NULL;
    lua_Integer status = 0;
    int rv = 2;
    int i;

    lua_getfield( L, 1, "status" );
    status = luaL_optinteger( L, -1, 0 );
    lua_pop( L, 1 );

    lua_getfield( L, 1, "co" );
    // should create new thread
    if( !lua_isthread( L, -1 ) )
    {
        // failed to create new thread
        if( !( th = lua_newthread( L ) ) ){
            lua_pushboolean( L, 0 );
            lua_pushstring( L, strerror( errno ) );
            return 2;
        }
        // retain thread
        lua_setfield( L, 1, "co" );

        lua_getfield( L, 1, "fn" );
        lua_xmove( L, th, 1 );
        lua_xmove( L, th, argc );
    }
    else
    {
        th = lua_tothread( L, -1 );
        lua_pop( L, 1 );
        // skip function if yield
        if( status == LUA_YIELD ){
            lua_xmove( L, th, argc );
        }
        else {
            lua_getfield( L, 1, "fn" );
            lua_xmove( L, th, 1 );
            lua_xmove( L, th, argc );
        }
    }

    // run thread
#if LUA_VERSION_NUM >= 502
    status = lua_resume( th, L, argc );
#else
    status = lua_resume( th, argc );
#endif
    lua_pushinteger( L, status );
    lua_setfield( L, 1, "status" );

    switch( status )
    {
        case 0:
        case LUA_YIELD:
            lua_settop( L, 0 );
            lua_pushboolean( L, 1 );
            argc = lua_gettop( th );
            lua_xmove( th, L, argc );
            return 1 + argc;
        
        // got error
        // LUA_ERRMEM:
        // LUA_ERRERR:
        // LUA_ERRSYNTAX:
        // LUA_ERRRUN:
        default:
            lua_pushnil( L );
            lua_setfield( L, 1, "co" );
            lua_settop( L, 0 );
            lua_pushboolean( L, 0 );
            // error message
            lua_xmove( th, L, 1 );
            // traceback
#if LUA_VERSION_NUM >= 502
            // get stack trace
            luaL_traceback( L, th, NULL, 0 );
            rv++;
#else
            // get debug module
            lua_getfield( th, LUA_GLOBALSINDEX, "debug" );
            if( lua_istable( th, -1 ) )
            {
                // get traceback function
                lua_getfield( th, -1, "traceback" );
                if( lua_isfunction( th, -1 ) ){
                    // call
                    lua_call( th, 0, 1 );
                    // append to message field
                    lua_xmove( th, L, 1 );
                    rv++;
                }
            }
#endif

            return rv;
    }
}


static int new_lua( lua_State *L )
{
    luaL_checktype( L, 1, LUA_TFUNCTION );
    lua_settop( L, 1 );

    lua_createtable( L, 0, 3 );
    if( lua_istable( L, -1 ) )
    {
        lua_pushvalue( L, 1 );
        lua_setfield( L, -2, "fn" );

        // alloc thread
        if( lua_newthread( L ) ){
            lua_setfield( L, -2, "co" );
            // status
            lua_pushinteger( L, 0 );
            lua_setfield( L, -2, "status" );
            // set metatable
            luaL_getmetatable( L, MODULE_MT );
            lua_setmetatable( L, -2 );

            return 1;
        }
        lua_settop( L, 0 );
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


LUALIB_API int luaopen_reco( lua_State *L )
{
    struct luaL_Reg mmethod[] = {
        { "__call", call_lua },
        { NULL, NULL }
    };
    struct luaL_Reg *ptr = mmethod;
    
    // create table __metatable
    luaL_newmetatable( L, MODULE_MT );
    // metamethods
    while( ptr->name ){
        lstate_fn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    lua_pop( L, 1 );
    
    // add new function
    lua_newtable( L );
    lstate_fn2tbl( L, "new", new_lua );
    // add status code
    lstate_num2tbl( L, "OK", 0 );
    lstate_num2tbl( L, "YIELD", LUA_YIELD );
    lstate_num2tbl( L, "ERRMEM", LUA_ERRMEM );
    lstate_num2tbl( L, "ERRERR", LUA_ERRERR );
    lstate_num2tbl( L, "ERRSYNTAX", LUA_ERRSYNTAX );
    lstate_num2tbl( L, "ERRRUN", LUA_ERRRUN );

    return 1;
}


