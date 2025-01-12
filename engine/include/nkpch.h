#pragma once

// Numeric types and limits
#include <cstdint>
#include <limits>

// Memory
#include <cstring>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <optional>
#include <cstdio>

// Functions
#include <functional>
#include <concepts>

// Sync
#include <mutex>
#include <thread>

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
#include "core/defines.h"
#include "macros/util.h"
#include "core/os.h"
