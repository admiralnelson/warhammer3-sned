#include "pch.h"
#include "lrdb/server.hpp"

const int listenPort = 21110;
static lrdb::server* pDebugger = nullptr;

bool IsDebuggerOn()
{
    return pDebugger != nullptr;
}

int StartDebugger(lua_State* L)
{
    if (pDebugger != nullptr) return 0;
    int res = MessageBoxA(NULL, "Starting debugger, press ok to continue milord!", "", MB_ICONEXCLAMATION | MB_YESNO);
    if (res == IDYES)
    {
        pDebugger = new lrdb::server(listenPort);
        pDebugger->reset(L);
    }
    return 0;
}

int StopDebugger(lua_State* L)
{
    if (pDebugger == nullptr) return 0;
    pDebugger->exit();
    delete pDebugger;
    pDebugger = nullptr;
    return 0;
}

int ExecuteLuaScriptDebug(lua_State* L)
{
    StartDebugger(L);
    std::string luaScriptPath = luaL_checkstring(L, 1);
    bool result = luaL_dofile(L, luaScriptPath.c_str());
    lua_pushboolean(L, result);
    return 1;
}
