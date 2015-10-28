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
#define lstate_ref(L) \
    luaL_ref( L, LUA_REGISTRYINDEX )

#define lstate_pushref(L,ref) \
    lua_rawgeti( L, LUA_REGISTRYINDEX, ref )

#define lstate_unref(L,ref) \
    (luaL_unref( L, LUA_REGISTRYINDEX, ref ),LUA_NOREF)

#define lstate_fn2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushcfunction(L,v); \
    lua_rawset(L,-3); \
}while(0)


#define lstate_str2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushstring(L,v); \
    lua_rawset(L,-3); \
}while(0)


#define lstate_num2tbl(L,k,v) do{ \
    lua_pushstring(L,k); \
    lua_pushinteger(L,v); \
    lua_rawset(L,-3); \
}while(0)


typedef struct {
    lua_State *L;
    int status;
    int narg;
    int *args;
} reco_t;


#define MODULE_MT   "reco"


static int status_lua( lua_State *L )
{
    reco_t *c = (reco_t*)luaL_checkudata( L, 1, MODULE_MT );
    
    lua_pushinteger( L, c->status );
    
    return 1;
}


static int getargs_lua( lua_State *L )
{
    reco_t *c = (reco_t*)luaL_checkudata( L, 1, MODULE_MT );
    int *args = c->args;
    int narg = c->narg;
    int idx = 1;
    
    // push all curried arguments
    if( lua_gettop( L ) == 1 )
    {
        for(; idx < narg; idx++ ){
            lstate_pushref( L, args[idx] );
        }
        
        return narg - 1;
    }
    
    // push an curried argument
    idx = (int)luaL_checkinteger( L, 2 );
    if( idx > 0 && idx < narg ){
        lstate_pushref( L, args[idx] );
        return 1;
    }
    
    return 0;
}


static int call_lua( lua_State *L )
{
    reco_t *c = (reco_t*)luaL_checkudata( L, 1, MODULE_MT );
    int argc = lua_gettop( L ) - 1;
    lua_State *th = c->L;
    int *args = c->args;
    const int narg = c->narg;
    int status = c->status;
    int i = 0;
    int rv = 3;

    // should create new thread
    if( !th )
    {
        // failed to create new thread
        if( !( th = lua_newthread( L ) ) ){
            c->status = LUA_ERRMEM;
            lua_pushboolean( L, 0 );
            lua_pushstring( L, strerror( errno ) );
            return 2;
        }
        // retain thread
        args[narg] = lstate_ref( L );
        c->L = th;
    }
    // skip function if yield
    else {
        i += status == LUA_YIELD;
    }
    
    // push func and arguments
    for(; i < narg; i++ ){
        lstate_pushref( th, args[i] );
    }
    // move args to thread
    if( argc ){
        lua_xmove( L, th, argc );
    }
    
    // run thread
#if LUA_VERSION_NUM >= 502
    status = lua_resume( th, L, i - 1 + argc );
#else
    status = lua_resume( th, i - 1 + argc );
#endif
    // update status
    c->status = status;
    
    switch( status )
    {
        case 0:
        case LUA_YIELD:
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
            lua_pushboolean( L, 0 );
            // error message
            lua_xmove( th, L, 1 );
            lua_pushinteger( L, status );
            // traceback
#if LUA_VERSION_NUM >= 502
            // get stack trace
            luaL_traceback( L, th, lua_tostring( L, -1 ), 1 );
#else
            // get debug module
            lua_getfield( th, LUA_GLOBALSINDEX, "debug" );
            if( lua_istable( th, -1 ) != 0 )
            {
                // get traceback function
                lua_getfield( th, -1, "traceback" );
                if( lua_isfunction( th, -1 ) != 0 ){
                    // call
                    lua_call( th, 0, 1 );
                    // append to message field
                    lua_xmove( th, L, 1 );
                    rv++;
                }
            }
#endif
            // re-create thread
            if( ( c->L = lua_newthread( L ) ) ){
                // retain thread
                args[narg] = lstate_ref( L );
            }
            
            return rv;
    }
}


static int tostring_lua( lua_State *L )
{
    lua_pushfstring( L, MODULE_MT ": %p", luaL_checkudata( L, 1, MODULE_MT ) );
    
    return 1;
}


static int gc_lua( lua_State *L )
{
    reco_t *c = (reco_t*)lua_touserdata( L, 1 );
    int narg = c->narg + 1;
    int i = 0;
    
    // release references
    for(; i < narg; i++ ){
        lstate_unref( L, c->args[i] );
    }
    free( (void*)c->args );
    
    return 0;
}


static int new_lua( lua_State *L )
{
    int narg = lua_gettop( L );
    lua_State *th = NULL;
    reco_t *c = NULL;
    int *args = NULL;
    
    // check fisrt argument
    luaL_checktype( L, 1, LUA_TFUNCTION );
    // alloc
    if( ( th = lua_newthread( L ) ) &&
        ( c = lua_newuserdata( L, sizeof( reco_t ) ) ) &&
        // narg(fn + other arguments) + thread
        ( args = (int*)malloc( ( narg + 1 ) * sizeof( int ) ) ) )
    {
        // temporary retain userdata
        int ref = lstate_ref( L );
        int i = narg;
        
        // retain thread
        args[narg] = lstate_ref( L );
        // retain args
        for(; i > 0; i-- ){
            args[i - 1] = lstate_ref( L );
        }
        
        c->L = th;
        c->status = 0;
        c->narg = narg;
        c->args = args;
        // push and release userdata
        lstate_pushref( L, ref );
        lstate_unref( L, ref );
        // set metatable
        luaL_getmetatable( L, MODULE_MT );
        lua_setmetatable( L, -2 );
        
        return 1;
    }
    
    // got error
    lua_pushnil( L );
    lua_pushstring( L, strerror( errno ) );
    
    return 2;
}


LUALIB_API int luaopen_reco( lua_State *L )
{
    struct luaL_Reg mmethod[] = {
        { "__gc", gc_lua },
        { "__tostring", tostring_lua },
        { "__call", call_lua },
        { NULL, NULL }
    };
    struct luaL_Reg method[] = {
        { "getArgs", getargs_lua },
        { "status", status_lua },
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
    // methods
    ptr = method;
    lua_pushstring( L, "__index" );
    lua_newtable( L );
    while( ptr->name ){
        lstate_fn2tbl( L, ptr->name, ptr->func );
        ptr++;
    }
    lua_rawset( L, -3 );
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


