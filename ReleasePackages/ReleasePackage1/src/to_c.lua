local res = {}
for line in io.lines() do
    res[#res+1] = '"' .. string.gsub(line, [=[[\"]]=], {[ [[\]]]=[[\\]],['"'] = [[\"]]}) .. [[\n"]]
end
io.write(table.concat(res, '\n'))
os.exit()