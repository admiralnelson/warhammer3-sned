#pragma once
#include "lua.h"
int StartDebugger(lua_State* L);
int StopDebugger(lua_State* L);
int ExecuteLuaScriptDebug(lua_State* L);