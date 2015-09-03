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



