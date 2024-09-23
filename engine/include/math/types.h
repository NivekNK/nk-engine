#pragma once

namespace nk {
    template<typename T>
    concept NumericOrBoolean = std::is_arithmetic_v<T> || std::is_same_v<T, bool>;
}