#pragma once
#include "Helper.h"
struct CAString {
    int length;      // 0x22 00 00 00
    int capacity;      // 0x22 00 00 00
    union {
        char* heap_buffer;
        char  sso_buffer[16]; // Short String Optimization buffer
    };    // 0x0000000060576d30
    // ... more fields, next one starts with 0x30
};

typedef CAString* FileName;
typedef void* VFS_FILE_PTR;
typedef void* VFS_FILE_RANGE;


typedef int(__fastcall* LUAOPEN_PACKAGE)(lua_State* L);
typedef void(__cdecl* LUAG_RUNERROR)(lua_State* L, const char* format, ...);
typedef char*(__fastcall* CA_STRING_C_STR)(CAString* pointerToCAString);
typedef __int64(__fastcall* GET_TEXTURE_FROM_VFS)(__int64 thisPtr, int a2, CAString path, int a4, int a5, char a6);
typedef void*(__fastcall* GET_FILE_ENTRY_FROM_VFS)(void* path);
typedef void (*CA_LOGGING)(char* s, ...);

typedef bool(__thiscall* VFS_READ_INTO_MEMORY)(void* thisVFSptr, FileName* fileName, size_t* length, void* data);
typedef VFS_FILE_PTR*(__thiscall* VFS_GET_FILE_ENTRY)(void* thisVFSptr, VFS_FILE_PTR* vfsFilePtr, FileName* fileName, VFS_FILE_RANGE fileRange, int vrmId);


static CA_LOGGING g_ca_logging = nullptr;
static LUAOPEN_PACKAGE g_fp_luaopen_package = nullptr;
static LUAG_RUNERROR g_luag_runerror = nullptr;
static CA_STRING_C_STR g_ca_string_to_char_thunk = nullptr;
static CA_STRING_C_STR g_ca_string_to_char = nullptr;
static GET_TEXTURE_FROM_VFS g_get_texture_from_vfs = nullptr;
static GET_FILE_ENTRY_FROM_VFS g_get_file_entry_from_vfs = nullptr;
static VFS_READ_INTO_MEMORY g_ca_read_into_memory = nullptr;
static VFS_GET_FILE_ENTRY g_ca_get_file_entry = nullptr;

bool SetupLoader();

