#pragma once

#include "collections/arr.h"
#include "core/result.h"
#include "core/strbuf.h"
#include "resources/loaders.h"

namespace nk {
    namespace mem { class Allocator; }

    class ResourceSystem final {
    public:
        static constexpr u32 default_max_loader_count = 32;

        ResourceSystem() = default;
        ~ResourceSystem();

        ResourceSystem(const ResourceSystem&) = delete;
        ResourceSystem& operator=(const ResourceSystem&) = delete;
        ResourceSystem(ResourceSystem&&) = delete;
        ResourceSystem& operator=(ResourceSystem&&) = delete;

        [[nodiscard]] static result<ResourceSystem*, resource_error> create(
            mem::Allocator& allocator,
            strview asset_base_path = {"assets", 6},
            u32 max_loader_count = default_max_loader_count);
        static void destroy(mem::Allocator& allocator, ResourceSystem* system);

        [[nodiscard]] result<void, resource_error> register_loader(
            ResourceLoader& loader);
        [[nodiscard]] result<Resource, resource_error> load(
            strview name,
            ResourceType type);
        [[nodiscard]] result<Resource, resource_error> load_custom(
            strview name,
            strview custom_type);
        [[nodiscard]] result<void, resource_error> unload(Resource& resource);
        // Loads through the immutable built-in loader registry without
        // publishing into ResourceSystem accounting. The caller owns both the
        // allocator lifetime and the matching detached unload.
        [[nodiscard]] result<Resource, resource_error> load_detached(
            mem::Allocator& allocator,
            strview name,
            ResourceType type) const;
        [[nodiscard]] result<void, resource_error> unload_detached(
            mem::Allocator& allocator,
            Resource& resource) const noexcept;

        [[nodiscard]] strview asset_base_path() const noexcept {
            return m_asset_base_path.view();
        }
        [[nodiscard]] u32 registered_loader_count() const noexcept {
            return m_registered_loader_count;
        }
        [[nodiscard]] u64 active_resource_count() const noexcept {
            return m_active_resource_count;
        }

    private:
        [[nodiscard]] result<void, resource_error> init(
            mem::Allocator& allocator,
            strview asset_base_path,
            u32 max_loader_count);
        void shutdown();
        [[nodiscard]] result<Resource, resource_error> load_with(
            strview name,
            u32 loader_id);
        [[nodiscard]] u32 find_loader(ResourceType type) const noexcept;
        [[nodiscard]] u32 find_custom_loader(strview custom_type) const noexcept;

        mem::Allocator* m_allocator = nullptr;
        strbuf<511> m_asset_base_path;
        cl::arr<ResourceLoader*> m_loaders;
        TextResourceLoader m_text_loader;
        BinaryResourceLoader m_binary_loader;
        ImageResourceLoader m_image_loader;
        MaterialResourceLoader m_material_loader;
        ShaderResourceLoader m_shader_loader;
        StaticMeshResourceLoader m_static_mesh_loader;
        u32 m_registered_loader_count = 0;
        u64 m_active_resource_count = 0;
        bool m_initialized = false;
    };
}
