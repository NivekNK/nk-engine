#pragma once

namespace nk {
    template <typename T>
    concept IArrT = !std::is_const_v<T> && ((std::is_class_v<T> && std::is_default_constructible_v<T>) ||
                                            std::is_arithmetic_v<T> ||
                                            std::is_pointer_v<T> ||
                                            std::is_enum_v<T>);

    template <typename D, typename T>
    concept IArr = requires(D& dyarr) {
        { dyarr.data() } -> std::same_as<T*>;
        { dyarr.length() } -> std::same_as<u64>;
    };
}
