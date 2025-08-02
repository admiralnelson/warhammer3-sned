#include "pch.h"
#include "psapi.h"
#include "LuaLoader.h"
#include "Debugger.h"
#include "queue"
#include "../external/Detours/src/detours.h"

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

typedef DWORD(WINAPI* GetFinalPathNameByHandleW_t)(HANDLE, LPWSTR, DWORD, DWORD);
static GetFinalPathNameByHandleW_t original_GetFinalPathNameByHandleW = nullptr;


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
        lua_pushfstring(L, "\n\tno file " LUA_QS, filename);
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
            else if (lua_isboolean(L, i))
            {
                std::cout << lua_toboolean(L, i) ? "true" : "false";
            }
            else if (lua_isfunction(L, i))
            {
                std::cout << "<lua function located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            else if (lua_isuserdata(L, i))
            {
                std::cout << "<lua userdata located at: " << std::hex << lua_touserdata(L, i) << ">";
            }
            else if (lua_istable(L, i))
            {
                std::cout << "<lua table located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            else if (lua_iscfunction(L, i))
            {
                std::cout << "<lua native function located at: " << std::hex << lua_topointer(L, i) << ">";
            }
            else if (lua_islightuserdata(L, i))
            {
                std::cout << "<lua lightuserdata located at: " << lua_topointer(L, i) << ">";
            }
            else if (lua_isthread(L, i))
            {
                std::cout << "<lua thread located at: " << lua_topointer(L, i) << ">";
            }
            else if (lua_isnoneornil(L, i))
            {
                std::cout << "nil";
            }
        }
        std::cout << "  ";
    }
    std::cout << std::endl;
    return 0;
}

#define lua_lock(L)     ((void) 0) 
#define api_checknelems(L, n)	api_check(L, (n) <= (L->top - L->base))
#define api_check(l,e)		lua_assert(e)



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
    lua_register(L, "ShowMessageBox", ShowMessageBox);
    lua_register(L, "print", LuaPrint);
    HWND currentWindow = GetActiveWindow();
    SetWindowText(currentWindow, L"Total Warhammer 3 Injected with SNED (Script Native Enchancer DLL)");

    return g_fp_luaopen_package(L);
}

std::vector<char*> SearchStringInMemory(const std::string& target) 
{
    std::vector<char*> result;
    size_t targetSize = target.size();
    HMODULE hModule = GetModuleHandle(NULL);
    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));
    char* start = (char*)moduleInfo.lpBaseOfDll;
    char* end = start + moduleInfo.SizeOfImage;
    for (char* current = start; current + targetSize <= end; ++current) 
    {
        if (memcmp(current, target.c_str(), targetSize) == 0) 
        {
            result.push_back(current);
        }
    }
    return result;
}

bool IsPointerValid(void* ptr) {
    MEMORY_BASIC_INFORMATION memInfo;
    if (VirtualQuery(ptr, &memInfo, sizeof(memInfo)) == 0) {
        return false;
    }
    if (memInfo.State != MEM_COMMIT) {
        return false;
    }
    return true;
}


static void __fastcall patch_luaG_runerror(lua_State* L, const char* fmt, ...) 
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    int length = vsnprintf(nullptr, 0, fmt, args);
    char* buffer = new char[length + 1];
    vsnprintf(buffer, length + 1, fmt, args);
    std::string result(buffer);
    delete[] buffer;
    result = "Lua Runtime Error! \n" + result;
    MessageBoxA(nullptr, result.c_str(), "Critical Lua Runtime Error", MB_ICONERROR);
    va_end(args);

    va_start(args, fmt);
    g_luag_runerror(L, args, args);
    va_end(args, fmt);
}

void* FindMemoryByPattern(const char* pattern) {
    const char* p = pattern;
    HMODULE hModule = GetModuleHandle(NULL);
    MODULEINFO moduleInfo;
    GetModuleInformation(GetCurrentProcess(), hModule, &moduleInfo, sizeof(moduleInfo));
    unsigned char* start = (unsigned char*)moduleInfo.lpBaseOfDll;
    unsigned char* end = start + moduleInfo.SizeOfImage;
    size_t patternLength = strlen(pattern);
    unsigned char* current = start;
    while (current < end) {
        bool found = true;
        for (size_t i = 0; i < patternLength; i += 3) {
            if (p[i] == '?' && p[i + 1] == '?') {
                continue;
            }
            int value = 0;
            sscanf(&p[i], "%x", &value);
            if (*(current + i / 3) != value) {
                found = false;
                break;
            }
        }
        if (found) {
            return current;
        }
        current++;
    }
    return NULL;
}

typedef FILE* (*fopen_t)(const char* filename, const char* mode);
fopen_t original_fopen = nullptr;

