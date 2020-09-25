-- Default to 1 request / second
wrk.method = "POST"
wrk.body   = "10\n"
wrk.headers["Content-Type"] = "text/plain"
local delay_val = 1000

function init(args)
    if #args >= 1 then
        -- First argument is "requests per second" per thread
        delay_val = (1000 / args[1])
    end
    
    if #args >= 2 then
        -- Second argument is argument
        wrk.body = args[2]
    end
end

-- Uncomment to dynamically generate a different request each time
-- function request()
--     return wrk.format(nil, nil, nil,tostring(math.random(10, 23)) .."\n")
-- end

-- Wrk calls a function name delay to get the delay between requests (in ms)
function delay()
    return delay_val
end

function response(status, headers, body)
    io.write(string.format("%s: %s\n", status, body))
end

-- Done Phase

-- Called when complete, presenting aggregate results
function done(summary, latency, requests)
    for i = 1, 99 do
        io.write(string.format("%d %d\n", i, latency:percentile(i)))
    end
end



