#pragma once

// Numeric types and limits
#include <cstdint>
#include <climits>
#include <limits>

// Memory
#include <cstring>
#include <cstdlib>
#include <format>
#include <string>
#include <string_view>
#include <optional>
#include <cstdio>
#include <filesystem>
#include <tuple>
#include <initializer_list>

// Functions
#include <functional>
#include <concepts>
#include <compare>
#include <type_traits>
#include <variant>

// Sync
#include <mutex>
#include <thread>
#include <chrono>
#include <ctime>

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
#include "core/assertion.h"
#include "core/result.h"
#include "systems/logging_system.h"