FILE* FopenHook(const char* filename, const char* mode) 
{
    // Log the file operation
    std::cout << "[SNED] fopen called with filename: " << filename << " and mode: " << mode << std::endl;

    // Call the original fopen function
    return original_fopen(filename, mode);
}

typedef HANDLE(WINAPI* CreateFileW_t)(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
    );
CreateFileW_t original_CreateFileW = nullptr;

HANDLE WINAPI CreateFileW_Hook(
    LPCWSTR lpFileName,
    DWORD dwDesiredAccess,
    DWORD dwShareMode,
    LPSECURITY_ATTRIBUTES lpSecurityAttributes,
    DWORD dwCreationDisposition,
    DWORD dwFlagsAndAttributes,
    HANDLE hTemplateFile
) {
    std::wcout << L"[SNED] CreateFileW called with filename: " << lpFileName << std::endl;
    return original_CreateFileW(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
}

typedef BOOL(WINAPI* ReadFile_t)(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
    );
ReadFile_t original_ReadFile = nullptr;

std::string WideStringToUtf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

bool IsItCalledFromWarhammer3Exe() 
{
    // Get the module handle of the caller
    HMODULE hModule = NULL;
    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCTSTR)_ReturnAddress(), &hModule);

    // Get the module file name
    WCHAR moduleFileName[MAX_PATH];
    GetModuleFileNameW(hModule, moduleFileName, MAX_PATH);

    // Check if the call originates from Warhammer3.exe
    std::wstring warhammerModuleName = L"Warhammer3.exe";

	return wcsstr(moduleFileName, warhammerModuleName.c_str()) != NULL;
}

DWORD WINAPI GetFinalPathNameByHandleW_Hook(HANDLE hFile, LPWSTR lpszFilePath, DWORD cchFilePath, DWORD dwFlags) {
    return original_GetFinalPathNameByHandleW(hFile, lpszFilePath, cchFilePath, dwFlags);
}

static std::mutex logMutex;

static int counter = 0;

class ThreadPool {
public:
    ThreadPool(size_t numThreads);
    ~ThreadPool();
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    std::atomic<bool> stop;
};

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty()) return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
            });
    }
}

ThreadPool::~ThreadPool() {
    stop = true;
    condition.notify_all();
    for (std::thread& worker : workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.push(std::move(task));
    }
    condition.notify_one();
}

// Global thread pool instance
ThreadPool threadPool(4);

BOOL WINAPI ReadFile_Hook(
    HANDLE hFile,
    LPVOID lpBuffer,
    DWORD nNumberOfBytesToRead,
    LPDWORD lpNumberOfBytesRead,
    LPOVERLAPPED lpOverlapped
) 
{
    BOOL result = original_ReadFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);

    // Defer the GetFinalPathNameByHandleW operation to another thread
    threadPool.enqueue([hFile, nNumberOfBytesToRead]() {
        const int maxPath = MAX_PATH;
        WCHAR filePath[maxPath];
        bool attemptToFindTheFile = false;
        if (hFile != INVALID_HANDLE_VALUE)
        {
            attemptToFindTheFile = GetFinalPathNameByHandleW(hFile, filePath, maxPath, FILE_NAME_NORMALIZED) != 0;
        }

        std::lock_guard<std::mutex> guard(logMutex); // Lock the mutex for thread-safe logging
        if (attemptToFindTheFile) {
            std::string path = WideStringToUtf8(filePath);
            std::cout << "[SNED] ReadFile called with handle: " << hFile << " on path " << path << " and bytes to read: " << nNumberOfBytesToRead << std::endl;
        }
    });


	return result;

  
}

bool is_probably_SSO_string(const char* ptr) {
    if (!ptr) return false;

	bool lengthIsNotLessThan15 = strnlen_s(ptr, 15) >= 15;
	if (lengthIsNotLessThan15) return false; // SSO strings are expected to be short

    size_t n_printable = 0;
    for (size_t i = 0; i < 15; ++i) {
        unsigned char c = static_cast<unsigned char>(ptr[i]);
        if (c == '\0') {
            // Null terminator found
            return (n_printable >= 2); // Require at least 2 printable chars before null
        }
        if (!isprint(c) && !isspace(c)) {
            // Non-printable and not whitespace
            return false;
        }
        ++n_printable;
    }
    // No null terminator found within max_len
    return false;
}

const char* viewCAString(const CAString* s) {
    if(is_probably_SSO_string((const char*)s)) {
        return (const char*)s;
	}

    if (!s || s->length <= 0) return nullptr;
    if (s->capacity <= 15) {
        return s->sso_buffer;
    }
    else {
        return s->heap_buffer;
    }
}

