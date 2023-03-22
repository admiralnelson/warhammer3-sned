#pragma once
#include "Helper.h"
typedef int(__fastcall* LUAOPEN_PACKAGE)(lua_State* L);
typedef void(__cdecl* LUAG_RUNERROR)(lua_State* L, const char* format, ...);

static LUAOPEN_PACKAGE g_fp_luaopen_package = nullptr;
static LUAG_RUNERROR g_luag_runerror = nullptr;
bool SetupLoader();