local assert = require('assertex')
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
    co.fn = function()
        return {} + 1
    end
    done, rc = co()
    assert.is_true(done)
    assert.equal(rc, reco.ERRRUN)
    assert.match(co:results(), 'stack traceback:')
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
