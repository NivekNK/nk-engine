#pragma once

#include "memory/object_lifetime.h"

namespace nk {
    template <typename T>
    concept IArrT = mem::ObjectStorageType<T>;

    template <typename A, typename T>
    concept HasDataLength = requires(A& arr) {
        { arr.data() } -> std::same_as<T*>;
        { arr.length() } -> std::same_as<u64>;
    };

    template <typename D>
    concept HasCapacity = requires(D& dyarr) {
        { dyarr.capacity() } -> std::same_as<u64>;
    };

    template <typename A, typename T>
    concept IArr = HasDataLength<A, T> && !HasCapacity<A> && IArrT<T>;

    template <typename D, typename T>
    concept IDyarr = HasDataLength<D, T> && HasCapacity<D> && IArrT<T>;
}
