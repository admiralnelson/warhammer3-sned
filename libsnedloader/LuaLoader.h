#pragma once
#include "Helper.h"
typedef int(__fastcall* LUAOPEN_PACKAGE)(lua_State* L);

static LUAOPEN_PACKAGE g_fp_luaopen_package = nullptr;
bool SetupLoader();