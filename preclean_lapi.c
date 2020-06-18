#include <windows.h>
#include <Strsafe.h>
#include <stdarg.h>
#include <stdlib.h> /* Need it for lua_topointer() to use strtoul() Maybe we can find an alternative built-into Windows? */
#include <math.h> /* For fmod() */

#define GMMODULE
 #include <Interface.h>
#undef GMMODULE

#include <lua.hpp>
#include "gmwrapper.h"

#pragma comment(lib, "user32")
static __inline void DbgMsg(const char* msg) {
    MessageBoxA(NULL, msg, "Debug Message", MB_OK);
};

extern "C" __declspec(dllexport) int gmod13_open(lua_State* L) {
    Prep;
    
    lua_CFunction LuaOpenFunc = NULL;
    const char* FinalRequireName = NULL;
    OrigRequireName = lua_tostring(L, 1);
    ThisHeap = GetProcessHeap();
    
    /* Phase One: Lua-side Initialization */
    lua_settop(L, 1);
    CompileAndPush(L, LuaCombinedSrc, "luacombined.lua");
    lua_pushvalue(L, 1);                   /* Argument 1 to luacombined.lua, the module name */
    lua_pushcfunction(L, ZeroFUD);         /* Argument 2 to luacombined.lua, a function that returns 0-size full userdata */
    lua_pushvalue(L, LUA_REGISTRYINDEX);   /* Argument 3 to luacombined.lua, the registry table */
    lua_pushcfunction(L, UserdataGC);
    lua_call(L, 4, 1);
    
    /* Phase Two: Find entrypoint function for this module */
    FinalRequireName = lua_tostring(L, 2); /* Return 1 from luacombined.lua, entrypoint function name */
    
   {HMODULE OurModuleHandle;
        /* Phase Two, Part One: Get handle of our own module */
    if (!GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
        (LPCWSTR)gmod13_open, /* Why yes, GetModuleHandleEx *does* produce one of the strangest typecasts in existence. */
        &OurModuleHandle)) {
        luaL_error(L, "In library '%s': error initializing Vanillin: failed to get handle to own module", OrigRequireName);
    };
        /* Phase Two, Part Two: Get address of the requisite procedure within our own module */
    LuaOpenFunc = (lua_CFunction)GetProcAddress(OurModuleHandle, FinalRequireName);
    if (LuaOpenFunc == NULL) {
        luaL_error(L, "In library '%s': error initializing Vanillin: could not find entrypoint function", OrigRequireName);
    };
   };
    
    /* Phase Three: Transfer control to 'true' entrypoint function */
    lua_settop(L, 1);
    return LuaOpenFunc(L);
};