static bool runOnce = false;
static void* get_static_config()
{
    if (runOnce) {
        return nullptr; // Prevent multiple calls
    }
    runOnce = true;

    void* baseWarhammer3 = GetModuleHandleA("Warhammer3.exe");
    // (1) Address of the global pointer (DAT_1444f6e20)
    void* configPointerLocation = (void*)((uintptr_t)baseWarhammer3 + 0x44f6e20);
    // (2) Dereference to get the actual struct pointer
    void* configStruct = configPointerLocation;
    std::cout << "[SNED] Config struct pointer: " << configStruct << std::endl;

    void* flag_addr = (void*)((uintptr_t)configStruct + 0x5ea4);

    DWORD oldProtect;
    VirtualProtect(flag_addr, 1, PAGE_EXECUTE_READWRITE, &oldProtect);

    // Step 5: Set bit 0x40 to enable logging
    uint8_t* ptrFlag = (uint8_t*)flag_addr;
    *ptrFlag |= 0x40;

    VirtualProtect(flag_addr, 1, oldProtect, &oldProtect);


    // (3) Address of the logging flag
    uint8_t* loggingFlag = nullptr;
    if (configStruct) {
        loggingFlag = (uint8_t*)((uintptr_t)configStruct + 0x5ea4);
        std::cout << "[SNED] Logging flag address: " << (void*)loggingFlag << std::endl;
        std::cout << "[SNED] Logging flag value: " << std::hex << (int)*loggingFlag << std::endl;
    }
    else {
        std::cout << "[SNED] Config struct pointer is null!" << std::endl;
    }
    return loggingFlag;
}

static void* __fastcall VFS_resolve_path_entry_patch(void* path)
{
    //PrintCAString(*path);
    void* res = g_get_file_entry_from_vfs(path);
    //if (is_probably_string((char*) res)) {
    //    std::cout << "[SNED] VFS_resolve_path_entry called with path: " << res << std::endl;
    //}
    //else {
    get_static_config();
	CAString* str = (CAString*)res;
	const char* cString = viewCAString(str);
    std::cout << "[SNED] STRUCT VFS_resolve_path_entry called with path: " << cString << std::endl;
    
    return res;
}

static void __fastcall hook_logging(char* s, ...)
{
    va_list args;
    va_start(args, s);

    // You can forward the call to the original, or do your own thing
    // For example, print the format string and the first int:
    vprintf(s, args);

    va_end(args);

    // If you want to call the real function:
    // Call original with varargs forwarding
    if (g_ca_logging) {
        va_start(args, s);
        g_ca_logging(s, args); // WRONG: see below!
        va_end(args);
    }
}



typedef BOOL(WINAPI* CloseHandle_t)(HANDLE hObject);
CloseHandle_t original_CloseHandle = nullptr;

BOOL WINAPI CloseHandle_Hook(HANDLE hObject) 
{
    bool result = original_CloseHandle(hObject);
    std::cout << "[SNED] CloseHandle called with handle: " << hObject <<  std::endl;

    return result;
}

