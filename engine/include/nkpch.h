#pragma once

// File manipulation
#include <stdio.h>
#include <filesystem>

// Write to the console
#include <iostream>

// String manipulation
#include <string>
#include <format>

// Types
#include <cstdint>
#include <algorithm>
#include <cmath>

// Threading
#include <mutex>
#include <atomic>
#include <future>

// Windows specific
#if defined(NK_PLATFORM_WINDOWS)
	#include <windows.h>
    #include <windowsx.h>
#else
    #include <stdlib.h>
    #include <sys/stat.h>
    #include <unistd.h>
#endif

// Interfaces
#include <concepts>
#include <type_traits>
#include <optional>
#include <variant>

// Memory manipulation
#include <cstdlib>
#include <memory>
#include <functional>
#include <initializer_list>
#include <iterator>

// Utilities
#include "core/defines.h"
#include "system/logging_system.h"
#include "core/assertion.h"
