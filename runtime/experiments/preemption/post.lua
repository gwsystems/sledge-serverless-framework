-- Default to 1 request / second
wrk.method = "POST"
wrk.body   = "10\n"
wrk.headers["Content-Type"] = "text/plain"
local delay_val = 1000

function init(args)
    if #args == 0 then
        io.write("[wrk stuff] -- --delay [delay in ms] [args ...]\n")
        os.exit();
    end 

    local current_arg = 1
    while current_arg <= #args do
        if args[current_arg] == "--delay" then
            delay_val = args[current_arg + 1]
            current_arg = current_arg + 2;
            io.write(string.format("Delay: %s\n", delay_val))
        else 
            -- Concatenate all remaining args
            local buffer = ""
            for i = current_arg, #args, 1 do
                buffer = buffer .. args[i]
            end 
            io.write(string.format("Buffer: %s\n", buffer))
            wrk.body = buffer
            -- And exit loop
            break;    
        end
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
    -- io.write(string.format("%s: %s\n", status, body))
end

-- Done Phase

-- Called when complete, presenting aggregate results
function done(summary, latency, requests)
    io.write("Percentile, Latency\n");
    for i = 1, 99 do
        io.write(string.format("%d, %d\n", i, latency:percentile(i)))
    end
end



