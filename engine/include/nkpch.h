#pragma once

// Numeric types and limits
#include <cstdint>
#include <limits>

// Memory
#include <cstring>
#include <format>

// Functions
#include <functional>

// OS specific
#if defined(NK_PLATFORM_WINDOWS)
	#include <windows.h>
    #include <windowsx.h>
#elif defined(NK_PLATFORM_LINUX)
    #include <stdlib.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

// Engine
#include "macros/util.h"
#include "core/defines.h"
