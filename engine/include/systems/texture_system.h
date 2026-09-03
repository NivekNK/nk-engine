#pragma once

#include "collections/arr.h"
#include "collections/map.h"
#include "core/result.h"
#include "core/str.h"
#include "resources/texture.h"

namespace nk {
    class Renderer;
    class ResourceSystem;
    namespace mem { class Allocator; }

    inline constexpr strview default_texture_name{"default", 7};
    inline constexpr strview default_specular_texture_name{
        "default_specular",
        16,
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
        void release(strview name);

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

        u32 reference_count(strview name) const noexcept;
        u32 loaded_count() const noexcept { return m_loaded_count; }

    private:
        struct TextureReference {
            u64 reference_count = 0;
            u32 slot = numeric::invalid_id;
            bool auto_release = false;
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
        u32 find_free_slot() const noexcept;

        mem::Allocator* m_allocator = nullptr;
        Renderer* m_renderer = nullptr;
        ResourceSystem* m_resources = nullptr;
        cl::arr<Texture> m_textures;
        cl::map<str, TextureReference> m_references;
        Texture m_default_texture{};
        Texture m_default_specular_texture{};
        u32 m_loaded_count = 0;
        bool m_initialized = false;
    };
}
