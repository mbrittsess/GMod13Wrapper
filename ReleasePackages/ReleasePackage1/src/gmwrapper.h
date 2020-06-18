/* Every single function here places "Prep;" at the beginning of each function.
It can prepare anything that needs to be done in every single function. */
#define Prep GarrysMod::Lua::ILuaBase* I = L->luabase

/* Global variables */
static const char* OrigRequireName = "UNINITIALIZED";
static HANDLE ThisHeap = NULL;

/* GUIDs Used */
static const char* guidUpvalues     = "{F56CF593-BBB6-4f54-8320-9B0942A85506}"; /* Version 1 */
static const char* guidPushfString  = "{CC38C278-8672-43a0-AFA4-9C2A90FC91B9}"; /* Version 1 */
static const char* guidLenFunc      = "{771DD06F-92A2-4e23-8EEC-317F7E17A3D8}"; /* Version 1 */
static const char* guidConcatFunc   = "{3C0C3DEF-DC94-4043-9073-4B21E10419A3}"; /* Version 1 */
static const char* guidLessThanFunc = "{9FFB88AB-5676-4345-85B8-B30EBB5C4CE2}"; /* Version 1 */
static const char* guidFUDSizes     = "{F5D2E50D-24B6-461c-8B20-A758EE8DC7C9}"; /* Version 1 */
static const char* guidFUDMeta      = "{16BBDBE2-B572-4d2e-AC4F-1D43FAAB1250}"; /* Version 1 */
static const char* guidSetMTFunc    = "{115965B1-94B1-41f9-9603-F62F37262D8D}"; /* Version 1 */
static const char* guidLUDCache     = "{0CC52835-A74E-4f3f-8C4A-0B41F6EE6AA3}"; /* Version 1 */
static const char* guidLUDMeta      = "{81410E3D-C338-4a6f-9EE1-1072A89D26BB}"; /* Version 1 */
static const char* guidToPtrFunc    = "{6E540BD9-836B-4419-998F-97D6E21295AA}"; /* Version 1 */
static const char* guidLGSRefs      = "{EF44B6B3-C150-4955-8337-5D57DEAB8303}"; /* Version 1 */
static const char* guidUDPrepFunc   = "{AD0D9949-2232-4505-B92C-7342216A6A8F}"; /* Version 1 */

/* Enumerations, constants */
enum VanillinIdxClass {
    VanIdx_Nonsensical,
    VanIdx_OnStack,
    VanIdx_AboveStack,
    VanIdx_Pseudo_Tbl,
    VanIdx_Pseudo_Upv
};

#define fudID  213
#define ludID  214
#define anchID 215

/* Texts for some Lua scripts */
static const char* LuaCombinedSrc = (""
    #include "LuaIncs\luacombined.txt"
    );

/* Various message strings */
static const char* VanillinCannotImplementError     = "In library '%s': called function %s(), Vanillin cannot implement it";
static const char* ImpossibleConditionEncountered   = "In library '%s': called function %s(), impossible condition encountered";
    
/* Prototypes for functions used */
static enum VanillinIdxClass IdxClassification(lua_State *L, int idx, int* aidx, void* StructPtr);
    /* That last parameter in IdxClassification might be used later */
static void CompileAndPush(lua_State *L, const char* src, const char* name);
static int  lua_absindex(lua_State *L, int idx);
static void BadIdxError(lua_State *L, const char* funcname, int idx);
static int  UpvalueIdxToOrdinal(lua_State *L, int idx);
static void PushSelf(lua_State *L);
static int  CheckUpv(lua_State *L, int ordUpv);
static void StackDump(lua_State *L, int linnum);
static int  ZeroFUD(lua_State *L);
static int  UserdataGC(lua_State* L);

/*
LUALIB_API int luaL_error (lua_State *L, const char *fmt, ...);
*/