extern "C" __declspec(dllexport) int gmod13_close(lua_State* L) {
    return 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_newstate
** STATUS: COMPLETE
    Always returns NULL. This *is* a valid behavior for lua_newstate(); it technically signals an out-of-memory condition. 
*/
LUA_API lua_State *(lua_newstate) (lua_Alloc f, void *ud) {
    return NULL;
};

/* WRAPPROGRESS
** FUNCTION: lua_close
** STATUS: COMPLETE
*/
LUA_API void       (lua_close) (lua_State *L) {
    luaL_error(L, VanillinCannotImplementError, OrigRequireName, __FUNCTION__);
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_newthread
** STATUS: COMPLETE
*/
LUA_API lua_State *(lua_newthread) (lua_State *L) {
    luaL_error(L, VanillinCannotImplementError, OrigRequireName, __FUNCTION__);
    return NULL;
};

/* WRAPPROGRESS
** FUNCTION: lua_atpanic
** STATUS: COMPLETE
*/
LUA_API lua_CFunction (lua_atpanic) (lua_State *L, lua_CFunction panicf) {
    luaL_error(L, VanillinCannotImplementError, OrigRequireName, __FUNCTION__);
    return NULL;
};

/* WRAPPROGRESS
** FUNCTION: lua_gettop
** STATUS: COMPLETE
*/
LUA_API int   (lua_gettop) (lua_State *L) {
    Prep;
    
    return I->Top();
};

/* WRAPPROGRESS
** FUNCTION: lua_settop
** STATUS: COMPLETE
*/
LUA_API void  (lua_settop) (lua_State *L, int idx) {
    Prep;
    
    int StackTop = lua_gettop(L);
    int NewTop;
    switch (IdxClassification(L, idx, &NewTop, NULL)) {
        case VanIdx_Nonsensical : /* If it was zero (or equivalent), that's actually legal here, so fall-through if that's the case */
            if ((StackTop + 1 + idx) != 0) {
                BadIdxError(L, __FUNCTION__, idx);
            };
        case VanIdx_OnStack    :
        case VanIdx_AboveStack :
            if (NewTop < StackTop) {
                I->Pop(StackTop - NewTop);
            } else if (NewTop > StackTop) {
                for (int i = 0; i < (NewTop - StackTop); i++) {
                    I->PushNil();
                };
            };
            
            return;
        default :
            BadIdxError(L, __FUNCTION__, idx);
            return;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_pushvalue
** STATUS: COMPLETE
    This function should only accpet *valid* indices. Indices which are *acceptable* but *invalid* should produce an error.
*/
LUA_API void  (lua_pushvalue) (lua_State *L, int idx) {
    Prep;
    
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_Nonsensical :
        case VanIdx_AboveStack :
            BadIdxError(L, __FUNCTION__, idx);
            return;
        case VanIdx_OnStack :
            I->Push(idx);
            return;
        case VanIdx_Pseudo_Tbl :
            switch (idx) {
                case LUA_REGISTRYINDEX : idx = GarrysMod::Lua::SPECIAL_REG;  break;
                case LUA_ENVIRONINDEX  : idx = GarrysMod::Lua::SPECIAL_ENV;  break;
                case LUA_GLOBALSINDEX  : idx = GarrysMod::Lua::SPECIAL_GLOB; break;
                default                : luaL_error(L, ImpossibleConditionEncountered,
                                            OrigRequireName, __FUNCTION__);
            };
            I->PushSpecial(idx);
            return;
        case VanIdx_Pseudo_Upv :
            int ordUpv = UpvalueIdxToOrdinal(L, idx);
            
            if (!CheckUpv(L, ordUpv)) {
                luaL_error(L, "In library '%s': tried to push function upvalue #%d, but function does not have that many upvalues, if any",
                    OrigRequireName, ordUpv);
            };
            
            lua_getfield(L, LUA_REGISTRYINDEX, guidUpvalues);
            PushSelf(L);
            lua_gettable(L, -2);
            lua_pushinteger(L, ordUpv);
            lua_gettable(L, -2);
            lua_insert(L, -3);
            lua_pop(L, 2);
            
            return;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_remove
** STATUS: COMPLETE
*/
LUA_API void  (lua_remove) (lua_State *L, int idx) {
    Prep;
    
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_OnStack :
            I->Remove(idx);
            return;
        default :
            BadIdxError(L, __FUNCTION__, idx);
            return;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_insert
** STATUS: COMPLETE
*/
LUA_API void  (lua_insert) (lua_State *L, int idx) {
    Prep;
    
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_OnStack :
            I->Insert(idx);
            return;
        default :
            BadIdxError(L, __FUNCTION__, idx);
            return;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_replace
** STATUS: COMPLETE
*/
LUA_API void  (lua_replace) (lua_State *L, int idx) {
    int aidx;
    
    if (lua_gettop(L) < 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_replace()!", OrigRequireName);
        return;
    };
    
    switch (IdxClassification(L, idx, &aidx, NULL)) {
        case VanIdx_OnStack :
            if (aidx != lua_gettop(L)) {
                lua_insert(L, aidx);
                lua_remove(L, aidx+1);
            };
            return;
        case VanIdx_Pseudo_Tbl :
            switch (aidx) {
                case LUA_REGISTRYINDEX :
                    luaL_error(L, "In library '%s': Vanillin lacks the capability to replace the Registry Table!", OrigRequireName);
                    return;
                case LUA_ENVIRONINDEX :
                    PushSelf(L);
                    lua_insert(L, -2);
                    lua_setfenv(L, -2);
                    lua_pop(L, 1);
                    return;
                case LUA_GLOBALSINDEX :
                    luaL_error(L, "In library '%s': Vanillin lacks the capability to replace the Globals Table!", OrigRequireName);
                    return;
                default :
                    luaL_error(L, ImpossibleConditionEncountered, OrigRequireName, __FUNCTION__);
                    return;
            };
        case VanIdx_Pseudo_Upv :
           {int ordinal = UpvalueIdxToOrdinal(L, idx);
            if (!CheckUpv(L, ordinal)) {
                luaL_error(L, "In library '%s': attempted to set upvalue #%d in a function without any", OrigRequireName, ordinal);
                return;
            } else {
                lua_getfield(L, LUA_REGISTRYINDEX, guidUpvalues);
                PushSelf(L);
                lua_gettable(L, -2);
                lua_pushinteger(L, ordinal);
                lua_pushvalue(L, -4); /* The new value */
                lua_settable(L, -3);
                lua_pop(L, 3);
                
                return;
            };};
        default :
            BadIdxError(L, __FUNCTION__, idx);
            return;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_checkstack
** STATUS: COMPLETE
    Since we have no actual way of checking if there's enough stack space, this function just always returns TRUE
unless we're asked for negative stack space.
*/
LUA_API int   (lua_checkstack) (lua_State *L, int sz) {
    if (sz >= 0) {
        return TRUE;
    } else {
        luaL_error(L, "In library '%s': called function %s(), asked if %d extra stack spaces available, that makes no sense",
            OrigRequireName, __FUNCTION__, sz);
        return FALSE;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_xmove
** STATUS: COMPLETE
*/
LUA_API void  (lua_xmove) (lua_State *from, lua_State *to, int n) {
    luaL_error(from, VanillinCannotImplementError, OrigRequireName, __FUNCTION__);
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_isnumber
** STATUS: COMPLETE
*/
LUA_API int             (lua_isnumber) (lua_State *L, int idx) {
    switch (lua_type(L, idx)) {
        case LUA_TNUMBER :
            return 1;
        case LUA_TSTRING :
           {int retval;
           
            lua_getglobal(L, "tonumber");
            lua_pushvalue(L, idx);
            lua_call(L, 1, 1);
            retval = !lua_isnil(L, -1);
            lua_pop(L, 1);
            
            return retval;};
        default :
            return 0;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_isstring
** STATUS: COMPLETE
*/
LUA_API int             (lua_isstring) (lua_State *L, int idx) {
    switch (lua_type(L, idx)) {
        case LUA_TNUMBER :
        case LUA_TSTRING :
            return 1;
        default :
            return 0;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_iscfunction
** STATUS: COMPLETE
*/
LUA_API int             (lua_iscfunction) (lua_State *L, int idx) {
    if (lua_isfunction(L, idx)) {
       {int retval = 0;
        int aidx = lua_absindex(L, idx);
        size_t len;
        const char* what;
        
        lua_getglobal(L, "debug");
        lua_getfield(L, -1, "getinfo");
        lua_pushvalue(L, aidx);
        lua_call(L, 1, 1);
        lua_getfield(L, -1, "what");
        what = lua_tolstring(L, -1, &len);
        if ((what[0]=='C') && (len==1)) {
            retval = 1;
        };
        lua_pop(L, 3);
        
        return retval;}
    } else {
        return 0;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_isuserdata
** STATUS: COMPLETE
*/
LUA_API int             (lua_isuserdata) (lua_State *L, int idx) {
    int type = lua_type(L, idx);
    return ((type==LUA_TUSERDATA) || (type==LUA_TLIGHTUSERDATA));
};

/* WRAPPROGRESS
** FUNCTION: lua_type
** STATUS: COMPLETE
    This function must allow 'acceptable indices', a supserset of 'valid indices' that also includes
values above the top of the stack, which are all considered to be either non-existent, or nil, depending on circumstance.
    Since we do rely on I->GetType(), there is a bit of ambiguity when it comes to Garry-specific 'types'. We have to decide
on a Lua type to represent them as, as we cannot really be certain of what types they actually are. While they are almost
universally Full Userdata, they are not really 'semantically' userdata in any meaningful sense, one could not meaningfully ask
for their length, one is not supposed to deal with their contents.
    As such, I've decided that they will be represented as tables. So, any unrecognized type returned by I->GetType() will
cause the function to return LUA_TTABLE.
*/
#if 0
LUA_API int             (lua_type) (lua_State *L, int idx) {
    Prep;
    int retval;
    
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_Nonsensical :
            BadIdxError(L, __FUNCTION__, idx);
            return 0;
        case VanIdx_AboveStack :
            return LUA_TNONE;
        case VanIdx_Pseudo_Upv :
            if (!CheckUpv(L, UpvalueIdxToOrdinal(L, idx))) {
                return LUA_TNONE;
            };
    };
    
    lua_pushvalue(L, idx);
    switch (I->GetType(-1)) {
        case GarrysMod::Lua::Type::INVALID :
            retval = LUA_TNONE; break;
        case GarrysMod::Lua::Type::NIL :
            retval = LUA_TNIL; break;
        case GarrysMod::Lua::Type::NUMBER :
            retval = LUA_TNUMBER; break;
        case GarrysMod::Lua::Type::BOOL :
            retval = LUA_TBOOLEAN; break;
        case GarrysMod::Lua::Type::STRING :
            retval = LUA_TSTRING; break;
        case GarrysMod::Lua::Type::TABLE :
            retval = LUA_TTABLE; break;
        case GarrysMod::Lua::Type::FUNCTION :
            retval = LUA_TFUNCTION; break;
        case GarrysMod::Lua::Type::USERDATA :
            /* Have to check if this is actually a Vanillin Light Userdata */
            lua_getfield(L, LUA_REGISTRYINDEX, guidLUDCache);
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);
            retval = (lua_isnil(L, -1)) ? LUA_TUSERDATA : LUA_TLIGHTUSERDATA;
            lua_pop(L, 2);
            break;
        case GarrysMod::Lua::Type::THREAD :
            retval = LUA_TTHREAD; break;
        case GarrysMod::Lua::Type::LIGHTUSERDATA :
            retval = LUA_TLIGHTUSERDATA; break;
        default:
           {int type = I->GetType(-1);
            lua_getglobal(L, "print");
            lua_pushfstring(L, "Was queried for value at index '%d', Garry reports the type is '%d'", idx, type);
            lua_call(L, 1, 0);};
            retval = LUA_TTABLE; break;
    };
    
    lua_pop(L, 1);
    return retval;
};
#else
/*  Due to the new behavior of I->GetType() regarding Full Userdata, we have had to greatly change how this function
works. The gist of I->GetType()'s new behavior is like this:
    * Check if the value's *sane* Lua type is non-Full Userdata
      * If so, return its equivalent GAPI type value
      * Otherwise:
        * Blindly assume the full userdata's contents are a GarrysMod::Lua::Userdata, and check the ->type value
        * Check if that value corresponds to the numeric value of a sane Lua type
          * If so, instead return the value for GarrysMod::Lua::Type::USERDATA
          * Otherwise, return the value held by ->type
    This clearly-insane system has forced us to drastically change how the types for full userdata, light userdata,
and GMod userdata are handled. Thankfully, as before, anything that's not Vanillin-supplied full or light userdata
is reported as a table. So our new algorithm goes like this:
    * Get the value of I->GetType()
    * Does it correspond to a 'normal' Lua type, other than full userdata?
        * If so, return the sane Lua type corresponding to that
        * Otherwise:
          * Does the value correspond to fudID, ludID, or anchID? (the latter should never happen)
            * If so, does the value exist in _R[guidFUDSizes], _R[guidLUDCache], or _R[guidUDAnchors]?
              * If so, return LUA_TUSERDATA, LUA_TLIGHTUSERDATA, or LUA_TUSERDATA, respectively
            * Otherwise, return LUA_TTABLE
          * Otherwise, return LUA_TTABLE
*/
LUA_API int             (lua_type) (lua_State *L, int idx) {
    Prep;
    int retval;
    
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_Nonsensical :
            BadIdxError(L, __FUNCTION__, idx);
            return 0;
        case VanIdx_AboveStack :
            return LUA_TNONE;
        case VanIdx_Pseudo_Upv :
            if (!CheckUpv(L, UpvalueIdxToOrdinal(L, idx))) {
                return LUA_TNONE;
            };
    };
    
    lua_pushvalue(L, idx);
    switch (I->GetType(-1)) {
        case GarrysMod::Lua::Type::INVALID :
            retval = LUA_TNONE; break;
        case GarrysMod::Lua::Type::NIL :
            retval = LUA_TNIL; break;
        case GarrysMod::Lua::Type::NUMBER :
            retval = LUA_TNUMBER; break;
        case GarrysMod::Lua::Type::BOOL :
            retval = LUA_TBOOLEAN; break;
        case GarrysMod::Lua::Type::STRING :
            retval = LUA_TSTRING; break;
        case GarrysMod::Lua::Type::TABLE :
            retval = LUA_TTABLE; break;
        case GarrysMod::Lua::Type::FUNCTION :
            retval = LUA_TFUNCTION; break;
        case GarrysMod::Lua::Type::USERDATA :
            retval = LUA_TTABLE; break;
        case GarrysMod::Lua::Type::THREAD :
            retval = LUA_TTHREAD; break;
        case GarrysMod::Lua::Type::LIGHTUSERDATA :
            /* NEED CODE HERE - NOT SURE HOW I->GetType() TREATES VALUES MASQUERADING AS THIS 
            IF A FULL USERDATA INTERPRETED AS A LIGHT USERDATA CAN RETURN THIS VALUE, THEN IT
            CAN'T BE TRUSTED AS WE HAVE NO WAY TO DISTINGUISH FULL FROM LIGHT USERDATA. */
            retval = LUA_TLIGHTUSERDATA; break;
        case fudID :
            lua_getfield(L, LUA_REGISTRYINDEX, guidFUDSizes);
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);
            retval = (I->GetType(-1) != GarrysMod::Lua::Type::NIL) ? LUA_TUSERDATA : LUA_TTABLE;
            lua_pop(L, 2);
            break;
        case ludID :
            lua_getfield(L, LUA_REGISTRYINDEX, guidLUDCache);
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);
            retval = (I->GetType(-1) != GarrysMod::Lua::Type::NIL) ? LUA_TLIGHTUSERDATA : LUA_TTABLE;
            lua_pop(L, 2);
            break;
        case anchID :
           {static const char* guidUDAnchors = "{E98ACFD3-7ECF-45f6-B82B-8FB1F58EB601}";
            lua_getfield(L, LUA_REGISTRYINDEX, guidUDAnchors);
            lua_pushvalue(L, -2);
            lua_gettable(L, -2);
            retval = (I->GetType(-1) != GarrysMod::Lua::Type::NIL) ? LUA_TUSERDATA : LUA_TTABLE;
            lua_pop(L, 2);
            break;};
        default:
            /*
           {int type = I->GetType(-1);
            lua_getglobal(L, "print");
            lua_pushfstring(L, "Was queried for value at index '%d', Garry reports the type is '%d'", idx, type);
            lua_call(L, 1, 0);};
            */
            retval = LUA_TTABLE; break;
    };
    
    lua_pop(L, 1);
    return retval;
};
#endif

/* WRAPPROGRESS
** FUNCTION: lua_typename
** STATUS: COMPLETE
    Might replace "unknown" with "invalid type", this is the behavior of the normal Lua API
*/
LUA_API const char     *(lua_typename) (lua_State *L, int tp) {
    static const char* TypeNames[] = {
        "no value",
        "nil",
        "boolean",
        "userdata",
        "number",
        "string",
        "table",
        "function",
        "userdata",
        "thread"
    };
    
    return ((tp <= LUA_TTHREAD) && (tp >= LUA_TNONE)) ? TypeNames[tp+1] : "unknown";
};

/* WRAPPROGRESS
** FUNCTION: lua_equal
** STATUS: COMPLETE
*/
LUA_API int            (lua_equal) (lua_State *L, int idx1, int idx2) {
    Prep;
    
    int aidx1 = lua_absindex(L, idx1);
    int aidx2 = lua_absindex(L, idx2);
    
    lua_pushvalue(L, aidx1);
    lua_pushvalue(L, aidx2);
    
    int retval = I->Equal(-2, -1);
    
    lua_pop(L, 2);
    
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_rawequal
** STATUS: COMPLETE
*/
LUA_API int            (lua_rawequal) (lua_State *L, int idx1, int idx2) {
    Prep;
    
    int aidx1 = lua_absindex(L, idx1);
    int aidx2 = lua_absindex(L, idx2);
    
    lua_pushvalue(L, aidx1);
    lua_pushvalue(L, aidx2);
    
    int retval = I->RawEqual(-2, -1);
    
    lua_pop(L, 2);
    
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_lessthan
** STATUS: COMPLETE
*/
LUA_API int            (lua_lessthan) (lua_State *L, int idx1, int idx2) {
    int aidx1;
    int aidx2;
    
   {enum VanillinIdxClass idx1class = IdxClassification(L, idx1, &aidx1, NULL);
    enum VanillinIdxClass idx2class = IdxClassification(L, idx2, &aidx2, NULL);
    
    if (((idx1class==VanIdx_Pseudo_Upv) && !CheckUpv(L, UpvalueIdxToOrdinal(L,idx1))) || (idx1class==VanIdx_Nonsensical) || (idx1class==VanIdx_AboveStack) ||
        ((idx2class==VanIdx_Pseudo_Upv) && !CheckUpv(L, UpvalueIdxToOrdinal(L,idx2))) || (idx2class==VanIdx_Nonsensical) || (idx2class==VanIdx_AboveStack)) {
        return 0; 
    } else {
       {int retval;
        lua_getfield(L, LUA_REGISTRYINDEX, guidLessThanFunc);
        lua_pushvalue(L, aidx1);
        lua_pushvalue(L, aidx2);
        lua_call(L, 2, 1);
        retval = lua_toboolean(L, -1);
        lua_pop(L, 1);
        return retval;};
    };};
};

/* WRAPPROGRESS
** FUNCTION: lua_tonumber
** STATUS: COMPLETE
*/
LUA_API lua_Number      (lua_tonumber) (lua_State *L, int idx) {
    Prep;
    lua_Number TheNum; 
    
    lua_pushvalue(L, idx);
    TheNum = (lua_Number)I->GetNumber(-1);
    lua_pop(L, 1);
    
    return TheNum;
};

/* WRAPPROGRESS
** FUNCTION: lua_tointeger
** STATUS: COMPLETE
*/
LUA_API lua_Integer     (lua_tointeger) (lua_State *L, int idx) {
    Prep;
    lua_Integer TheNum;
    
    lua_pushvalue(L, idx);
    TheNum = (lua_Integer)I->GetNumber(-1);
    lua_pop(L, 1);
    
    return TheNum;
};

/* WRAPPROGRESS
** FUNCTION: lua_toboolean
** STATUS: COMPLETE
*/
LUA_API int             (lua_toboolean) (lua_State *L, int idx) {
    Prep;
    
    lua_pushvalue(L, idx);
    int retval = (int)I->GetBool(-1);
    lua_pop(L, 1);
    
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_tolstring
** STATUS: COMPLETE
*/
LUA_API const char     *(lua_tolstring) (lua_State *L, int idx, size_t *len) {
    Prep;
    
    unsigned int glen;
    const char*  ret;
    unsigned int aidx = lua_absindex(L, idx);
    
    lua_pushvalue(L, aidx);
    BOOL WasANum = lua_type(L, -1) == LUA_TNUMBER ? TRUE : FALSE;
    
    ret = I->GetString(-1, &glen);
    if (len != NULL) {
        *len = (size_t)glen;
    };
    if (WasANum) {
        lua_replace(L, aidx);
    } else {
        lua_pop(L, 1);
    };
    
    return ret;
};

/* WRAPPROGRESS
** FUNCTION: lua_objlen
** STATUS: COMPLETE
*/
LUA_API size_t          (lua_objlen) (lua_State *L, int idx) {
    int aidx = lua_absindex(L, idx);
    size_t retval;
    
    switch (lua_type(L, aidx)) {
        case LUA_TSTRING :
        case LUA_TTABLE  :
            lua_getfield(L, LUA_REGISTRYINDEX, guidLenFunc);
            lua_pushvalue(L, aidx);
            lua_call(L, 1, 1);
            retval = (size_t)lua_tointeger(L, -1);
            lua_pop(L, 1);
            
            return retval;
        case LUA_TUSERDATA :
            lua_getfield(L, LUA_REGISTRYINDEX, guidFUDSizes);
            lua_pushvalue(L, aidx);
            lua_gettable(L, -2);
            retval = (size_t)lua_tointeger(L, -1); /* If the FUD is not in there, it will return nil, which 
                converts to 0...the desired safe effect. */
            lua_pop(L, 2);
            
            return retval;
        default :
            return 0;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_tocfunction
** STATUS: COMPLETE
*/
LUA_API lua_CFunction   (lua_tocfunction) (lua_State *L, int idx) {
    Prep;
    
    lua_CFunction retval;
    lua_pushvalue(L, idx);
    retval = I->GetCFunction(-1);
    lua_pop(L, 1);
    
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_touserdata
** STATUS: COMPLETE
*/
#if 0
LUA_API void	       *(lua_touserdata) (lua_State *L, int idx) {
    Prep;
    
    void* retval = NULL;
    
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_Nonsensical :
        case VanIdx_AboveStack :
            return NULL;
        case VanIdx_Pseudo_Upv :
            if (!CheckUpv(L, UpvalueIdxToOrdinal(L, idx))) {
                return NULL;
            } else {
                break;
            };
    };
    
    lua_pushvalue(L, idx);
    if (lua_islightuserdata(L, -1)) {
        retval = *((void**)I->GetUserdata(-1));
    } else if (lua_isuserdata(L, -1)) {
        retval = I->GetUserdata(-1);
        if (retval == NULL) {
            DbgMsg("Retrieved block address of Full Userdata, got NULL!");
        };
    };
    
    lua_pop(L, 1);
    return retval;
};
#else
LUA_API void	       *(lua_touserdata) (lua_State *L, int idx) {
    Prep;
    
    void* retval = NULL;
    
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_Nonsensical :
        case VanIdx_AboveStack :
            return NULL;
        case VanIdx_Pseudo_Upv :
            if (!CheckUpv(L, UpvalueIdxToOrdinal(L, idx))) {
                return NULL; /* Wait, should we error or return NULL? DoesCheckUpv() raise errors? */
            } else {
                break;
            };
    };
    
    lua_pushvalue(L, idx);
    if (lua_isuserdata(L, -1)) { /* The procedure is now the same for both of them. */
        retval = ((GarrysMod::Lua::UserData*)I->GetUserdata(-1))->data;
    };
    
    lua_pop(L, 1);
    return retval;
};
#endif

/* WRAPPROGRESS
** FUNCTION: lua_tothread
** STATUS: COMPLETE
*/
LUA_API lua_State      *(lua_tothread) (lua_State *L, int idx) {
    luaL_error(L, "In library '%s': Vanillin lacks the capability to retrieve states from threads-on-the-stack!", OrigRequireName);
    
    return NULL;
};

/* WRAPPROGRESS
** FUNCTION: lua_topointer
** STATUS: COMPLETE
*/
LUA_API const void     *(lua_topointer) (lua_State *L, int idx) {
    int aidx;
    
    switch (IdxClassification(L, idx, &aidx, NULL)) {
        case VanIdx_Nonsensical :
        case VanIdx_AboveStack :
            return NULL;
        case VanIdx_Pseudo_Upv :
            if (!CheckUpv(L, UpvalueIdxToOrdinal(L, idx))) {
                return NULL;
            } else {
                break;
            };
    };
    
    switch (lua_type(L, idx)) {
        case LUA_TLIGHTUSERDATA :
        case LUA_TUSERDATA      : /* Should we maybe change this to its 'real' block address, or keep it as the ->data member? */
            return lua_touserdata(L, idx);
        case LUA_TTABLE    :
        case LUA_TTHREAD   :
        case LUA_TFUNCTION :
           {void* retval;
            lua_getfield(L, LUA_REGISTRYINDEX, guidToPtrFunc);
            lua_pushvalue(L, aidx);
            lua_call(L, 1, 1); /* Returns a base-16 string */
            retval = (void*)strtoul(lua_tostring(L, -1), NULL, 16);
            lua_pop(L, 1);
            
            return retval;};
        default :
            return NULL;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_pushnil
** STATUS: COMPLETE
*/
LUA_API void  (lua_pushnil) (lua_State *L) {
    Prep;
    
    I->PushNil();
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushnumber
** STATUS: COMPLETE
*/
LUA_API void  (lua_pushnumber) (lua_State *L, lua_Number n) {
    Prep;
    
    I->PushNumber((double)n);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushinteger
** STATUS: COMPLETE
*/
LUA_API void  (lua_pushinteger) (lua_State *L, lua_Integer n) {
    Prep;
    
    I->PushNumber((double)n);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushlstring
** STATUS: COMPLETE
*/
LUA_API void  (lua_pushlstring) (lua_State *L, const char *s, size_t len) {
    Prep;
    
    I->PushString(s, (unsigned int)len);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushstring
** STATUS: COMPLETE
*/
LUA_API void  (lua_pushstring) (lua_State *L, const char *s) {
    Prep;
    
    I->PushString(s, lstrlenA(s));
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushvfstring
** STATUS: COMPLETE
*/
LUA_API const char *(lua_pushvfstring) (lua_State *L, const char *fmt,
                                                      va_list argp) {
    int CountPushed = 0;
    int FmtOfs = 0;
    
    lua_getfield(L, LUA_REGISTRYINDEX, guidPushfString);
    
    while (1) {
        if (fmt[FmtOfs] == '\0') {
            /* Time to finish this. */
            break;
        } else if (fmt[FmtOfs] == '%') {
            /* Escape sequence processing. */
            FmtOfs += 1;
            switch (fmt[FmtOfs]) {
                case '%' :
                    lua_pushstring(L, "%");
                    break;
                case 's' :
                    lua_pushstring(L, va_arg(argp, const char*));
                    break;
                case 'f' :
                    lua_pushnumber(L, va_arg(argp, lua_Number));
                    break;
                case 'p' :
                   {char buf[4*sizeof(void *) + 8]; //Why is this calculated like this? I can't remember...
                    StringCbPrintfA(buf, sizeof(buf), "%p", va_arg(argp, void*));
                    lua_pushstring(L, (const char*)buf);
                    break;};
                case 'd' :
                    lua_pushboolean(L, FALSE);
                    lua_pushinteger(L, va_arg(argp, lua_Integer));
                    CountPushed += 1;
                    break;
                case 'c' :
                   {char to_be_pushed = (char)va_arg(argp, int);
                    lua_pushlstring(L, &to_be_pushed, 1);
                    break;};
                case '\0' :
                    goto force_break; /* Trailing %'s on the end of the string should do nothing. */
                    break;
                default :
                    lua_pushlstring(L, &fmt[FmtOfs], 1);
                    break;
            };
            FmtOfs += 1;
            CountPushed += 1;
            continue;
            
           force_break:
            break;
        } else {
            /* Literal insertion. */
            int LiteralStart = FmtOfs;
            char ThisChar = fmt[FmtOfs];
            while ((ThisChar != '%') && (ThisChar != '\0')) {
                FmtOfs += 1;
                ThisChar = fmt[FmtOfs];
            };
            lua_pushlstring(L, &fmt[LiteralStart], FmtOfs - LiteralStart);
            CountPushed += 1;
        };
    };
    
    lua_call(L, CountPushed, 1);
    
    return lua_tostring(L, -1);
};

/* WRAPPROGRESS
** FUNCTION: lua_pushfstring
** STATUS: COMPLETE
*/
LUA_API const char *(lua_pushfstring) (lua_State *L, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    const char* retval = lua_pushvfstring(L, fmt, argp);
    va_end(argp);
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushcclosure
** STATUS: COMPLETE
    Garry's API does now allow us to create functions with upvalues, via I->PushCCLosure().
    What's missing, however, is the rather critical fact that HE DOESN'T OFFER A WAY FOR FUNCTIONS TO RETRIEVE THEM.
    I'd be forced to use debug.getupvalue(), and that's just too inefficient. I'll stick with my own hacked-in way,
thank you, considering half the point of upvalues is to be fast and easily-accessed.
*/
LUA_API void  (lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n) {
    Prep;
    
    if ((n > 255) || (n < 0)) {
        luaL_error(L, "In library '%s': tried to create closure with %d upvalues, must be between 0 and 255 (inclusive)",
            OrigRequireName, n);
        
        return;
    } else if (n > lua_gettop(L)) {
        luaL_error(L, "In library '%s': tried to create closure with %d upvalues, insufficient values on stack",
            OrigRequireName, n);
        
        return;
    } else if (n != 0) {
       {int idxCurrUpv = lua_gettop(L) - n + 1;
        int ordCurrUpv = 1;
        
        lua_newtable(L);
        lua_pushinteger(L, n);
        lua_setfield(L, -2, "n");
        
        while (ordCurrUpv <= n) {
            lua_pushvalue(L, idxCurrUpv);
            lua_rawseti(L, -2, ordCurrUpv);
            
            ordCurrUpv += 1; idxCurrUpv += 1;
        };};
        
        lua_insert(L, -(n+1));
        lua_pop(L, n);
        lua_getfield(L, LUA_REGISTRYINDEX, guidUpvalues);
        lua_insert(L, -2);
        I->PushCFunction(fn);
        lua_pushvalue(L, -1);
        lua_insert(L, -4); /* Put one copy below _R[guidUpvalues] */
        lua_insert(L, -2); /* And another below the function's own upvalue table, as the func will be the key and the table the value. */
        lua_settable(L, -3);
        lua_pop(L, 1);
    } else {
        I->PushCFunction(fn);
    };
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushboolean
** STATUS: COMPLETE
*/
LUA_API void  (lua_pushboolean) (lua_State *L, int b) {
    Prep;
    
    I->PushBool((bool)b);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pushlightuserdata
** STATUS: COMPLETE
    Currently, this function cannot be implemented as easily as it could be in
the old version of Vanillin, as there is no new-GVomit function to create light
userdata. We will wait and see if another appears or if we have to create it
manually ourselves.
    
    Well, since no official implementation of this has appeared, we will have to
implement this ourselves. We push a string representation of the pointer as a key
into _R[guidLUDCache] to see if a corresponding userdata already exists. If not,
we create a Full Userdata whose payload is the pointer itself, with an appropriate
metatable to make it act like a light userdata (__eq is not necessary, all light
userdata with the same pointer are represented by the same FUD). FUDs are also keys,
not just values, with the string representation as values, in addition to keys.
So, given an object, we can quickly get the string representation of its value
without having to fetch its pointer or anything.

    We do not currently support type-wide metatables for Light Userdata. It would be
possible for us to do this (could borrow some lessons from our implementation of
full userdata in GMod 12 Vanillin) but it would be frightfully inefficient, and
it's not very high-priority, as no known library does this, no application that I'm
aware of does, and there's not much point to doing it except on 64-bit platforms.
*/
#if 0
LUA_API void  (lua_pushlightuserdata) (lua_State *L, void *p) {
    Prep;
    
    int OrigTop = lua_gettop(L);
    int CacheIdx;
    int AddrIdx;
    int ObjIdx;
    
    lua_getfield(L, LUA_REGISTRYINDEX, guidLUDCache); CacheIdx = lua_gettop(L);
    lua_pushfstring(L, "%p", p); AddrIdx = lua_gettop(L);
    lua_pushvalue(L, AddrIdx); ObjIdx = lua_gettop(L); /* This second copy of Addr will be replaced soon with the Obj */
    lua_gettable(L, CacheIdx);
    if (lua_isnil(L, ObjIdx)) {
        /* Create the Full Userdata, but not a Vanillin Full Userdata */
        void** block; lua_pop(L, 1);
        block = (void**)I->NewUserdata(sizeof(void*)); /* We bypass lua_newuserdata() for this */
        *block = p;
        
        /* Assign the metatable */
        lua_getfield(L, LUA_REGISTRYINDEX, guidLUDMeta);
        lua_setmetatable(L, ObjIdx);
        
        /* lud_cache[obj], lud_cache[addr] = addr, obj */
        lua_pushvalue(L, ObjIdx); lua_pushvalue(L, AddrIdx); lua_settable(L, CacheIdx);
        lua_pushvalue(L, AddrIdx); lua_pushvalue(L, ObjIdx); lua_settable(L, CacheIdx);
    };
    
    lua_pushvalue(L, ObjIdx);
    lua_replace(L, OrigTop+1);
    lua_settop(L, OrigTop+1);
    
    return;
};
#else
LUA_API void  (lua_pushlightuserdata) (lua_State *L, void *p) {
    Prep;
    
    int OrigTop = lua_gettop(L);
    int CacheIdx;
    int AddrIdx;
    int ObjIdx;
    
    lua_getfield(L, LUA_REGISTRYINDEX, guidLUDCache); CacheIdx = lua_gettop(L);
    lua_pushfstring(L, "%p", p); AddrIdx = lua_gettop(L);
    lua_pushvalue(L, AddrIdx); ObjIdx = lua_gettop(L); /* This second copy of Addr will be replaced soon with the Obj */
    lua_gettable(L, CacheIdx);
    if (lua_isnil(L, ObjIdx)) {
        /* Create the Full Userdata, but not a Vanillin Full Userdata */
        GarrysMod::Lua::UserData* value; lua_pop(L, 1);
        value = (GarrysMod::Lua::UserData*)I->NewUserdata(sizeof(GarrysMod::Lua::UserData));
        value->data = p;
        value->type = ludID;
        
        /* Assign the metatable */
        lua_getfield(L, LUA_REGISTRYINDEX, guidLUDMeta);
        lua_setmetatable(L, ObjIdx);
        
        /* lud_cache[obj], lud_cache[addr] = addr, obj */
        lua_pushvalue(L, ObjIdx); lua_pushvalue(L, AddrIdx); lua_settable(L, CacheIdx);
        lua_pushvalue(L, AddrIdx); lua_pushvalue(L, ObjIdx); lua_settable(L, CacheIdx);
    };
    
    
    
    lua_pushvalue(L, ObjIdx);
    lua_replace(L, OrigTop+1);
    lua_settop(L, OrigTop+1);
    
    return;
};
#endif

/* WRAPPROGRESS
** FUNCTION: lua_pushthread
** STATUS: COMPLETE
*/
LUA_API int   (lua_pushthread) (lua_State *L) {
    luaL_error(L, "In library '%s': Vanillin lacks the capability to push threads-on-the-stack from states!", OrigRequireName);
    
    return 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_gettable
** STATUS: COMPLETE
*/
LUA_API void  (lua_gettable) (lua_State *L, int idx) {
    Prep;
    
    if (lua_gettop(L) < 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_gettable()!", OrigRequireName);
        return;
    };
    
    lua_pushvalue(L, idx);
    lua_insert(L, -2);
    I->GetTable(-2);
    lua_remove(L, -2);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_getfield
** STATUS: COMPLETE
*/
LUA_API void  (lua_getfield) (lua_State *L, int idx, const char *k) {
    Prep;
    
    lua_pushvalue(L, idx);
    I->GetField(-1, k);
    lua_remove(L, -2);
    
    return ;
};

/* WRAPPROGRESS
** FUNCTION: lua_rawget
** STATUS: COMPLETE
*/
LUA_API void  (lua_rawget) (lua_State *L, int idx) {
    Prep;
    
    if (lua_gettop(L) < 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_rawget()!", OrigRequireName);
        return;
    };
    
    lua_pushvalue(L, idx);
    lua_insert(L, -2);
    I->RawGet(-2);
    lua_remove(L, -2);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_rawgeti
** STATUS: COMPLETE
*/
LUA_API void  (lua_rawgeti) (lua_State *L, int idx, int n) {
    Prep;
    
    lua_pushvalue(L, idx);
    lua_pushinteger(L, n);
    I->RawGet(-2);
    lua_remove(L, -2);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_createtable
** STATUS: COMPLETE
    It should be noted, that lua_createtable()'s second and third arguments are merely recommendations. 
*/
LUA_API void  (lua_createtable) (lua_State *L, int narr, int nrec) {
    Prep;
    
    I->CreateTable();
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_newuserdata
** STATUS: COMPLETE
    Unfortunately, we have to go back to doing some of the metatable madness that we had to do in Old
Vanillin, because Garry decided to camelcase the typename for full userdata. Normally, you should get this:

        type(full_userdata) --> "userdata"
    
    But currently, in Garry's Mod, you get this:
    
        type(full_userdata) --> "UserData"
    
    Since Vanillin strives for correctness, and it's not unimaginable that existing scripts would test for
this, we have to *make* Garry's Mod report "userdata".
    As in GMod 12, GMod 13 reports the type of a userdata by checking the "MetaName" field in its metatable.
Fortunately, GMod 13 *does* allocate and free the memory for us, so we don't have to do the "Anchor Userdata"
bookkeeping that we used to do (though we do still have to keep track of allocation size). There are a number
of other minor differences, so rather than list them, I'll just list what we have to do with userdata now:
    1. When a full userdata is created, it is initially given a default metatable containing the following
    fields:
        * MetaName    = "userdata"
        * __metatable = false
     The latter makes it so that "not getmetatable(full_userdata)" will return 'true'.
    2. The userdata is stored in a weak-key table with its size as a value. This table will also be used
     to test if the userdata is a Vanillin-created userdata.
    3. When lua_getmetatable() is given a userdata whose metatable is the same as that metatable, it will
     return 0 and push nothing on the stack, the same as if the object had no metatable to begin with.
    4. When lua_setmetatable() is given a userdata with that metatable, and applying an actual table, then
     it will create or overwrite the key "MetaName" in that table with a value of "userdata".
    5. When lua_setmetatable() is given a userdata with that metatable, and applying a nil, it will instead
     give that userdata the same table it started off with.
*/
#if 0
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz) {
    Prep;
    
    void* block = NULL;
    GarrysMod::Lua::UserData* block_struct = NULL;
    int block_idx = 0;
  while(1) {
    //void* block = I->NewUserdata((unsigned int)sz);
    block = I->NewUserdata((unsigned int)sz);
    block_idx = lua_gettop(L);
    block_struct = (GarrysMod::Lua::UserData*)block;
    if (block == NULL) {
        DbgMsg("Requested Full Userdata, retrieved one with block address of NULL!");
        lua_pop(L, 1);
    } else {
        break;
    };
  };
    
    if (block_struct->data == NULL) {
        DbgMsg("Full Userdata structure returned has a NULL ->data field");
    };
    
    lua_getglobal(L, "print");
    lua_pushfstring(L, "block_struct->type is %d, Garry-type is %d", (int)block_struct->type, I->GetType(block_idx));
    lua_call(L, 1, 0);
    
    /* Record the size */
    lua_getfield(L, LUA_REGISTRYINDEX, guidFUDSizes);
    lua_pushvalue(L, -2);
    lua_pushinteger(L, (lua_Integer)sz);
    lua_settable(L, -3);
    
    /* Set the metatable (can't use lua_setmetatable() for this) */
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "setmetatable");
    lua_pushvalue(L, -4); /* The FUD */
    lua_getfield(L, LUA_REGISTRYINDEX, guidFUDMeta);
    lua_call(L, 2, 0);
    
    lua_pop(L, 2);
    
    return block;
};
#elif 0
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz) {
    Prep;
    
    GarrysMod::Lua::UserData* block = NULL;
    I->PushNil();
    do { /* block->data is sometimes NULL, keep trying until we get a non-NULL one. */
        I->Pop(1);
        block = (GarrysMod::Lua::UserData*)I->NewUserdata((unsigned int)sz);
    } while (block->data == NULL);
    
    block->type = fudID;
    
    /* Record the size */
    lua_getfield(L, LUA_REGISTRYINDEX, guidFUDSizes);
    lua_pushvalue(L, -2);
    lua_pushinteger(L, (lua_Integer)sz);
    lua_settable(L, -3);
    
    /* Set the metatable (can't use lua_setmetatable() for this) */
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "setmetatable");
    lua_pushvalue(L, -4); /* The FUD */
    lua_getfield(L, LUA_REGISTRYINDEX, guidFUDMeta);
    lua_call(L, 2, 0);
    
    lua_pop(L, 2);
    
    return block->data;
};
#else
/*  Well, boys and girls, we've had to return to the Anchor system from GMod 12 Vanillin! :| Fuuuunnnn...
    For those of you who weren't around during the stone age of the first half of 2012, the way this system
works goes like this:
    First off, it's centered around Vanillin management of memory allocation and deallocation, because
the GMod API can't be trusted to do it on its own (and in GMod 12's API, it *couldnt'*, because userdata were
solely void* containers, not arbitrary-binary-data containers. While they are now in GMod 13, as of about
a week ago, the API now rigidly relies on a strict interpretation of them as containing a
GarrysMod::Lua::Userdata struct, whether it actually is or not (or even if it's smaller than such a struct)
    Now...the naive way to have such a setup, is for the actual userdata you request to be void*-sized (or,
now, userdata-struct-sized), containing only a pointer to the actual memory you've allocated. The userdata
would have a metatable attached whose __gc metamethod will free that memory when the userdata is garbage-
collected.
    The problem with this is that most full userdata get their own metatables attached on top, this is
entirely appropriate. So what we do is allocate *two* userdata. Both get the same content, a pointer to the
actual block of memory, allocated by HeapAlloc(). The first userdata, which I'll refer to as the 'front'
userdata, is left on the stack, with a default metatable attached to it that has special significance for
Vanillin (between the metatable and Vanillin, the 'front' userdata will not actually appear to have any
metatable at all, and additionally, it will cause type(front) to return "userdata" instead of "UserData",
making it compatible with normal Lua code.) The second userdata, referred to as the 'anchor', is stored in
precisely ONE place...as a key in a weak-key table, whose value is the 'front' userdata. The 'anchor' has
a metatable whose __gc metamethod will call HeapFree() on the allocated memory when the 'anchor' is collected,
and due to the relationship between the 'front' and the 'anchor' in the weak-key table, the 'anchor' will not
be collected before the 'front', and its lifetime will not extend significantly past that of the 'front'. */
LUA_API void *(lua_newuserdata) (lua_State *L, size_t sz) {
    Prep;
    
    GarrysMod::Lua::UserData* front  = NULL;
    GarrysMod::Lua::UserData* anchor = NULL;
    
    void* block = NULL;
    
    /* The Lua function which will manage our "prep" work once we have the userdatas and memory block */
    lua_getfield(L, LUA_REGISTRYINDEX, guidUDPrepFunc);
    
    I->PushNil();
    do { /* Not taking any more chances with what GMod returns. */
        I->Pop(1);
        front = (GarrysMod::Lua::UserData*)I->NewUserdata(sizeof(GarrysMod::Lua::UserData));
    } while (front == NULL);
    
    I->PushNil();
    do {
        I->Pop(1);
        anchor = (GarrysMod::Lua::UserData*)I->NewUserdata(sizeof(GarrysMod::Lua::UserData));
    } while (anchor == NULL);
    
    front->type  = fudID;
    anchor->type = anchID;
    
    block = HeapAlloc(
        ThisHeap,
        0,
        sz);
    
    if (block == NULL) {
        luaL_error(L, "memory allocation error: not enough memory, or default process heap is corrupt");
        return NULL;
    };
    
    front->data  = block;
    anchor->data = block;
    
    lua_pushnumber(L, (lua_Number)sz);
    lua_pushfstring(L, "%p", block);
    lua_call(L, 4, 1); /* Leaves the 'front' userdata on the stack. */
    
    return block;
};
#endif

/* WRAPPROGRESS
** FUNCTION: lua_getmetatable
** STATUS: COMPLETE
*/
LUA_API int   (lua_getmetatable) (lua_State *L, int objindex) {
    Prep;
    
    int aidx;
    int OrigTop = lua_gettop(L);
    
    switch (IdxClassification(L, objindex, &aidx, NULL)) {
        case VanIdx_Nonsensical :
        case VanIdx_AboveStack :
            return 0;
        case VanIdx_Pseudo_Upv :
            if (!CheckUpv(L, UpvalueIdxToOrdinal(L, objindex))) {
                return 0;
            } else {
                break;
            };
    };
    
    lua_pushvalue(L, aidx);
    if (!I->GetMetaTable(-1)) {
        lua_settop(L, OrigTop);
        return 0;
    } else {
        lua_getfield(L, LUA_REGISTRYINDEX, guidFUDMeta);
        if (lua_rawequal(L, -1, -2)) { /* It's a naked full userdata */
            lua_settop(L, OrigTop);
            return 0;
        } else {
            lua_remove(L, -1); /* _R[guidFUDMeta] */
            lua_remove(L, -2); /* Object */
            return 1;
        };
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_getfenv
** STATUS: COMPLETE
*/
LUA_API void  (lua_getfenv) (lua_State *L, int idx) {
    int FinalTop = lua_gettop(L) + 1;
    int aidx = lua_absindex(L, idx);
    
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "getfenv");
    lua_pushvalue(L, aidx);
    lua_call(L, 1, 1);
    lua_replace(L, FinalTop);
    lua_settop(L, FinalTop);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_settable
** STATUS: COMPLETE
*/
LUA_API void  (lua_settable) (lua_State *L, int idx) {
    Prep;
    
    if (lua_gettop(L) <= 2) {
        luaL_error(L, "In library '%s': there must be at least two values on stack to call lua_settable()!", OrigRequireName);
        return;
    };
    
    lua_pushvalue(L, idx);
    lua_insert(L, -3);
    I->SetTable(-3);
    lua_pop(L, 1);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_setfield
** STATUS: COMPLETE
*/
LUA_API void  (lua_setfield) (lua_State *L, int idx, const char *k) {
    Prep;
    
    if (lua_gettop(L) <= 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_setfield()!", OrigRequireName);
        return;
    };
    
    lua_pushvalue(L, idx);
    lua_insert(L, -2);
    I->SetField(-2, k);
    lua_pop(L, 1);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_rawset
** STATUS: COMPLETE
*/
LUA_API void  (lua_rawset) (lua_State *L, int idx) {
    Prep;
    
    if (lua_gettop(L) <= 2) {
        luaL_error(L, "In library '%s': there must be at least two values on stack to call lua_rawset()!", OrigRequireName);
        return;
    };
    
    lua_pushvalue(L, idx);
    lua_insert(L, -3);
    I->RawSet(-3);
    lua_pop(L, 1);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_rawseti
** STATUS: COMPLETE
*/
LUA_API void  (lua_rawseti) (lua_State *L, int idx, int n) {
    Prep;
    
    if (lua_gettop(L) < 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_rawseti()!", OrigRequireName);
        return;
    };
    
    lua_pushvalue(L, idx);
    lua_pushinteger(L, n);
    lua_pushvalue(L, -3);
    I->RawSet(-3);
    lua_pop(L, 2);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_setmetatable
** STATUS: COMPLETE
*/
LUA_API int   (lua_setmetatable) (lua_State *L, int objindex) {
    int aidx = lua_absindex(L, objindex);
    
    if (lua_gettop(L) < 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_setmetatable()!", OrigRequireName);
        return 0; /* Meaningless, we just have to give the compiler something to return. */
    };
    
    lua_getfield(L, LUA_REGISTRYINDEX, guidSetMTFunc);
    lua_pushvalue(L, aidx);
    lua_pushvalue(L, -3);
    lua_remove(L, -4);
    lua_call(L, 2, 0);
    
    return 1; /* Lua 5.1.4 sources show this always returns 1 */
};

/* WRAPPROGRESS
** FUNCTION: lua_setfenv
** STATUS: COMPLETE
*/
LUA_API int   (lua_setfenv) (lua_State *L, int idx) {
    int tblidx = lua_gettop(L);
    int aidx = lua_absindex(L, idx);
    int retval;
    
    if (lua_gettop(L) < 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_setfenv()!", OrigRequireName);
        return 0; /* Meaningless, we just have to give the compiler something to return. */
    };
    
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "setfenv");
    lua_pushvalue(L, aidx);
    lua_pushvalue(L, tblidx);
    lua_call(L, 2, 1);
    retval = lua_toboolean(L, -1);
    lua_settop(L, tblidx-1);
    
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_call
** STATUS: COMPLETE
*/
LUA_API void  (lua_call) (lua_State *L, int nargs, int nresults) {
    Prep;
    
    if (nargs < 0) {
        luaL_error(L, "In library '%s': called lua_call() with %d arguments, that makes no sense!", OrigRequireName, nargs);
        return;
    };
    
    if ((nresults < 0) && (nresults != LUA_MULTRET)) {
        luaL_error(L, "In library '%s': called lua_call() requesting %d results, that makes no sense!", OrigRequireName, nresults);
        return;
    };
    
    if (lua_gettop(L) < (nargs+1)) {
        luaL_error(L, "In library '%s': called lua_call() with %d arguments, not enough values on stack!", OrigRequireName, nargs);
        return;
    };
    
    I->Call(nargs, nresults);
    
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_pcall
** STATUS: COMPLETE
    Unlike the normal Lua 5.1 implementation, ours can handle using a pseudoindex for 'errfunc'.
*/
LUA_API int   (lua_pcall) (lua_State *L, int nargs, int nresults, int errfunc) {
    Prep;
    
    int retval;
    int errfuncidx;
    int toremoveidx = lua_absindex(L, -(nargs+1));
    
    if (nargs < 0) {
        luaL_error(L, "In library '%s': called lua_pcall() with %d arguments, that makes no sense!", OrigRequireName, nargs);
        return 0; /* Meaningless, we just have to give the compiler something to return. */
    };
    
    if ((nresults < 0) && (nresults != LUA_MULTRET)) {
        luaL_error(L, "In library '%s': called lua_pcall() requesting %d results, that makes no sense!", OrigRequireName, nresults);
        return 0; /* See above */
    };
    
    if (lua_gettop(L) < (nargs+1)) {
        luaL_error(L, "In library '%s': called lua_pcall() with %d arguments, not enough values on stack!", OrigRequireName, nargs);
        return 0; /* See above */
    };
    
    if (errfunc != 0) {
        lua_pushvalue(L, errfunc);
        errfuncidx = toremoveidx;
    } else {
        lua_pushnil(L); /* Just to make things easier on us. */
        errfuncidx = 0;
    };
    lua_insert(L, toremoveidx);
    
    retval = I->PCall(nargs, nresults, errfuncidx); /* I->PCall() does return the same values as lua_pcall() */
    lua_remove(L, toremoveidx);
    
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_cpcall
** STATUS: COMPLETE
*/
LUA_API int   (lua_cpcall) (lua_State *L, lua_CFunction func, void *ud) {
    lua_pushcfunction(L, func);
    lua_pushlightuserdata(L, ud);
    return lua_pcall(L, 1, 0, 0);
};

/* WRAPPROGRESS
** FUNCTION: lua_load
** STATUS: COMPLETE
*/
LUA_API int   (lua_load) (lua_State *L, lua_Reader reader, void *dt,
                                        const char *chunkname) {
    lua_getglobal(L, "CompileString");
   {int blocks = 0;
    while (1) {
        size_t blocksz;
        const char* block = reader(L, dt, &blocksz);
        
        if ((block==NULL) || (blocksz==0)) {
            break;
        };
        
        blocks += 1;
        lua_pushlstring(L, block, blocksz);
    };
    lua_concat(L, blocks);
    };
    
    lua_pushstring(L, (chunkname==NULL) ? "" : chunkname);
    lua_pushboolean(L, FALSE);
    lua_call(L, 3, 1);
    
    return (lua_isstring(L, -1)) ? LUA_ERRSYNTAX : 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_dump
** STATUS: COMPLETE
*/
LUA_API int (lua_dump) (lua_State *L, lua_Writer writer, void *data) {
    luaL_error(L, VanillinCannotImplementError, OrigRequireName, __FUNCTION__);
    return 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_yield
** STATUS: COMPLETE
*/
LUA_API int  (lua_yield) (lua_State *L, int nresults) {
    luaL_error(L, "Vanillin lacks the capability to yield its own thread!");
    return 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_resume
** STATUS: COMPLETE
*/
LUA_API int  (lua_resume) (lua_State *L, int narg) {
    luaL_error(L, "Vanillin lacks the capability to start threads!");
    return 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_status
** STATUS: COMPLETE
*/
LUA_API int  (lua_status) (lua_State *L) {
    luaL_error(L, "Vanillin lacks the capability to report thread status!");
    return 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_gc
** STATUS: COMPLETE
*/
LUA_API int (lua_gc) (lua_State *L, int what, int data) {
    lua_getglobal(L, "collectgarbage");
    
    switch (what) {
        case LUA_GCSTOP       :
            lua_pushliteral(L, "stop");
            lua_call(L, 1, 0);
            return 0;
        case LUA_GCRESTART    :
            lua_pushliteral(L, "restart");
            lua_call(L, 1, 0);
            return 0;
        case LUA_GCCOLLECT    :
            lua_pushliteral(L, "collect");
            lua_call(L, 1, 0);
            return 0;
        case LUA_GCCOUNT      : /* This returns the number of kilobytes of memory being used. */
           {int kbCount;
            lua_pushliteral(L, "count");
            lua_call(L, 1, 1);
            kbCount = lua_tointeger(L, -1);
            lua_pop(L, 1);
            return kbCount;};
        case LUA_GCCOUNTB     : /* This returns the remainder-of((number of *bytes* being used) / 1024) */
           {double retval;
            lua_pushliteral(L, "count");
            lua_call(L, 1, 1);
            retval = lua_tonumber(L, -1); /* Number of kilobytes being used */
            retval *= 1024; /* Number of bytes being used */
            retval = fmod(retval, 1024);
            lua_pop(L, 1);
            return retval;};
        case LUA_GCSTEP       :
           {int cycle_finished;
            lua_pushliteral(L, "step");
            lua_pushinteger(L, data);
            lua_call(L, 2, 1);
            cycle_finished = lua_toboolean(L, -1);
            lua_pop(L, 1);
            return cycle_finished;};
        case LUA_GCSETPAUSE   :
           {int prev_val;
            lua_pushliteral(L, "setpause");
            lua_pushinteger(L, data);
            lua_call(L, 2, 1);
            prev_val = lua_tointeger(L, -1);
            lua_pop(L, 1);
            return prev_val;};
        case LUA_GCSETSTEPMUL :
           {int prev_val;
            lua_pushliteral(L, "setstepmul");
            lua_pushinteger(L, data);
            lua_call(L, 2, 1);
            prev_val = lua_tointeger(L, -1);
            lua_pop(L, 1);
            return prev_val;};
        default :
            lua_pop(L, 1);
            return -1; /* This is what the 5.1.4 API does */
    }
};

/* WRAPPROGRESS
** FUNCTION: lua_error
** STATUS: COMPLETE
    Confirmed to work with non-string error objects.
*/
LUA_API int   (lua_error) (lua_State *L) {
    if (lua_gettop(L) == 0) {
        luaL_error(L, "In library '%s': attempted to call lua_error() with no error object/message!", OrigRequireName);
    } else {
        lua_getglobal(L, "error");
        lua_insert(L, -2);
        lua_pushinteger(L, 0);
        lua_call(L, 2, 0);
    };
    
    return 0;
};

/* WRAPPROGRESS
** FUNCTION: lua_next
** STATUS: COMPLETE
*/
LUA_API int   (lua_next) (lua_State *L, int idx) {
    Prep;
    
    int retval;
    int tblidx;
    
    if (lua_gettop(L) < 1) {
        luaL_error(L, "In library '%s': there must be at least one value on stack to call lua_next()!", OrigRequireName);
        return 0;
    };
    
    lua_pushvalue(L, idx);
    lua_insert(L, -2);
    tblidx = lua_absindex(L, -2);
    retval = I->Next(-2);
    lua_remove(L, tblidx);
    
    return retval;
};

/* WRAPPROGRESS
** FUNCTION: lua_concat
** STATUS: COMPLETE
*/
LUA_API void  (lua_concat) (lua_State *L, int n) {
    int OrigTop = lua_gettop(L);
    
    if (n < 0) {
        luaL_error(L, "In library '%s': called function lua_concat(), requested concatenation of %d values, must pass 0 or more!",
            OrigRequireName, n);
        return;
    } else if (lua_gettop(L) < n) {
        luaL_error(L, "In library '%s': called function lua_concat(), requested concatenation of %d values, only %d values exist on stack!",
            OrigRequireName, n, OrigTop);
        return ;
    } else if (n >= 2) {
        lua_getfield(L, LUA_REGISTRYINDEX, guidConcatFunc);
        int pushidx = -(n+1);
       {int cnt;
        for (cnt = 1; cnt <= n; cnt += 1) {
            lua_pushvalue(L, pushidx);
        };};
        lua_call(L, n, 1);
        lua_insert(L, pushidx);
        lua_pop(L, n);
        
        return;
    } else if (n == 1) {
        return; /* It does nothing. This is the behavior of the 5.1.4 C API */
    } else if (n == 0) {
        lua_pushliteral(L, ""); /* This is the actual behavior of the 5.1.4 C API */
        return;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_getallocf
** STATUS: COMPLETE
*/
LUA_API lua_Alloc (lua_getallocf) (lua_State *L, void **ud) {
    luaL_error(L, "Vanillin lacks the capability to retrieve a state's memory-allocation function!");
    return NULL;
};

/* WRAPPROGRESS
** FUNCTION: lua_setallocf
** STATUS: COMPLETE
*/
LUA_API void lua_setallocf (lua_State *L, lua_Alloc f, void *ud) {
    luaL_error(L, "Vanillin lacks the capability to set a state's memory-allocation function!");
    return;
};

/* WRAPPROGRESS
** FUNCTION: lua_getstack
** STATUS: COMPLETE --Keep an eye on it for further testing
*/
/*  The design of this function and lua_getinfo() is a bit complex...I'm documenting it here so that I
don't forget it later, and so that others can figure it out after me.
    lua_getinfo() is used to retrieve info about functions, and it roughly corresponds to debug.getinfo().
Like debug.getinfo(), you can explicitly pass it a function to get info about, or a number representing
the execution level.
    In order to get info about a *specific* function, you only have to call lua_getinfo(). You push the
function you want info about onto the stack and make sure your 'where' string starts with '>', then it
fills in the lua_Debug structure you passed.
    But in order to get info about a particular stack level, you have to create a lua_Debug, pass it to
lua_getstack() and indicate which execution level you want info on, which will fill it with a bit of info,
and *then* you pass that to lua_getinfo().
    The problem is that the lua_Debug structure can technically be passed around anywhere and you can get
info from it, even if you've changed the execution level at some point. Because of upvalues, you can't
just stick the function into the lua_Debug structure itself, so here's the solution I've come up with.
    lua_Debug has two fields that aren't used by users, and they're both ints: lua_Debug.event and
lua_Debug.i_ci.
    I want to be able to get info on the stored execution level anywhere, but I don't want to cause the
functions to be anchored into existence when nobody is using them. So here's the solution I came up with;
it doesn't allow you to get info on an execution stack level that doesn't exist anymore, but that is fine
with me.
    There's (yet another) registry table, under guidLGSRefs, with weak values. We use luaL_ref to store
the value into the table and get a unique integer key for it, and store that key into lua_Debug.i_ci, which
we can use to retrieve the function later. */
LUA_API int lua_getstack (lua_State *L, int level, lua_Debug *ar) {
    int orig_top = lua_gettop(L);
    
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "getinfo");
    lua_remove(L, -2);
    lua_pushinteger(L, level+1);
    lua_pushliteral(L, "f");
    lua_call(L, 2, 1);
    
    if (lua_isnil(L, -1)) {
        lua_settop(L, orig_top);
        
        return 0;
    } else {
        lua_getfield(L, LUA_REGISTRYINDEX, guidLGSRefs);
        lua_getfield(L, -2, "func");
        ar->i_ci = luaL_ref(L, -2);
        lua_settop(L, orig_top);
        
        return 1;
    };
};

/* WRAPPROGRESS
** FUNCTION: lua_getinfo
** STATUS: COMPLETE --Keep an eye on it for further testing
*/
LUA_API int lua_getinfo (lua_State *L, const char *what, lua_Debug *ar) {
    int status = 1;
    BOOL found_L = FALSE;
    BOOL found_f = FALSE;
    
    /* Initialize ar since we seem to be having so many problems */
    ar->name     = "lua_getinfo_default_initial_value";
    ar->namewhat = "lua_getinfo_default_initial_value";
    ar->what     = "lua_getinfo_default_initial_value";
    ar->source   = "lua_getinfo_default_initial_value";
    ar->currentline     = 0;
    ar->nups            = 0;
    ar->linedefined     = 0;
    ar->lastlinedefined = 0;
    ar->short_src[0]    = 0;
    
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "getinfo");
    lua_remove(L, -2);
    
    if (what[0] == '>') { /* Swap debug.getinfo and the function under it */
        lua_pushvalue(L, -2);
        lua_remove(L, -3);
        what += 1;
    } else { /* Push _R[guidLGSRefs][ar->i_ci] */
        lua_getfield(L, LUA_REGISTRYINDEX, guidLGSRefs);
        lua_rawgeti(L, -1, ar->i_ci);
        lua_remove(L, -2);
    };
    
    lua_pushliteral(L, "nSlufL");
    lua_call(L, 2, 1);
    
    int tbl_idx = lua_absindex(L, -1);
    for (; *what; what++) {
        switch(*what) {
            case 'n' :
                lua_getfield(L, tbl_idx, "name");
                ar->name = (lua_isnil(L, -1)) ? NULL : lua_tostring(L, -1);
                lua_getfield(L, tbl_idx, "namewhat");
                ar->namewhat = lua_tostring(L, -1);
                break;
            case 'S' :
                lua_getfield(L, tbl_idx, "source");
                ar->source = lua_tostring(L, -1);
                lua_getfield(L, tbl_idx, "short_src");
                lstrcpynA(ar->short_src, lua_tostring(L, -1), LUA_IDSIZE);
                lua_getfield(L, tbl_idx, "linedefined");
                ar->linedefined = lua_tointeger(L, -1);
                lua_getfield(L, tbl_idx, "lastlinedefined");
                ar->lastlinedefined = lua_tointeger(L, -1);
                lua_getfield(L, tbl_idx, "what");
                ar->what = lua_tostring(L, -1);
                break;
            case 'l' :
                lua_getfield(L, tbl_idx, "currentline");
                ar->currentline = lua_tointeger(L, -1);
                break;
            case 'u' :
                /* Let's see if this is a Vanillin C function. */
                lua_getfield(L, LUA_REGISTRYINDEX, guidUpvalues);
                lua_getfield(L, tbl_idx, "func");
                lua_gettable(L, -2);
                if (lua_isnil(L, -1)) { /* Not a Vanillin function */
                    lua_getfield(L, tbl_idx, "nups");
                    ar->nups = lua_tointeger(L, -1);
                } else {
                    lua_getfield(L, -1, "n");
                    ar->nups = lua_tointeger(L, -1);
                };
                break;
            case 'L' :
                found_L = TRUE;
                break;
            case 'f' :
                found_f = TRUE;
                break;
            default :
                status = 0;
                break;
        };
    };
    
    lua_settop(L, tbl_idx);
    
    if (found_f) {
        lua_getfield(L, tbl_idx, "func");
    };
    if (found_L) {
        lua_getfield(L, tbl_idx, "activelines");
    };
    
    lua_remove(L, tbl_idx);
    
    return status;
};


/* Various utility functions used throughout. */

/* Pass it the idx you want to classify, optionally a pointer to an int which will receive the "absolute" index, and optionally
a pointer to a structure (whose layout and meaning have not been made yet) which will give some instructions on actions to
automatically take as regards error messages for various classes of index. */
static enum VanillinIdxClass IdxClassification(lua_State *L, int idx, int* aidx, void* StructPtr) {
    /* Going to be using that fourth parameter for a structure to automate error-handling in a bit. */
    enum VanillinIdxClass retval;
    int raidx = idx;
    int top = lua_gettop(L);
    
    if (idx > 0) { /* Absolute index */
        raidx = idx;
        retval = (idx > top) ? VanIdx_AboveStack : VanIdx_OnStack;
    } else if ((idx <= LUA_REGISTRYINDEX) && (idx >= lua_upvalueindex(255))) { /* Pseudoindex */
        raidx = idx;
        retval = (idx <= lua_upvalueindex(1)) ? VanIdx_Pseudo_Upv : VanIdx_Pseudo_Tbl;
    } else if ((idx < 0) && (idx >= -top)) { /* Valid relative index */
        raidx = top + idx + 1;
        retval = VanIdx_OnStack;
    } else { /* 0, or invalid relative index */
        raidx = 0;
        retval = VanIdx_Nonsensical;
    };
    
    if (aidx != NULL) {
        *aidx = raidx;
    };
    
    return retval;
};

static void CompileAndPush(lua_State *L, const char* src, const char* name) {
    lua_getglobal(L, "CompileString");
    lua_pushstring(L, src);
    lua_pushstring(L, name);
    lua_pushboolean(L, FALSE);
    lua_call(L, 3, 1);
    
    return;
};

static int lua_absindex(lua_State *L, int idx) {
    if ((idx >= 0) || (idx <= LUA_REGISTRYINDEX)) {
        return idx;
    } else {
        return lua_gettop(L) + 1 + idx;
    };
};

static void BadIdxError(lua_State *L, const char* funcname, int idx) {
    switch (IdxClassification(L, idx, NULL, NULL)) {
        case VanIdx_Nonsensical :
            luaL_error(L, "In library '%s': called function %s(), bad index '%d' (nonsensical), stack top: '%d'",
                OrigRequireName, funcname, idx, lua_gettop(L));
            return;
        case VanIdx_OnStack :
            luaL_error(L, "In library '%s': called function %s(), bad index '%d', stack top: '%d'",
                OrigRequireName, funcname, idx, lua_gettop(L));
            return;
        case VanIdx_AboveStack :
            luaL_error(L, "In library '%s': called function %s(), bad index '%d' (above stack), stack top: '%d'",
                OrigRequireName, funcname, idx, lua_gettop(L));
            return;
        case VanIdx_Pseudo_Tbl :
           {const char* idxname;
            switch (idx) {
                case LUA_REGISTRYINDEX :
                    idxname = "LUA_REGISTRYINDEX"; break;
                case LUA_ENVIRONINDEX  :
                    idxname = "LUA_ENVIRONINDEX";  break;
                case LUA_GLOBALSINDEX  :
                    idxname = "LUA_GLOBALSINDEX";  break;};
            luaL_error(L, "In library '%s': called function %s(), bad index '%s'",
                OrigRequireName, funcname, idxname);};
            return;
        case VanIdx_Pseudo_Upv :
            luaL_error(L, "In library '%s': called function %s(), bad index 'lua_upvalueindex(%d)'",
                OrigRequireName, funcname, UpvalueIdxToOrdinal(L, idx));
            return;
    };
};

static int UpvalueIdxToOrdinal(lua_State *L, int idx) {
    return (idx * -1) + LUA_GLOBALSINDEX;
};

/* TODO: Make this use the API debug functions. */
static void PushSelf(lua_State *L) {
    lua_getfield(L, LUA_GLOBALSINDEX, "debug");
    lua_getfield(L, -1, "getinfo");
    lua_pushinteger(L, 1);
    lua_pushliteral(L, "f");
    lua_call(L, 2, 1);
    lua_getfield(L, -1, "func");
    lua_insert(L, -3);
    lua_pop(L, 2);
    
    return;
};

/* Returns true or false based on whether the given upvalue exists in the current function */
static int CheckUpv(lua_State *L, int ordUpv) {
    int OrigTop = lua_gettop(L);
    lua_getfield(L, LUA_REGISTRYINDEX, guidUpvalues);
    PushSelf(L);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) {
        lua_settop(L, OrigTop);
        return FALSE;
    };
    lua_getfield(L, -1, "n");
    if (ordUpv > lua_tointeger(L, -1)) {
        lua_settop(L, OrigTop);
        return FALSE;
    } else {
        lua_settop(L, OrigTop);
        return TRUE;
    };
};

static void StackDump(lua_State *L, int linnum) { /* Rewrite this when lua_concat() is finished */
    int StackTop = lua_gettop(L);
    int Index = 1;
    
    lua_getglobal(L, "print");
    lua_pushfstring(L, "Dump of stack at line %d: (%d items)", linnum, lua_gettop(L));
    lua_call(L, 1, 0);
    
    while (Index <= StackTop) {
        const char* tostring;
        lua_getglobal(L, "tostring"); lua_pushvalue(L, Index); lua_call(L, 1, 1); tostring = lua_tostring(L, -1); lua_pop(L, 1);
        lua_getglobal(L, "print");
        lua_pushfstring(L, "[%d] (%s) %s", Index, lua_typename(L, lua_type(L, Index)), tostring);
        lua_call(L, 1, 0);
        
        Index += 1;
    };
    
    lua_getglobal(L, "print");
    lua_pushliteral(L, "");
    lua_call(L, 1, 0);
    
    return;
};

static int ZeroFUD(lua_State *L) {
    Prep;
    
    I->NewUserdata(0);
    
    return 1;
};

static int UserdataGC(lua_State* L) {
    void* block = lua_touserdata(L, 1);
    HeapFree(ThisHeap, 0, block);
    
    return 0;
};

#if 0
/* This is just here temporarily to satisfy our linker.*/
LUALIB_API int luaL_error (lua_State *L, const char *fmt, ...) {
    va_list argp;
    va_start(argp, fmt);
    lua_pushvfstring(L, fmt, argp);
    va_end(argp);
    return lua_error(L);
};
#endif