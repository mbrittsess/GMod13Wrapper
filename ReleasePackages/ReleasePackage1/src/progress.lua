local ins = table.insert

local lapi = io.open("lapi.c")

local progress = setmetatable({
    complete   = {};
    incomplete = {};
    pending    = {};
},{
    __newindex = function()
        error("Tried to write to an invalid index in progress[].")
    end
})

local completion = setmetatable({},{
    __newindex = function(tbl, key, newval)
        if type(newval) ~= "number" then 
            error("Tried to insert a non-number into 'completion' table.")
        else
            rawset(tbl, key, newval)
        end
    end
})


while true do
    local line = lapi:read("*l")
    if not line then break end
    
    if line:match("^/%* WRAPPROGRESS") then
        local funcname = lapi:read("*l"):match("^%*%* FUNCTION: ([%w_]+)")
        if not funcname then error("Error while trying to parse function name.") end
        
        local statusline = lapi:read("*l")
        if not statusline then error("Error while trying to parse function status.") end
        
        local typeof = statusline:match("^%*%* STATUS: (%w+)")
        if typeof == "COMPLETE" then
            ins(progress.complete, funcname)
            ins(completion, 100)
        elseif typeof == "INCOMPLETE" then
            local percentage = tonumber(statusline:match("^** STATUS: INCOMPLETE (%d+)%%"))
            ins(progress.incomplete, funcname)
            ins(completion, percentage)
        elseif typeof == "PENDING" then
            ins(progress.pending, funcname)
            ins(completion, 0)
        else
            error("Unknown function status value.")
        end
        
        lapi:read("*l")
    end
end

local completion_percentage = 0
for _,v in ipairs(completion) do
    completion_percentage = completion_percentage + v
end
completion_percentage = completion_percentage / #completion

local num_of_funcs = #progress.complete + #progress.incomplete + #progress.pending

local fmt = string.format
io.write("Report on lapi.c progress:\n")
io.write("    Total number of functions: ", num_of_funcs, "\n")
io.write("     Number of completed functions: ", #progress.complete, " (", fmt("%.2f%%",(#progress.complete / num_of_funcs)*100)," of total)\n")
io.write("     Number of incomplete functions: ", #progress.incomplete, " (", fmt("%.2f%%",(#progress.incomplete / num_of_funcs)*100)," of total)\n")
io.write("     Number of pending functions: ", #progress.pending, "  (", fmt("%.2f%%",(#progress.pending / num_of_funcs)*100)," of total)\n")
io.write("    Total progress: ",fmt("%.2f%%",completion_percentage),"\n")
io.write("     Completed functions:\n")
for _,name in ipairs(progress.complete) do
io.write("      ",name,"\n")
end
io.write("     Incomplete functions:\n")
for _,name in ipairs(progress.incomplete) do
io.write("      ",name,"\n")
end
io.write("     Pending functions:\n")
for _,name in ipairs(progress.pending) do
io.write("      ",name,"\n")
end
os.exit()