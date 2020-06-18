--[[This code is rather dense, obtuse, and not up to my usual documentation standards. My apologies.
Most of the explanations for what this code does, can be found within lapi.c, in the locations where
the functions are used. You're best off finding them by looking for uses of their GUID names, i.e.,
'guidUpvalues', that sort of thing.]]
local OrigRequireName, ZeroFUD, _R, UserdataGC = ...

--List of GUIDs
local guidUpvalues     = "{F56CF593-BBB6-4f54-8320-9B0942A85506}"
local guidPushfString  = "{CC38C278-8672-43a0-AFA4-9C2A90FC91B9}"
local guidLenFunc      = "{771DD06F-92A2-4e23-8EEC-317F7E17A3D8}"
local guidConcatFunc   = "{3C0C3DEF-DC94-4043-9073-4B21E10419A3}"
local guidLessThanFunc = "{9FFB88AB-5676-4345-85B8-B30EBB5C4CE2}"
local guidFUDSizes     = "{F5D2E50D-24B6-461c-8B20-A758EE8DC7C9}"
local guidFUDMeta      = "{16BBDBE2-B572-4d2e-AC4F-1D43FAAB1250}"
local guidSetMTFunc    = "{115965B1-94B1-41f9-9603-F62F37262D8D}"
local guidLUDCache     = "{0CC52835-A74E-4f3f-8C4A-0B41F6EE6AA3}"
local guidLUDMeta      = "{81410E3D-C338-4a6f-9EE1-1072A89D26BB}"
local guidToPtrFunc    = "{6E540BD9-836B-4419-998F-97D6E21295AA}"
local guidLGSRefs      = "{EF44B6B3-C150-4955-8337-5D57DEAB8303}"
local guidUDPrepFunc   = "{AD0D9949-2232-4505-B92C-7342216A6A8F}"
local guidAnchMeta     = "{54A22EA9-6E04-4fcf-9557-617756C1AC1A}" --Not listed in gmwrapper.h, not used there
local guidUDAnchors    = "{E98ACFD3-7ECF-45f6-B82B-8FB1F58EB601}" --Also not listed in gmwrapper.h (but locally in lua_type() within lapi.c)
local guidFUDPtrStr    = "{1B0A2F15-F771-44a1-9B3E-0E52F4887E3A}" --Not listed in gmwrapper.h

local weak_key_meta = {__mode = "k"}
local weak_val_meta = {__mode = "v"}

--Upvalue stuff
if not _R[guidUpvalues] then _R[guidUpvalues] = setmetatable({}, weak_key_meta) end

--lua_pushfstring()'s function
do local ins, concat, fmt, type = table.insert, table.concat, string.format, type
local actions = {
    ["string"]  = function(pos, value, args, strings)
        ins(strings, value)
    end;
    ["boolean"] = function(pos, value, args, strings)
        return
    end;
    ["number"]  = function(pos, value, args, strings)
        if type(args[pos-1]) == "boolean" then
            ins(strings, fmt("%d", value))
        else
            ins(strings, fmt("%f", value))
        end
    end;
}

if not _R[guidPushfString] then _R[guidPushfString] = function(...)
    local args = {...}
    local strings = {}
    for pos, value in ipairs(args) do actions[type(value)](pos, value, args, strings) end
    
    return concat(strings)
end end end

--Some basic operation stuff
do local concat, d_getmetatable, d_setmetatable, pcall, error = table.concat, debug.getmetatable, debug.setmetatable, pcall, error
local match, tonumber, tostring = string.match, tonumber, tostring
local function raise_without_location(msg) error(msg:match(" (.*)$")) end

do local function len_func(a) return #a end
if not _R[guidLenFunc] then _R[guidLenFunc] = function(obj)
    local status, res = pcall(len_func, obj)
    return status and res or raise_without_location(res)
end end end

do local function concat_func(a, b) return a .. b end
if not _R[guidConcatFunc] then _R[guidConcatFunc] = function(...)
    local args = {...}
    local intermediate = args[#args-1] .. args[#args]
    for i = (#args-2), 1, -1 do
        local status, res = pcall(concat_func, args[i], intermediate)
        intermediate = status and res or raise_without_location(res)
    end
    return intermediate
end end end

do local function cmp_lt(a, b) return a < b end
if not _R[guidLessThanFunc] then _R[guidLessThanFunc] = function(obj1, obj2)
    local status, res = pcall(cmp_lt, a, b)
    return status and res or raise_without_location(res)
end end end end

--Full Userdata stuff
if not _R[guidFUDSizes]  then _R[guidFUDSizes]  = setmetatable({}, weak_key_meta) end
do local fmt = string.format
if not _R[guidFUDMeta]   then _R[guidFUDMeta]   = {
    MetaName="userdata",
    __metatable=false,
    __tostring=function(obj) return fmt("userdata: %s", _R[guidFUDPtrStr][obj]) end} --Had to add a __tostring for these, as well.
end end
if not _R[guidAnchMeta]  then _R[guidAnchMeta]  = {MetaName="userdata_anch", __gc=UserdataGC} end
if not _R[guidUDAnchors] then _R[guidUDAnchors] = setmetatable({}, weak_key_meta) end
if not _R[guidFUDPtrStr] then _R[guidFUDPtrStr] = setmetatable({}, weak_key_meta) end
do local setmetatable = debug.setmetatable
   local fudmeta  = _R[guidFUDMeta]
   local anchmeat = _R[guidAnchMeta]
   local rawset = rawset
if not _R[guidSetMTFunc] then _R[guidSetMTFunc] = function(obj, mt)
    if _R[guidFUDSizes][obj] then --It's a Vanillin FUD
        if mt==nil then
            setmetatable(obj, fudmeta)
        else
            setmetatable(obj, mt)
            rawset(mt, "MetaName", "userdata")
        end
    else
        setmetatable(obj, mt)
    end
end end 
if not _R[guidUDPrepFunc] then _R[guidUDPrepFunc] = function(front, anch, size, ptr)
    setmetatable(front, fudmeta)
    setmetatable(anch,  anchmeta)
    _R[guidUDAnchors][anch] = front
    _R[guidFUDSizes][front] = size
    _R[guidFUDPtrStr][front]= ptr
    
    return front
end end end

--Light Userdata stuff
if not _R[guidLUDCache] then _R[guidLUDCache] = setmetatable({}, {__mode="kv"}) end
do local fmt = string.format
   local LUDCache = _R[guidLUDCache]
if not _R[guidLUDMeta] then _R[guidLUDMeta] = {
    MetaName="userdata";
    __tostring=function(obj) return fmt("userdata: %s", LUDCache[obj]) end;
    }
end end

--Function for lua_topointer()
do local d_getmetatable, d_setmetatable, tonumber = debug.getmetatable, debug.setmetatable, tonumber
if not _R[guidToPtrFunc] then _R[guidToPtrFunc] = function(obj)
    local mt = d_getmetatable(obj)
    if mt then d_setmetatable(obj, nil) end
    local retval = tostring(obj):match("(%x+)$")
    if mt then d_setmetatable(obj, mt) end
    return retval
end end end

--lua_getstack() references
if not _R[guidLGSRefs] then _R[guidLGSRefs] = setmetatable({}, weak_val_meta) end

return ("luaopen_"..(OrigRequireName:match("%-") and OrigRequireName:match("%-(.*)") or OrigRequireName):gsub("%.", "_"))