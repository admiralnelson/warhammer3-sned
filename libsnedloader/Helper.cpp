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