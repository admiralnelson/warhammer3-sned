#include "pch.h"
#include "Helper.h"

bool GetColour(short& ret)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info))
        return false;
    ret = info.wAttributes;
    return true;
}

int print2(lua_State* L)
{
    std::string s = std::string(luaL_checkstring(L, 1));
    std::cout << s << std::endl;

    return 0;
}

int PrintError(lua_State* L)
{
    std::string s = std::string(luaL_checkstring(L, 1));
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    short CurrentColour;
    GetColour(CurrentColour);

    SetConsoleTextAttribute(hConsole, BACKGROUND_RED | FOREGROUND_INTENSITY);

    std::cout << s;

    SetConsoleTextAttribute(hConsole, CurrentColour);

    return 0;
}

int PrintWarning(lua_State* L)
{
    std::string s = std::string(luaL_checkstring(L, 1));
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    short CurrentColour;
    GetColour(CurrentColour);
    SetConsoleTextAttribute(hConsole, BACKGROUND_GREEN | BACKGROUND_RED);

    std::cout << s;
    SetConsoleTextAttribute(hConsole, CurrentColour);

    return 0;
}

int ShowMessageBox(lua_State* L) {
    const char* message = luaL_checkstring(L, 1);
    const char* title = luaL_checkstring(L, 2);
    const char* icon = luaL_checkstring(L, 3);

    UINT type = MB_OK;
    if (strcmp(icon, "warning") == 0) {
        type |= MB_ICONWARNING;
    }
    else if (strcmp(icon, "informational") == 0) {
        type |= MB_ICONINFORMATION;
    }
    else if (strcmp(icon, "error") == 0) {
        type |= MB_ICONERROR;
    }

    MessageBoxA(NULL, message, title, type);

    return 0;
}
