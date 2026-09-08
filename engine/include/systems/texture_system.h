#pragma once

#include "collections/arr.h"
#include "collections/map.h"
#include "collections/slice.h"
#include "core/result.h"
#include "core/str.h"
#include "resources/texture.h"
#include "renderer/sampler.h"

namespace nk {
    class Renderer;
    class ResourceSystem;
    struct TextureMap;
    namespace mem { class Allocator; }

    inline constexpr strview default_texture_name{"default", 7};
    inline constexpr strview default_specular_texture_name{
        "default_specular",
        16,
    };
    inline constexpr strview default_normal_texture_name{
        "default_normal",
        14,
    };

    enum class texture_error_code : u8 {
        invalid_name,
        not_initialized,
        capacity_exceeded,
        out_of_memory,
        file_failed,
        decode_failed,
        resource_failed,
        renderer_failed,
        invalid_operation,
        invalid_dimensions,
        invalid_region,
        incompatible_texture,
    };

    struct texture_error {
        texture_error_code code;
        i32 native_code;
    };

    class TextureSystem final {
    public:
        static constexpr u32 default_max_texture_count = 1024;

        TextureSystem() = default;
        ~TextureSystem();

        TextureSystem(const TextureSystem&) = delete;
        TextureSystem& operator=(const TextureSystem&) = delete;
        TextureSystem(TextureSystem&&) = delete;
        TextureSystem& operator=(TextureSystem&&) = delete;

        [[nodiscard]] static result<TextureSystem*, texture_error> create(
            mem::Allocator& allocator,
            Renderer& renderer,
            ResourceSystem& resources,
            u32 max_texture_count = default_max_texture_count);
        static void destroy(mem::Allocator& allocator, TextureSystem* system);

        [[nodiscard]] result<Texture*, texture_error> acquire(
            strview name,
            bool auto_release);
        // Loads {name}_{r,l,u,d,f,b}.png as +X,-X,+Y,-Y,+Z,-Z.
        [[nodiscard]] result<Texture*, texture_error> acquire_cube(
            strview name,
            bool auto_release);
        [[nodiscard]] result<Texture*, texture_error> acquire_writable(
            strview name,
            u32 width,
            u32 height,
            u8 channel_count,
            bool has_transparency,
            bool auto_release = true);
        [[nodiscard]] result<void, texture_error> write(
            Texture& texture,
            TextureRegion region,
            cl::slice<const u8> pixels);
        [[nodiscard]] result<void, texture_error> resize(
            Texture& texture,
            u32 width,
            u32 height);
        void release(strview name);
        [[nodiscard]] result<void, texture_error> acquire_map_resources(TextureMap& map);
        [[nodiscard]] result<void, texture_error> release_map_resources(TextureMap& map);

        Texture& default_texture() noexcept { return m_default_texture; }
        const Texture& default_texture() const noexcept {
            return m_default_texture;
        }
        Texture& default_specular_texture() noexcept {
            return m_default_specular_texture;
        }
        const Texture& default_specular_texture() const noexcept {
            return m_default_specular_texture;
        }
        Texture& default_normal_texture() noexcept {
            return m_default_normal_texture;
        }
        const Texture& default_normal_texture() const noexcept {
            return m_default_normal_texture;
        }

        u32 reference_count(strview name) const noexcept;
        u32 loaded_count() const noexcept { return m_loaded_count; }
        u32 runtime_count() const noexcept { return m_runtime_count; }

    private:
        enum class TextureSource : u8 {
            file,
            runtime,
        };

        struct TextureReference {
            u64 reference_count = 0;
            u32 slot = numeric::invalid_id;
            bool auto_release = false;
            TextureSource source = TextureSource::file;
        };

        [[nodiscard]] result<void, texture_error> init(
            mem::Allocator& allocator,
            Renderer& renderer,
            ResourceSystem& resources,
            u32 max_texture_count);
        void shutdown();
        [[nodiscard]] result<void, texture_error> create_default_textures();
        [[nodiscard]] result<void, texture_error> load_texture(
            strview name,
            Texture& texture);
        [[nodiscard]] result<void, texture_error> load_cube_texture(
            strview name,
            Texture& texture);
        u32 find_free_slot() const noexcept;

        mem::Allocator* m_allocator = nullptr;
        Renderer* m_renderer = nullptr;
        ResourceSystem* m_resources = nullptr;
        cl::arr<Texture> m_textures;
        cl::map<str, TextureReference> m_references;
        Texture m_default_texture{};
        Texture m_default_specular_texture{};
        Texture m_default_normal_texture{};
        u32 m_loaded_count = 0;
        u32 m_runtime_count = 0;
        bool m_initialized = false;
    };
}
