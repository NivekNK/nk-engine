#pragma once

namespace nk {
    template <typename D, typename T>
    concept IDyarr = requires(D& dyarr) {
        { dyarr.data() } -> std::same_as<T*>;
        { dyarr.length() } -> std::same_as<u64>;
        { dyarr.push_use() } -> std::same_as<T&>;
    };
}
