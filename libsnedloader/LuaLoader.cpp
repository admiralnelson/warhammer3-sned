#include "pch.h"
#include "LuaLoader.h"
#include "Debugger.h"

//#include "lrdb/server.hpp"


#define EXPORT extern "C" __declspec(dllexport)

/* environment variables that hold the search path for packages */
#define LUA_PATH	"LUA_PATH"
#define LUA_CPATH	"LUA_CPATH"

/* prefix for open functions in C libraries */
#define LUA_POF		"luaopen_"

/* separator for open functions in C libraries */
#define LUA_OFSEP	"_"


#define LIBPREFIX	"LOADLIB: "

#define POF		LUA_POF
#define LIB_FAIL	"open"


/* error codes for ll_loadfunc */
#define ERRLIB		1
#define ERRFUNC		2

static void pusherror(lua_State* L) {
    int error = GetLastError();
    char buffer[128];
    if (FormatMessageA(FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, error, 0, buffer, sizeof(buffer), NULL))
        lua_pushstring(L, buffer);
    else
        lua_pushfstring(L, "system error %d\n", error);
}

static void** ll_register(lua_State* L, const char* path) {
    void** plib;
    lua_pushfstring(L, "%s%s", LIBPREFIX, path);
    lua_gettable(L, LUA_REGISTRYINDEX);  /* check library in registry? */
    if (!lua_isnil(L, -1))  /* is there an entry? */
        plib = (void**)lua_touserdata(L, -1);
    else {  /* no entry yet; create one */
        lua_pop(L, 1);
        plib = (void**)lua_newuserdata(L, sizeof(const void*));
        *plib = NULL;
        luaL_getmetatable(L, "_LOADLIB");
        lua_setmetatable(L, -2);
        lua_pushfstring(L, "%s%s", LIBPREFIX, path);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }
    return plib;
}

static lua_CFunction ll_sym(lua_State* L, void* lib, const char* sym) {
    lua_CFunction f = (lua_CFunction)GetProcAddress((HINSTANCE)lib, sym);
    if (f == NULL) pusherror(L);
    return f;
}

static void* ll_load(lua_State* L, const char* path) {
    HINSTANCE lib = LoadLibraryA(path);
    if (lib == NULL) pusherror(L);
    return lib;
}


static int ll_loadfunc(lua_State* L, const char* path, const char* sym) {
    void** reg = ll_register(L, path);
    if (*reg == NULL) *reg = ll_load(L, path);
    if (*reg == NULL)
        return ERRLIB;  /* unable to load library */
    else {
        lua_CFunction f = ll_sym(L, *reg, sym);
        if (f == NULL)
            return ERRFUNC;  /* unable to find function */
        lua_pushcfunction(L, f);
        return 0;  /* return function */
    }
}

static const char* pushnexttemplate(lua_State* L, const char* path) {
    const char* l;
    while (*path == *LUA_PATHSEP) path++;  /* skip separators */
    if (*path == '\0') return NULL;  /* no more templates */
    l = strchr(path, *LUA_PATHSEP);  /* find next separator */
    if (l == NULL) l = path + strlen(path);
    lua_pushlstring(L, path, l - path);  /* template */
    return l;
}

static int readable(const char* filename) {
    FILE* f = fopen(filename, "r");  /* try to open file */
    if (f == NULL) return 0;  /* open failed */
    fclose(f);
    return 1;
}

static const char* findfile(lua_State* L, const char* name,
    const char* pname) {
    const char* path;
    name = luaL_gsub(L, name, ".", LUA_DIRSEP);
    lua_getfield(L, LUA_ENVIRONINDEX, pname);
    path = lua_tostring(L, -1);
    if (path == NULL)
        luaL_error(L, LUA_QL("package.%s") " must be a string", pname);
    lua_pushstring(L, "");  /* error accumulator */
    while ((path = pushnexttemplate(L, path)) != NULL) {
        const char* filename;
        filename = luaL_gsub(L, lua_tostring(L, -1), LUA_PATH_MARK, name);
        if (readable(filename))  /* does file exist and is readable? */
            return filename;  /* return that file name */
        lua_pop(L, 2);  /* remove path template and file name */
        luaO_pushfstring(L, "\n\tno file " LUA_QS, filename);
        lua_concat(L, 2);
    }
    return NULL;  /* not found */
}

static const char* mkfuncname(lua_State* L, const char* modname) {
    const char* funcname;
    const char* mark = strchr(modname, *LUA_IGMARK);
    if (mark) modname = mark + 1;
    funcname = luaL_gsub(L, modname, ".", LUA_OFSEP);
    funcname = lua_pushfstring(L, POF"%s", funcname);
    lua_remove(L, -2);  /* remove 'gsub' result */
    return funcname;
}

static void loaderror(lua_State* L, const char* filename) {
    luaL_error(L, "error loading module " LUA_QS " from file " LUA_QS ":\n\t%s",
        lua_tostring(L, 1), filename, lua_tostring(L, -1));
}

