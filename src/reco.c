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
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
// lua
#include <lua.h>
#include <lauxlib.h>


// memory alloc/dealloc
#define pnalloc(n,t)    (t*)malloc( (n) * sizeof(t) )
#define pdealloc(p)     free((void*)p)

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


typedef struct {
    lua_State *L;
    int status;
    int narg;
    int *args;
} reco_t;


#define MODULE_MT   "reco"


static int call_lua( lua_State *L )
{
    reco_t *c = (reco_t*)luaL_checkudata( L, 1, MODULE_MT );
    int argc = lua_gettop( L );
    lua_State *th = c->L;
    int *args = c->args;
    int narg = c->narg;
    int status = c->status;
    int i = 0;
    
    // re-create thread
    if( !th )
    {
        if( !( th = lua_newthread( L ) ) ){
            lua_pushinteger( L, LUA_ERRMEM );
            lua_pushstring( L, strerror( errno ) );
            return 2;
        }
        c->L = th;
    }

    
    if( status == LUA_YIELD ){
        i = 1;
        narg -= 1;
    }
    
    // push func and arguments
    for(; i < narg; i++ ){
        lstate_pushref( th, args[i] );
    }
    // move args to th
    lua_xmove( L, th, argc );
    narg = narg - 1 + argc;
    
    // run on coroutine
#if LUA_VERSION_NUM >= 502
    status = lua_resume( th, L, narg );
#else
    status = lua_resume( th, narg );
#endif
    lua_pushinteger( L, status );
    switch( status )
    {
        case 0:
        case LUA_YIELD:
            lua_xmove( th, L, lua_gettop( th ) );
        break;
        
        // LUA_ERRMEM:
        // LUA_ERRERR:
        // LUA_ERRSYNTAX:
        // LUA_ERRRUN:
        default:
            lua_xmove( th, L, 1 );
            // re-create thread
            if( !( c->L = lua_newthread( L ) ) ){
                lua_pushstring( L, strerror( errno ) );
            }
        break;
    }
    
    // update status
    c->status = status;
    
    return 0;
}


static int len_lua( lua_State *L )
{
    reco_t *c = (reco_t*)luaL_checkudata( L, 1, MODULE_MT );

    lua_pushinteger( L, c->narg );
    
    return 1;
}


static int tostring_lua( lua_State *L )
{
    lua_pushfstring( L, MODULE_MT ": %p", luaL_checkudata( L, 1, MODULE_MT ) );
    
    return 1;
}


static int gc_lua( lua_State *L )
{
    reco_t *c = (reco_t*)lua_touserdata( L, 1 );
    int i = 0;
    
    for(; i < c->narg; i++ ){
        lstate_unref( L, c->args[i] );
    }
    pdealloc( c->args );
    
    return 0;
}


static int new_lua( lua_State *L )
{
    int narg = lua_gettop( L );
    reco_t *c = lua_newuserdata( L, sizeof( reco_t ) );
    int *args = NULL;
    
    // check fisrt argument
    luaL_checktype( L, 1, LUA_TFUNCTION );
    
    if( c && 
        ( c->L = lua_newthread( L ) ) && 
        ( args = pnalloc( narg, int ) ) )
    {
        int i = narg;
        
        // retain args
        for(; i > 0; i-- ){
            args[i - 1] = lstate_ref( L );
        }
        
        c->narg = narg;
        c->args = args;
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
        { "__len", len_lua },
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
    
    return 1;
}


