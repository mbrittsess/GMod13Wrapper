#include <lua.h>

static void StackDump(lua_State *L, int linnum) { /* Rewrite this when lua_concat() is finished */
    int StackTop = lua_gettop(L);
    int Index = 1;
    
    lua_getglobal(L, "print");
    lua_pushfstring(L, "Dump of stack at line %d: (%d items)", linnum, lua_gettop(L)-1);
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

static void lua_print(lua_State *L, const char* msg) {
    lua_getglobal(L, "print");
    lua_pushstring(L, msg);
    lua_call(L, 1, 0);
    
    return;
};

static int specvals(lua_State *L) {
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_pushvalue(L, LUA_ENVIRONINDEX);
    lua_pushvalue(L, LUA_REGISTRYINDEX);
    
    return 3;
};

static int is_equal(lua_State *L) {
    lua_pushboolean(L, lua_equal(L, 1, 2));
    
    return 1;
};

static int is_rawequal(lua_State *L) {
    lua_pushboolean(L, lua_rawequal(L, 1, 2));
    
    return 1;
};

static int typeinfo(lua_State *L) {
    lua_pushinteger(L, lua_type(L, 1));
    lua_pushstring(L, lua_typename(L, lua_type(L, 1)));
    
    return 2;
};

static int to_boolean(lua_State *L) {
    lua_pushinteger(L, lua_toboolean(L, 1));
    
    return 1;
};

static int show_upvalues(lua_State *L) {
    lua_Integer ordUpv = lua_tointeger(L, 1);
    
    if ((ordUpv < 1) || (ordUpv > 255)) {
        luaL_error(L, "Invalid argument #1!");
    };
    
    if (lua_gettop(L)==1) {
        lua_pushvalue(L, lua_upvalueindex(ordUpv));
        return 1;
    } else {
        lua_replace(L, lua_upvalueindex(ordUpv));
        return 0;
    };
};

static int make_upvalues(lua_State *L) {
    lua_pushcclosure(L, show_upvalues, lua_gettop(L));
    
    return 1;
};

static int test_type(lua_State *L) {
    int idx = lua_tointeger(L, 1);
    lua_pushinteger(L, lua_type(L, idx));
    return 1;
};

static int tolstring_test(lua_State *L) {
    size_t len;
    const char* str = lua_tolstring(L, 1, &len);
    
    lua_settop(L, 1);
    lua_pushlstring(L, str, len);
    lua_getglobal(L, "type");
    lua_pushvalue(L, 1);
    lua_call(L, 1, 1);
    
    return 2;
};

static int gettable_stack_test(lua_State *L) { StackDump(L, __LINE__);
    lua_pushvalue(L, LUA_GLOBALSINDEX);
    lua_pushliteral(L, "_VERSION"); StackDump(L, __LINE__);
    lua_gettable(L, 1); StackDump(L, __LINE__);
    return 1;
};

static int rawget_test(lua_State *L) {
    lua_pushvalue(L, 2);
    lua_gettable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, 1);
    return 2;
};

static int rawgeti_test(lua_State *L) {
    int iKey = lua_tointeger(L, 2);
    lua_gettable(L, 1);
    lua_rawgeti(L, 1, iKey);
    return 2;
};

static int multret_test(lua_State *L) {
    lua_getglobal(L, "unpack");
    lua_insert(L, -2);
    lua_call(L, 1, LUA_MULTRET);
    return lua_gettop(L);
};

static int err_test(lua_State *L) {
    lua_newtable(L);
    lua_pushinteger(L, 12345);
    lua_setfield(L, 1, "code");
    return lua_error(L);
};

static int isnum(lua_State *L) {
    lua_pushboolean(L, lua_isnumber(L, 1));
    return 1;
};

static int iscfunc(lua_State *L) {
    int retbool = lua_iscfunction(L, 1);
    int stacktop = lua_gettop(L);
    
    lua_pushboolean(L, retbool);
    lua_pushinteger(L, stacktop);
    
    return 2;
};

static int anchor_expire(lua_State *L) {
    lua_getglobal(L, "print");
    lua_pushliteral(L, "Anchor expiring!");
    lua_call(L, 1, 0);
    
    return 0;
};

static int newanchor(lua_State *L) {
    lua_newuserdata(L, 0);
    lua_getglobal(L, "debug");
    lua_getfield(L, -1, "setmetatable");
    lua_pushvalue(L, 1);
    lua_newtable(L);
    lua_pushcfunction(L, anchor_expire);
    lua_setfield(L, -2, "__gc");
    lua_call(L, 2, 0);
    lua_settop(L, 1);
    
    return 1;
};

