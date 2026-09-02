#pragma once

#include <utility>

#include "core/strbuf.h"

namespace nk {
    class ResourceSystem;

    enum class ResourceType : u8 {
        unknown,
        text,
        binary,
        image,
        material,
        static_mesh,
        custom,
    };

    struct Resource final {
        Resource() noexcept = default;

        Resource(const Resource&) = delete;
        Resource& operator=(const Resource&) = delete;
        Resource& operator=(Resource&&) = delete;

        Resource(Resource&& other) noexcept {
            move_from(other);
        }

        [[nodiscard]] bool loaded() const noexcept {
            return owner != nullptr &&
                   loader_id != numeric::invalid_id &&
                   data != nullptr;
        }

        template <typename T>
        [[nodiscard]] T* as() noexcept {
            return static_cast<T*>(data);
        }

        template <typename T>
        [[nodiscard]] const T* as() const noexcept {
            return static_cast<const T*>(data);
        }

        void reset() noexcept {
            loader_id = numeric::invalid_id;
            type = ResourceType::unknown;
            name.clear();
            full_path.clear();
            data_size = 0;
            data = nullptr;
            owner = nullptr;
        }

        u32 loader_id = numeric::invalid_id;
        ResourceType type = ResourceType::unknown;
        strbuf<255> name;
        strbuf<511> full_path;
        u64 data_size = 0;
        void* data = nullptr;

    private:
        void move_from(Resource& other) noexcept {
            loader_id = other.loader_id;
            type = other.type;
            name = other.name;
            full_path = other.full_path;
            data_size = other.data_size;
            data = other.data;
            owner = other.owner;
            other.reset();
        }

        ResourceSystem* owner = nullptr;

        friend class ResourceSystem;
    };
}
