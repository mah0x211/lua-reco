lua-reco
===

reusable coroutine module.

## Installation

```
luarocks install reco --from=http://mah0x211.github.io/rocks/
```

## Status Code Constants

- reco.OK
- reco.YIELD
- reco.ERRMEM
- reco.ERRERR
- reco.ERRSYNTAX
- reco.ERRRUN
    

## API

### co, err = reco.new( fn:function )

create a new reusable coroutine object.

**Parameters**

- `fn:function`: function that run in coroutine.


**Returns**

1. `co:table`: table wrapped in the reco metatable, or nil.
2. `err:str`: error string. 


**Properties**

- `co.status:int`: a last status code.


### ok, ... = co( ... )

run a reusable coroutine object.

**Parameters**

- `...`: you can pass any arguments.

**Returns**

1. `ok:boolean`: true on success, or false on failure.
2. function return values, or error string and traceback.



## Usage

```lua
local reco = require('reco');

local function myfn( ... )
    print( 'args', ... );
    coroutine.yield( 'a', 'b', 'c' );
    print 'ok';
    
    return ...;
end

local args = { 'x', 'y', 'z' };

print('Coroutine');
local co = coroutine.create( myfn );
print( 'run', coroutine.resume( co, unpack( args ) ) );
print( 'run', coroutine.resume( co, unpack( args ) ) );
-- cannot resume dead coroutine
print( 'run', coroutine.resume( co, unpack( args ) ) );

print('');

print('ReCo');
co = reco.new( myfn );
print( 'run', co( unpack( args ) ) );
print( 'run', co( unpack( args ) ) );
print( 'run', co( unpack( args ) ) );
```
