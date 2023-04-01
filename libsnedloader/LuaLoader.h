#pragma once
#include "Helper.h"

typedef __int64 CAString;
typedef int(__fastcall* LUAOPEN_PACKAGE)(lua_State* L);
typedef void(__cdecl* LUAG_RUNERROR)(lua_State* L, const char* format, ...);
typedef char*(__fastcall* CA_STRING_C_STR)(CAString pointerToCAString);
typedef __int64(__fastcall* GET_TEXTURE_FROM_VFS)(__int64 thisPtr, int a2, CAString path, int a4, int a5, char a6);
typedef __int64(__fastcall* GET_FILE_FROM_VFS)(__int64 thisPtr, CAString path);

static LUAOPEN_PACKAGE g_fp_luaopen_package = nullptr;
static LUAG_RUNERROR g_luag_runerror = nullptr;
static CA_STRING_C_STR g_ca_string_to_char_thunk = nullptr;
static CA_STRING_C_STR g_ca_string_to_char = nullptr;
static GET_TEXTURE_FROM_VFS g_get_texture_from_vfs = nullptr;
static GET_FILE_FROM_VFS g_get_file_from_vfs = nullptr;

bool SetupLoader();