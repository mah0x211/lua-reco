lua-reco
===

[![test](https://github.com/mah0x211/lua-reco/actions/workflows/test.yml/badge.svg)](https://github.com/mah0x211/lua-reco/actions/workflows/test.yml)
[![codecov](https://codecov.io/gh/mah0x211/lua-reco/branch/master/graph/badge.svg)](https://codecov.io/gh/mah0x211/lua-reco)

reusable coroutine module.


## Installation

```
luarocks install reco
```

## Status Code Constants

- reco.OK
- reco.YIELD
- reco.ERRMEM
- reco.ERRERR
- reco.ERRSYNTAX
- reco.ERRRUN
- reco.ERRFILE (`if defined`)


## co, err = reco.new( fn )

create a new reusable coroutine object.

**Parameters**

- `fn:function`: function that run in coroutine.

**Returns**

- `co:reco`: table wrapped in the reco metatable, or nil.
- `err:str`: error string. 


## co:reset( [fn] )

replaces the function with the passed function if it is not nil. and recreates the coroutine.

**Parameters**

- `fn:function`: function that run in coroutine.


## done, status = co( ... )

starts or continues the execution of coroutine.

**Parameters**

- `...:any`: you can pass any arguments.

**Returns**

- `done:boolean`: `true` on finished.
- `status:integer`: status code.


### info = co:getinfo( [level], [what] )

get the debug information about `co`.

**Parameters**

- `level:integer`: stack level that same as `debug.getinfo` function. default is `1`.
- `what:string`: same as `debug.getinfo` function. default is `'nSlutr'`.

**Returns**

- `info:table`: debug information or `nil`.


## ... = co:results()

get the return values. these values will be removed from `co`.

**Returns**

- `...:any`: function return values, or error string and traceback.


## Usage

```lua
local unpack = unpack or table.unpack
local reco = require('reco')

local function myfn(...)
    print('args', ...)
    coroutine.yield('a', 'b', 'c')
    print 'ok'
    return ...
end

local args = {
    'x',
    'y',
    'z',
}

local function run_coroutine()
    print('coroutine')
    local co = coroutine.wrap(myfn)
    print('run', co(unpack(args)))
    print('run', co(unpack(args)))
    -- cannot resume dead coroutine
    local _, err = pcall(function()
        print('run', co(unpack(args)))
    end)
    print(err)
end

local function run_reco()
    print('reco')
    local co = reco.new(myfn)
    print('run', co(unpack(args)))
    print('res:', co:results())
    print('run', co(unpack(args)))
    print('res:', co:results())
    print('run', co(unpack(args)))
end

local function run_reco_error()
    local myfnerr = function(...)
        print('it throw an error')
        local a
        a = a + {
            ...,
        }
    end

    print('reco with error function')
    local co = reco.new(myfnerr)
    local done, rc = co()
    print(done, rc, rc == reco.ERRRUN)
    print(co:results())
end

run_coroutine()
print('')
run_reco()
print('')
run_reco_error()
```