bool SetupLoader()
{
    system("pause");
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

    //void* ca_string_to_char_ptr_thunk = FindMemoryByPattern("E9 0B F3 FF FF CC CC CC CC CC CC CC CC CC CC CC");
    //if (!ca_string_to_char_ptr_thunk)
    //{
    //    printf("failed to acquire position of ca_string_to_char_ptr_thunk.");
    //}
    //else
    //{
    //    g_ca_string_to_char_thunk = (CA_STRING_C_STR)ca_string_to_char_ptr_thunk;
    //}

    //void* ca_string_to_char_ptr = FindMemoryByPattern("40 53 48 83 EC 30 48 C7 44 24 20 FE FF FF FF 48 8B 59 08 48 8D 05 B4 85 E3 03 48 3B D8 74 43 48 8B C3 48 B9 00 00 00 00 00 00 00 F0 48 23 C1 48 B9 00 00 00 00 00 00 00 80 48 3B C1 74 24 48 85");
    //if (!ca_string_to_char_ptr)
    //{
    //    printf("failed to acquire position of ca_string_to_char_ptr.");
    //}
    //else
    //{
    //    g_ca_string_to_char = (CA_STRING_C_STR)ca_string_to_char_ptr;
    //}

    //void* get_texture_from_vfs = FindMemoryByPattern("48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 54 41 56 41 57 48 81 EC A0 00 00 00 48 8D 05 60 80 88 02 44 8B F2 48 89 01 48 8B F9 89 51 20 48 8D 05 6D 00 86 02 48 89 41 08 45 33 E4 4C 89 61 18 33 D2 48 83 C1 28 41 8B F1 49 8B E8 E8 49 58 EC FF 44 0F B6 BC 24 E8 00 00 00 48 8D 05 91 45 8A 02 48 89 07 48 8D 05 5F 46 8A");
    //if(!get_texture_from_vfs)
    //{
    //    printf("failed to acquire position of get_texture_from_vfs.");
    //}
    //else
    //{
    //    g_get_texture_from_vfs = (GET_TEXTURE_FROM_VFS)get_texture_from_vfs;
    //}

    //void* get_file_from_vfs = FindMemoryByPattern("41 56 48 83 EC 40 48 C7 44 24 20 FE FF FF FF 48 89 5C 24 50 48 89 6C 24 58 48 89 74 24 60 48 89");
    //if (!get_file_from_vfs)
    //{
    //    printf("failed to acquire position of get_file_from_vfs.");
    //}
    //else
    //{
    //    g_get_file_from_vfs = (GET_FILE_FROM_VFS)get_file_from_vfs;
    //}

    //if (MH_CreateHook(get_texture_from_vfs, &patch_get_texture_from_vfs, reinterpret_cast<void**>(&g_get_texture_from_vfs)) != MH_OK)
    //{
    //    std::cout << "Failed to create `get_texture_from_vfs` hook." << std::endl;

    //    return false;
    //}

    //if (MH_CreateHook(get_file_from_vfs, &patch_get_file_from_vfs, reinterpret_cast<void**>(&g_get_file_from_vfs)) != MH_OK)
    //{
    //    std::cout << "Failed to create `get_file_from_vfs` hook." << std::endl;

    //    return false;
    //}
    
    void* VFS_resolve_path_entry = FindMemoryByPattern("48 89 5c 24 10 48 89 6c 24 18 48 89 74 24 20 57 41 56 41 57 48 83 ec 40 48 8b f1 48 8b 51 08 48 8b c2 49 be 00 00 00 00 00 00 00 f0");
    if (!VFS_resolve_path_entry)
    {
        printf("failed to acquire position of VFS_resolve_path_entry.");
    }
    else
    {
        g_get_file_entry_from_vfs = (GET_FILE_ENTRY_FROM_VFS)VFS_resolve_path_entry;
    }

    void* ca_logging = FindMemoryByPattern("48 89 4c 24 08 48 89 54 24 10 4c 89 44 24 18 4c 89 4c 24 20 48 83 ec 28 48 8d 54 24 38");
    if (!ca_logging)
    {
        printf("failed to acquire position of ca_logging .");
    }
    else
    {
        g_ca_logging = (CA_LOGGING)ca_logging;
    }


    HMODULE hKernel32 = GetModuleHandleA("kernel32.dll");
    if (!hKernel32) {
        std::cerr << "Failed to get handle to kernel32.dll." << std::endl;
        return false;
    }

    original_ReadFile = (ReadFile_t)GetProcAddress(hKernel32, "ReadFile");
    if (!original_ReadFile) {
        std::cerr << "Failed to get address of ReadFile." << std::endl;
        return false;
    }


    if (MH_CreateHook(luaopen_packageAddress, &hf_luaopen_package, reinterpret_cast<void**>(&g_fp_luaopen_package)) != MH_OK)
    {
        std::cout << "Failed to create `luaopen_package` hook." << std::endl;

        return false;
    }

    if (MH_CreateHook(&fopen, &FopenHook, reinterpret_cast<void**>(&original_fopen)) != MH_OK) 
    {
        std::cerr << "Failed to create fopen hook." << std::endl;
        return false;
    }

    if (DetourTransactionBegin() != NO_ERROR)
    {
        std::cout << "Failed to begin Detour transaction." << std::endl;
        return false;
    }

    if (DetourUpdateThread(GetCurrentThread()) != NO_ERROR)
    {
        std::cout << "Failed to update Detour thread." << std::endl;
        return false;
    }

    // Attach the ReadFile hook
    //if (DetourAttach(&(PVOID&)original_ReadFile, ReadFile_Hook) != NO_ERROR)
    //{
    //    std::cerr << "Failed to attach ReadFile hook." << std::endl;
    //    return false;
    //}

    if (DetourAttach(&(PVOID&)g_get_file_entry_from_vfs, VFS_resolve_path_entry_patch) != NO_ERROR) {
        std::cerr << "Failed to create VFS_resolve_path_entry_patch hook." << std::endl;
        return false;
    }

    if (DetourAttach(&(PVOID&)g_ca_logging, hook_logging) != NO_ERROR) {
        std::cerr << "Failed to create g_ca_logging hook." << std::endl;
        return false;
    }

    if (DetourTransactionCommit() != NO_ERROR)
    {
        std::cout << "Failed to commit Detour transaction." << std::endl;
        return false;
    }




    //// Create a hook for CloseHandle
    //if (MH_CreateHook(&CloseHandle, &CloseHandle_Hook, reinterpret_cast<void**>(&original_CloseHandle)) != MH_OK) {
    //    std::cerr << "Failed to create CloseHandle hook." << std::endl;
    //    return false;
    //}


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