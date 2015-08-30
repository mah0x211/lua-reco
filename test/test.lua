
local function sample()
    print 'ok';
end

local co = coroutine.create( sample );
print( coroutine.resume( co ) );
print( coroutine.resume( co ) );

co = coroutine.wrap( sample );
print( co() );
print( co() );