static int loader_C(lua_State* L) {
    const char* funcname;
    const char* name = luaL_checkstring(L, 1);
    const char* filename = findfile(L, name, "cpath");
    if (filename == NULL) return 1;  /* library not found in this path */
    funcname = mkfuncname(L, name);
    if (ll_loadfunc(L, filename, funcname) != 0)
        loaderror(L, filename);
    return 1;  /* library loaded successfully */
}

static int ll_loadlib(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    const char* init = luaL_checkstring(L, 2);
    int stat = ll_loadfunc(L, path, init);
    if (stat == 0)  /* no errors? */
        return 1;  /* return the loaded function */
    else {  /* error; error message is on stack top */
        lua_pushnil(L);
        lua_insert(L, -2);
        lua_pushstring(L, (stat == ERRLIB) ? LIB_FAIL : "init");
        return 3;  /* return nil, error message, and where */
    }
}

static void ll_unloadlib(void* lib) {
    FreeLibrary((HINSTANCE)lib);
}

static int gctm(lua_State* L) {
    void** lib = (void**)luaL_checkudata(L, 1, "_LOADLIB");
    if (*lib) ll_unloadlib(*lib);
    *lib = NULL;  /* mark library as closed */

    //close the debugger too if it's still on
    StopDebugger(L);

    return 0;
}

static int LuaPrint(lua_State* L)
{
    int nargs = lua_gettop(L);

    for (int i = 1; i <= nargs; i++)
    {
        if (lua_isstring(L, i))
        {
            /* Pop the next arg using lua_tostring(L, i) and do your print */
            std::cout << lua_tostring(L, i);
        }
        else
        {
            if (lua_isnumber(L, i))
            {
                std::cout << lua_tonumber(L, i);
            }
            if (lua_isboolean(L, i))
            {
                std::cout << lua_toboolean(L, i) ? "true" : "false";
            }
            if (lua_isfunction(L, i))
            {
                std::cout << "<lua function located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            if (lua_isuserdata(L, i))
            {
                std::cout << "<lua userdata located at: " << std::hex << lua_touserdata(L, i) << ">";
            }
            if (lua_istable(L, i))
            {
                std::cout << "<lua table located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            if (lua_iscfunction(L, i))
            {
                std::cout << "<lua native function located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            if (lua_islightuserdata(L, i))
            {
                std::cout << "<lua lightuserdata located at: " << lua_topointer(L, i) << ">";
            }
            if (lua_isthread(L, i))
            {
                std::cout << "<lua thread located at: " << lua_topointer(L, i) << ">";
            }
            if (lua_isnoneornil(L, i))
            {
                std::cout << "nil";
            }
        }
        std::cout << "  ";
    }
    std::cout << std::endl;
    return 0;
}

static uint64_t __fastcall hf_luaopen_package(lua_State* L)
{
    //MessageBoxA(nullptr, "Lua has been patched sire.", "*Finger crossed*", MB_OK);
    /* create new type _LOADLIB */
    luaL_newmetatable(L, "_LOADLIB");
    lua_pushcfunction(L, gctm);
    lua_setfield(L, -2, "__gc");
    lua_register(L, "require2", ll_loadlib);
    lua_register(L, "print2", print2);
    lua_register(L, "PrintError", PrintError);
    lua_register(L, "PrintWarning", PrintWarning);
    lua_register(L, "StartDebugger", StartDebugger);
    lua_register(L, "StopDebugger", StopDebugger);
    lua_register(L, "ExecuteLuaScriptDebug", ExecuteLuaScriptDebug);
    lua_register(L, "print", LuaPrint);
    HWND currentWindow = GetActiveWindow();
    SetWindowText(currentWindow, L"Total Warhammer 2 Injected with SNED (Script Native Enchancer DLL)");

    return g_fp_luaopen_package(L);
}

bool SetupLoader()
{
    if (MH_Initialize() != MH_OK)
    {
        std::cout << "Failed to initialize hook library." << std::endl;

        return false;
    }

    HMODULE Warhammer3ExeAddress = GetModuleHandleA("Warhammer3.exe");
    if (!Warhammer3ExeAddress)
    {
        printf("failed to acquire Warhammer3 base");
        return false;
    }
    void* luaopen_packageAddress = (void*)GetProcAddress(Warhammer3ExeAddress, "luaopen_package");
    bool bAlternateMethod = false;
    if (!luaopen_packageAddress)
    {
        printf("failed to acquire position of luaopen_package. Maybe function is not exported? trying another attempt....");
        luaopen_packageAddress = (void*)(GetModuleHandleA("Warhammer3.exe") + 0x39d410);
        if (luaopen_packageAddress)
        {
            printf("hopefully second entry point is not dud. ++ Finger Crossed ++");
            bAlternateMethod = true;
        }
    }

    if (MH_CreateHook(luaopen_packageAddress, &hf_luaopen_package, reinterpret_cast<void**>(&g_fp_luaopen_package)) != MH_OK)
    {
        std::cout << "Failed to create `luaopen_package` hook." << std::endl;

        return false;
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK)
    {
        std::cout << "Failed to enable hooks." << std::endl;

        return false;
    }

    std::cout << "Everything seems normal." << std::endl;
    if (bAlternateMethod)
    {
        std::cout << "Although alternate method was used..." << std::endl;
    }

    return true;
}