static int pokeupval(lua_State *L) { /* (function, ordinal|'n', value) */
    lua_settop(L, 3);
    lua_getfield(L, LUA_REGISTRYINDEX, "{F56CF593-BBB6-4f54-8320-9B0942A85506}");
    lua_pushvalue(L, 1);
    lua_gettable(L, -2);
    if (lua_isnil(L, -1)) { /* Make an upvalue table if there isn't one. */
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushinteger(L, 0);
        lua_setfield(L, -2, "n");
        lua_pushvalue(L, 1);
        lua_pushvalue(L, 5);
        lua_settable(L, 4);
    };
    if (lua_type(L, 2)==LUA_TSTRING) {
        lua_pushvalue(L, 3);
        lua_setfield(L, -2, "n");
    } else {
        lua_pushvalue(L, 2);
        lua_pushvalue(L, 3);
        lua_settable(L, -3);
    };
    
    return 0;
};

static int ltfunc(lua_State *L) {
    int idx1 = lua_tointeger(L, 1);
    int idx2 = lua_tointeger(L, 2);
    lua_pushboolean(L, lua_lessthan(L, idx1, idx2));
    return 1;
};

static int newfud(lua_State *L) {
    size_t szFUD = (size_t)lua_tointeger(L, 1);
    lua_newuserdata(L, szFUD);
    
    return 1;
};

static int lenfunc(lua_State *L) {
    lua_pushinteger(L, lua_objlen(L, 1));
    return 1;
};

static int getmt(lua_State *L) {
    int retval = lua_getmetatable(L, 1);
    lua_settop(L, 2);
    lua_pushboolean(L, retval);
    return 2;
};

static int setmt(lua_State *L) {
    lua_settop(L, 2);
    lua_setmetatable(L, 1);
    return 0;
};

static int newlud(lua_State *L) {
    uintptr_t val1 = (uintptr_t)lua_tonumber(L, 1);
    void* p = (void*)val1;
    lua_pushlightuserdata(L, p);
    return 1;
};

static int isfud(lua_State *L) {
    lua_pushboolean(L, lua_type(L, 1) == LUA_TUSERDATA);
    return 1;
};

static int islud(lua_State *L) {
    lua_pushboolean(L, lua_type(L, 1) == LUA_TLIGHTUSERDATA);
    return 1;
};

static int toud(lua_State *L) {
    lua_pushfstring(L, "%p", lua_touserdata(L, 1));
    return 1;
};

static int toptr(lua_State *L) {
    lua_pushfstring(L, "%p", lua_topointer(L, 1));
    return 1;
};

static int concat(lua_State *L) {
    lua_concat(L, lua_gettop(L));
    return 1;
};

static int next (lua_State *L) {
  lua_settop(L, 2);
  if (lua_next(L, 1))
    return 2;
  else {
    lua_pushnil(L);
    return 1;
  }
}

static int pcall_func(lua_State *L) { /* pcall_func(ShouldError) */
    if (lua_toboolean(L, 1)) {
        lua_pushliteral(L, "Test Error");
        return lua_error(L);
    } else {
        return 0;
    };
};

static int pcall(lua_State *L) { /* pcall(ShouldError[, ErrHandler]) */
    lua_settop(L, 2);
    lua_pushcfunction(L, pcall_func);
    lua_pushvalue(L, 1);
    lua_pushinteger(L, lua_pcall(L, 1, 0, (lua_isnil(L, 2)) ? 0 : 2));
    
    return 1;
};

static const char* load_worker(lua_State *L, void* data, size_t* size) {
    int count;
    const char* retval;
    
    lua_rawgeti(L, 1, 0);
    count = lua_tointeger(L, -1) + 1; lua_pop(L, 1);
    lua_pushinteger(L, count); lua_rawseti(L, 1, 0);
    if (count > lua_objlen(L, 1)) {
        return NULL;
    };
    
    lua_rawgeti(L, 1, count);
    lua_pushliteral(L, "\n"); lua_concat(L, 2);
    retval = lua_tolstring(L, -1, size);
    lua_pop(L, 1);
    
    return retval;
};

static int load_test(lua_State *L) { /* Pass it an array, it loads each value as a string portion of the funciton */
    lua_settop(L, 1);
    lua_pushfstring(L, "table: %p", lua_topointer(L, 1));
    
    lua_pushinteger(L, 0);
    lua_rawseti(L, 1, 0);
    lua_load(L, load_worker, NULL, lua_tostring(L, 2));
    
    return 1;
};

#if 0
// Function is now unsafe.
typedef struct {
    void* data;
    unsigned char type;
} fud_struct;

