#pragma once

#include "core/result.h"
#include "core/strview.h"
#include "resources/resource.h"

namespace nk {
    namespace mem { class Allocator; }

    enum class resource_error_code : u8 {
        invalid_name,
        not_initialized,
        capacity_exceeded,
        duplicate_loader,
        no_loader,
        out_of_memory,
        file_failed,
        decode_failed,
        invalid_data,
        foreign_resource,
    };

    struct resource_error {
        resource_error_code code;
        i32 native_code;
    };

    class ResourceLoader {
    public:
        virtual ~ResourceLoader() = default;

        [[nodiscard]] virtual ResourceType type() const noexcept = 0;
        [[nodiscard]] virtual strview custom_type() const noexcept {
            return {};
        }
        [[nodiscard]] virtual strview type_path() const noexcept = 0;

        [[nodiscard]] virtual result<void, resource_error> load(
            mem::Allocator& allocator,
            strview asset_base_path,
            strview name,
            Resource& out_resource) const = 0;
        virtual void unload(
            mem::Allocator& allocator,
            Resource& resource) const noexcept = 0;
    };
}
