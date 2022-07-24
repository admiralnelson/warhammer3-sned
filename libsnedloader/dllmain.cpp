//hacks to enable DLL again
/*
    Copyright (C) 2021  admiralnelson aka kris b.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    email: admiralofinternetmeme[at]outlook[dot]com
    discord: z bb - tablet (bot easy)#1668
*/

// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"

#define EXPORT extern "C" __declspec(dllexport)

const char* LicenseText = ""
"Warhammer 2 SNED (Script Native Extension DLL) Runtime Copyright(C) 2021 admiralnelson\n"
"This program comes with ABSOLUTELY NO WARRANTY "
"for details please read the supplied LICENSE.txt in the supplied archive\n"
"This is free software, and you are welcome to redistribute it\n"
"under certain conditions.\n";

uintptr_t FindDMAAddy(uintptr_t ptr, std::vector<unsigned int> offsets)
{
    uintptr_t addr = ptr;
    for (unsigned int i = 0; i < offsets.size(); ++i)
    {
        addr = *(uintptr_t*)addr;
        addr += offsets[i];
    }
    return addr;
}


EXPORT BOOL Test()
{
#pragma EXPORT
    printf("hellow");
    return true;
}

bool RedirectConsoleIO()
{
    bool result = true;


    FILE* fp;

    // Redirect STDIN if the console has an input handle
    if (GetStdHandle(STD_INPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONIN$", "r", stdin) != 0)
            result = false;
        else
            setvbuf(stdin, NULL, _IONBF, 0);

    // Redirect STDOUT if the console has an output handle
    if (GetStdHandle(STD_OUTPUT_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stdout) != 0)
            result = false;
        else
            setvbuf(stdout, NULL, _IONBF, 0);

    // Redirect STDERR if the console has an error handle
    if (GetStdHandle(STD_ERROR_HANDLE) != INVALID_HANDLE_VALUE)
        if (freopen_s(&fp, "CONOUT$", "w", stderr) != 0)
            result = false;
        else
            setvbuf(stderr, NULL, _IONBF, 0);

    // Make C++ standard streams point to console as well.
    std::ios::sync_with_stdio(true);

    return result;
}

bool ReleaseConsole()
{
    bool result = true;
    FILE* fp;

    // Just to be safe, redirect standard IO to NUL before releasing.

    // Redirect STDIN to NUL
    if (freopen_s(&fp, "NUL:", "r", stdin) != 0)
        result = false;
    else
        setvbuf(stdin, NULL, _IONBF, 0);

    // Redirect STDOUT to NUL
    if (freopen_s(&fp, "NUL:", "w", stdout) != 0)
        result = false;
    else
        setvbuf(stdout, NULL, _IONBF, 0);

    // Redirect STDERR to NUL
    if (freopen_s(&fp, "NUL:", "w", stderr) != 0)
        result = false;
    else
        setvbuf(stderr, NULL, _IONBF, 0);

    // Detach from console
    if (!FreeConsole())
        result = false;

    return result;
}

bool CreateNewConsole()
{
    bool result = false;

    // Attempt to create new console
    if (AllocConsole())
    {
        result = RedirectConsoleIO();
    }

    return result;
}

bool AttachParentConsole()
{
    bool result = false;

    // Release any current console and redirect IO to NUL
    ReleaseConsole();

    // Attempt to attach to parent process's console
    if (AttachConsole(ATTACH_PARENT_PROCESS))
    {
        result = RedirectConsoleIO();
    }

    return result;
}

BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {

        /*FILE* console_stream = nullptr;

        AllocConsole();
        SetConsoleTitle(L"Warhammer 2 SNED (Script Native Extension DLL) console");
        freopen_s(&console_stream, "conout$", "w", stdout);*/

        bool bSpawnConsole = std::getenv("WARHAMMER_2_SNED_INLINE_CONSOLE") != nullptr;
        if (bSpawnConsole)
        {
            CreateNewConsole();
            SetConsoleTitle(L"Warhammer 2 SNED (Script Native Extension DLL) console");
        }
        else
        {
            AttachParentConsole();
        }

        std::cout << LicenseText << std::endl;

        SetupLoader();

        break;
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

