#include "pch.h"

//static lrdb::server *ServerInstance = nullptr;

int StartDebugger(lua_State* L)
{
    MessageBoxA(NULL, "Interactive LRDB debugger is not implemented yet", "", MB_ICONEXCLAMATION | MB_OK);
    /*if (ServerInstance == nullptr)
    {
        std::cout << "=======Debugger started, game will freeze!=======" << std::endl;
        ServerInstance = new lrdb::server(21110);
        ServerInstance->reset(L);
    }*/
    return 0;
}

int StopDebugger(lua_State* L)
{
    MessageBoxA(NULL, "Interactive LRDB debugger is not implemented yet", "", MB_ICONEXCLAMATION | MB_OK);
    /*if (ServerInstance != nullptr)
    {
        ServerInstance->exit();
        delete ServerInstance;
        ServerInstance = nullptr;
        std::cout << "=======Debugger stopped=======" << std::endl;
    }*/
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
