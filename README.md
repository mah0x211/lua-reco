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

### co, err = reco.new( fn:function, ... )

create a new reusable coroutine object.

**Parameters**

- `fn:function`: function that run in coroutine.
- `...`: arguments that passed to fn on invocation.

**Returns**

1. `co:userdata`: reco object or nil.
2. `err:str`: error string. 


### ... = co:getArgs( [idx:int] )

returns curried arguments.

**Parameters**

- `idx:int`: argument index number.

**Returns**

- `...`: curried arguments of coroutine.


### status = co:status()

returns a last status code.

**Returns**

- `status:int`: status code.


### ok, ... = co( ... )

run a reusable coroutine object.

**Parameters**

- `...`: you can pass any arguments.

**Returns**

1. `ok:boolean`: true on success, or false on failure.
2. function return values, or error string, status code and traceback.


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

print('');

print('Arguments');
co = reco.new( myfn, 1, 2, 3, 4 );
print( 'args', co:getArgs() );
print( 'arg idx:1', co:getArgs(1) );
```
