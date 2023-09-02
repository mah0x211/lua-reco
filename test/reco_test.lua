local testcase = require('testcase')
local reco = require('reco')

function testcase.new()
    local f = function()
    end

    -- test that returns new instance of reco
    local co = assert(reco.new(f))
    assert.match(tostring(co), '^reco: ', false)

    -- test that throws an error with invalid argument
    local err = assert.throws(function()
        reco.new({})
    end)
    assert.match(err, '#1 .+ [(]function expected, got table', false)
end

function testcase.reset()
    local co = assert(reco.new(function()
        coroutine.yield('foo', 'bar')
        return 'baz', 'qux'
    end))

    -- test that returns values from yield
    local done, rc, again = co()
    assert.is_false(done)
    assert.equal(rc, reco.YIELD)
    assert.is_true(again)
    assert.equal({
        co:results(),
    }, {
        'foo',
        'bar',
    })

    -- test that returns values from return
    done, rc, again = co()
    assert.is_true(done)
    assert.equal(rc, reco.OK)
    assert.is_nil(again)
    assert.equal({
        co:results(),
    }, {
        'baz',
        'qux',
    })

    -- test that reset internal coroutine
    co:reset()
    done, rc = co()
    assert.is_false(done)
    assert.equal(rc, reco.YIELD)
    assert.equal({
        co:results(),
    }, {
        'foo',
        'bar',
    })

    -- test that reset with new function
    co:reset(function()
        return 'hello', 'world!'
    end)
    done, rc = co()
    assert.is_true(done)
    assert.equal(rc, reco.OK)
    assert.equal({
        co:results(),
    }, {
        'hello',
        'world!',
    })
end

function testcase.call_results()
    local co = assert(reco.new(function()
        return 'foo', 1, {}
    end))

    -- test that returns true
    local done, rc = assert(co())
    assert.is_true(done)
    assert.equal(rc, reco.OK)

    -- test that returns results
    assert.equal({
        co:results(),
    }, {
        'foo',
        1,
        {},
    })

    -- test that returns nil
    assert.is_nil(co:results())

    -- test that returns ERRRUN
    co = assert(reco.new(function()
        return {} + 1
    end))
    done, rc = co()
    assert.is_true(done)
    assert.equal(rc, reco.ERRRUN)
    assert.match(co:results(), 'stack traceback:')

    -- test that can be called multiple times
    done, rc = co()
    assert.is_true(done)
    assert.equal(rc, reco.ERRRUN)
    assert.match(co:results(), 'stack traceback:')
end

function testcase.getinfo()
    local fn = function()
        coroutine.yield('foo', 1, {})
        local a = {} + 1
        return 'foo', 1, a, {}
    end
    local fninfo = debug.getinfo(fn)
    local co = assert(reco.new(fn))
    local done, rc = co()
    assert.is_false(done)
    assert.equal(rc, reco.YIELD)

    -- test that get debug info of coroutine
    local info = assert(co:getinfo())
    assert.equal({
        ftransfer = info.ftransfer,
        istailcall = info.istailcall,
        isvararg = info.isvararg,
        lastlinedefined = info.lastlinedefined,
        linedefined = info.linedefined,
        namewhat = info.namewhat,
        nparams = info.nparams,
        ntransfer = info.ntransfer,
        nups = info.nups,
        short_src = info.short_src,
        source = info.source,
        what = info.what,
    }, {
        ftransfer = fninfo.ftransfer,
        istailcall = fninfo.istailcall,
        isvararg = fninfo.isvararg,
        lastlinedefined = fninfo.lastlinedefined,
        linedefined = fninfo.linedefined,
        namewhat = fninfo.namewhat,
        nparams = fninfo.nparams,
        ntransfer = fninfo.ntransfer,
        nups = fninfo.nups,
        short_src = fninfo.short_src,
        source = fninfo.source,
        what = fninfo.what,
    })
end

function testcase.throw_error()
    local co = assert(reco.new(function()
    end))

    -- test that throws an error with invalid self argument
    local err = assert.throws(function()
        co.results({})
    end)
    assert.match(err, '#1 .+ [(]reco expected, got table', false)
end