static int fud_verify(lua_State *L) {
    fud_struct* block;
    int first_type;
    int second_type;
    
    lua_settop(L, 0);
    block = (fud_struct*)lua_newuserdata(L, sizeof(fud_struct)); lua_print(L, "getting first type...");
    first_type = lua_type(L, 1);
    block->type = 18; lua_print(L, "getting second type...");
    second_type = lua_type(L, 1);
    lua_getglobal(L, "print");
    lua_pushfstring(L, "First type was given as %d (%s), second was given as %d (%s), struct pointer is %p, block pointer is %p",
        first_type, lua_typename(L, first_type), second_type, lua_typename(L, second_type), block, block->data);
    lua_call(L, 1, 0);
    lua_getglobal(L, "print");
    lua_pushfstring(L, "Value is evaluted as: %d", -(1)-1);
    lua_call(L, 1, 0);
    lua_settop(L, -(1)-1);
    return 0;
};
#endif

static int math_test(lua_State *L) {
    lua_getglobal(L, "print");
    lua_pushfstring(L, "Value evalutes as: %d %d %d", -(1)-1, -(2)-1, -(3)-1);
    lua_call(L, 1, 0);
    
    lua_pushnil(L);
    lua_pop(L, 1);
    
    return 0;
};

int luaopen_hello(lua_State* L) {
    lua_getglobal(L, "print");
    lua_pushliteral(L, "Hello, world!");
    lua_call(L, 1, 0);
    
    lua_newtable(L);
    lua_pushcfunction(L, specvals); lua_setfield(L, -2, "specvals");
    lua_pushcfunction(L, typeinfo); lua_setfield(L, -2, "typeinfo");
    lua_pushcfunction(L, is_equal); lua_setfield(L, -2, "is_equal");
    lua_pushcfunction(L, is_rawequal); lua_setfield(L, -2, "is_rawequal");
    lua_pushcfunction(L, to_boolean); lua_setfield(L, -2, "to_boolean");
    lua_pushcfunction(L, make_upvalues); lua_setfield(L, -2, "make_upvalues");
    lua_pushcfunction(L, test_type); lua_setfield(L, -2, "test_type");
    lua_pushcfunction(L, tolstring_test); lua_setfield(L, -2, "tolstring");
    lua_pushcfunction(L, gettable_stack_test); lua_setfield(L, -2, "gettable_stack_test");
    lua_pushcfunction(L, rawget_test); lua_setfield(L, -2, "rawget_test");
    lua_pushcfunction(L, rawgeti_test); lua_setfield(L, -2, "rawgeti_test");
    lua_pushcfunction(L, multret_test); lua_setfield(L, -2, "multret_test");
    lua_pushcfunction(L, err_test); lua_setfield(L, -2, "err_test");
    lua_pushcfunction(L, isnum); lua_setfield(L, -2, "isnum");
    lua_pushcfunction(L, iscfunc); lua_setfield(L, -2, "iscfunc");
    lua_pushcfunction(L, newanchor); lua_setfield(L, -2, "newanchor");
    lua_pushcfunction(L, pokeupval); lua_setfield(L, -2, "pokeupval");
    lua_pushcfunction(L, ltfunc); lua_setfield(L, -2, "ltfunc");
    lua_pushcfunction(L, newfud); lua_setfield(L, -2, "newfud");
    lua_pushcfunction(L, lenfunc); lua_setfield(L, -2, "lenfunc");
    lua_pushcfunction(L, getmt); lua_setfield(L, -2, "getmt");
    lua_pushcfunction(L, setmt); lua_setfield(L, -2, "setmt");
    lua_pushcfunction(L, newlud); lua_setfield(L, -2, "newlud");
    lua_pushcfunction(L, isfud); lua_setfield(L, -2, "isfud");
    lua_pushcfunction(L, islud); lua_setfield(L, -2, "islud");
    lua_pushcfunction(L, toud); lua_setfield(L, -2, "toud");
    lua_pushcfunction(L, toptr); lua_setfield(L, -2, "toptr");
    lua_pushcfunction(L, concat); lua_setfield(L, -2, "concat");
    lua_pushcfunction(L, next); lua_setfield(L, -2, "next");
    lua_pushcfunction(L, pcall); lua_setfield(L, -2, "pcall");
    lua_pushcfunction(L, load_test); lua_setfield(L, -2, "load_test");
    
  //lua_pushcfunction(L, fud_verify); lua_setfield(L, -2, "fud_verify");
    
    lua_pushcfunction(L, math_test); lua_setfield(L, -2, "math_test");
    
    return 1;
};