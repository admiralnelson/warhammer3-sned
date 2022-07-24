// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#include <windows.h>
extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "lobject.h"
}

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <sstream>
#include <map>
#include <iostream>
#include <ctime>
#include <vector>
#include <mutex>
#include <deque>
#include <thread>
#include <iostream>
#include <condition_variable>
#include <future>
#include "MinHook.h"
#include "LuaLoader.h"


// add headers that you want to pre-compile here
#include "framework.h"

#endif //PCH_H
