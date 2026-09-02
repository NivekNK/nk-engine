#include "nkpch.h"

#include "systems/texture_system.h"

#include "collections/dyarr.h"
#include "core/format.h"
#include "memory/allocator.h"
#include "renderer/renderer.h"
#include "resources/image_loader.h"

namespace nk {
    namespace {
        texture_error translate_image_error(const image_error& error) noexcept {
            switch (error.code) {
                case image_error_code::file_failed:
                    return {
                        texture_error_code::file_failed,
                        static_cast<i32>(error.file),
                    };
                case image_error_code::decode_failed:
                case image_error_code::limits_exceeded:
                    return {
                        texture_error_code::decode_failed,
                        error.native_code,
                    };
                case image_error_code::out_of_memory:
                    return {
                        texture_error_code::out_of_memory,
                        error.native_code,
                    };
            }
            return {texture_error_code::decode_failed, error.native_code};
        }
    }

    TextureSystem::~TextureSystem() {
        shutdown();
    }

    result<TextureSystem*, texture_error> TextureSystem::create(
        mem::Allocator& allocator,
        Renderer& renderer,
        const u32 max_texture_count) {
        TextureSystem* system = allocator.construct_t(TextureSystem);
        if (system == nullptr)
            return err(texture_error{texture_error_code::out_of_memory, 0});

        auto initialized = system->init(
            allocator,
            renderer,
            max_texture_count);
        if (!initialized) {
            const texture_error error = initialized.error();
            allocator.deconstruct_t(TextureSystem, system);
            return err(error);
        }
        return ok(system);
    }

    void TextureSystem::destroy(
        mem::Allocator& allocator,
        TextureSystem* system) {
        if (system != nullptr)
            allocator.deconstruct_t(TextureSystem, system);
    }

    result<void, texture_error> TextureSystem::init(
        mem::Allocator& allocator,
        Renderer& renderer,
        const u32 max_texture_count) {
        if (max_texture_count == 0)
            return err(texture_error{texture_error_code::capacity_exceeded, 0});

        m_allocator = &allocator;
        m_renderer = &renderer;
        if (!m_textures.arr_init(&allocator, max_texture_count)) {
            shutdown();
            return err(texture_error{texture_error_code::out_of_memory, 0});
        }
        auto map_initialized = m_references.map_init(
            &allocator,
            max_texture_count,
            hash_seed::deterministic);
        if (!map_initialized) {
            shutdown();
            return err(texture_error{texture_error_code::out_of_memory, 0});
        }

        auto default_created = create_default_texture();
        if (!default_created) {
            const texture_error error = default_created.error();
            shutdown();
            return err(error);
        }

        m_renderer->set_default_texture(&m_default_texture);
        m_initialized = true;
        InfoLog(
            "Texture system initialized with capacity {}.",
            max_texture_count);
        return ok();
    }

    void TextureSystem::shutdown() {
        if (m_renderer != nullptr) {
            m_renderer->set_default_texture(nullptr);
            for (Texture& texture : m_textures) {
                if (texture.m_internal_data != nullptr)
                    m_renderer->destroy_texture(&texture);
            }
            if (m_default_texture.m_internal_data != nullptr)
                m_renderer->destroy_texture(&m_default_texture);
        }

        if (m_references.allocator() != nullptr)
            (void)m_references.map_shutdown();
        if (m_textures.allocator() != nullptr)
            (void)m_textures.arr_shutdown();

        m_default_texture = {};
        m_loaded_count = 0;
        m_initialized = false;
        m_renderer = nullptr;
        m_allocator = nullptr;
    }

    result<void, texture_error> TextureSystem::create_default_texture() {
        constexpr u32 dimension = 256;
        constexpr u32 channels = 4;
        constexpr u64 byte_count =
            static_cast<u64>(dimension) * dimension * channels;

        cl::dyarr<u8> pixels;
        if (!pixels.dyarr_init_len(m_allocator, byte_count, byte_count))
            return err(texture_error{texture_error_code::out_of_memory, 0});
        std::memset(pixels.data(), 255, byte_count);

        for (u32 row = 0; row < dimension; ++row) {
            for (u32 column = 0; column < dimension; ++column) {
                if ((row & 1u) == (column & 1u)) {
                    const u64 offset =
                        (static_cast<u64>(row) * dimension + column) * channels;
                    pixels[offset] = 0;
                    pixels[offset + 1] = 0;
                }
            }
        }

        auto created = m_renderer->create_texture(
            default_texture_name,
            dimension,
            dimension,
            channels,
            pixels.data(),
            false,
            &m_default_texture);
        if (!created) {
            return err(texture_error{
                texture_error_code::renderer_failed,
                created.error().native_code,
            });
        }
        m_default_texture.id = numeric::invalid_id;
        m_default_texture.generation = 0;
        return ok();
    }

    result<Texture*, texture_error> TextureSystem::acquire(
        const strview name,
        const bool auto_release) {
        if (!m_initialized)
            return err(texture_error{texture_error_code::not_initialized, 0});
        if (name.empty())
            return err(texture_error{texture_error_code::invalid_name, 0});
        if (name == default_texture_name)
            return ok(&m_default_texture);

        if (TextureReference* reference = m_references.find(name);
            reference != nullptr) {
            ++reference->reference_count;
            return ok(&m_textures[reference->slot]);
        }

        const u32 slot = find_free_slot();
        if (slot == numeric::invalid_id) {
            return err(texture_error{
                texture_error_code::capacity_exceeded,
                0,
            });
        }

        Texture texture{};
        auto loaded = load_texture(name, texture);
        if (!loaded)
            return err(loaded.error());
        texture.id = slot;

        str owned_name{*m_allocator};
        if (!owned_name.assign(name)) {
            m_renderer->destroy_texture(&texture);
            return err(texture_error{texture_error_code::out_of_memory, 0});
        }

        auto inserted = m_references.try_emplace(
            std::move(owned_name),
            TextureReference{
                .reference_count = 1,
                .slot = slot,
                .auto_release = auto_release,
            });
        if (!inserted) {
            m_renderer->destroy_texture(&texture);
            return err(texture_error{texture_error_code::out_of_memory, 0});
        }

        m_textures[slot] = texture;
        ++m_loaded_count;
        TraceLog(
            "Texture '{}' acquired with reference count 1.",
            name);
        return ok(&m_textures[slot]);
    }

    void TextureSystem::release(const strview name) {
        if (!m_initialized || name.empty() || name == default_texture_name)
            return;

        TextureReference* reference = m_references.find(name);
        if (reference == nullptr || reference->reference_count == 0) {
            WarnLog("Attempted to release unknown texture '{}'.", name);
            return;
        }

        --reference->reference_count;
        if (reference->reference_count != 0 || !reference->auto_release)
            return;

        Texture& texture = m_textures[reference->slot];
        m_renderer->destroy_texture(&texture);
        m_references.remove(name);
        --m_loaded_count;
        TraceLog("Texture '{}' unloaded.", name);
    }

    u32 TextureSystem::reference_count(const strview name) const noexcept {
        if (!m_initialized || name.empty() || name == default_texture_name)
            return 0;
        const TextureReference* reference = m_references.find(name);
        return reference == nullptr
            ? 0
            : static_cast<u32>(reference->reference_count);
    }

    result<void, texture_error> TextureSystem::load_texture(
        const strview name,
        Texture& texture) {
        strbuf<512> path;
        if (!format_to(path, "assets/textures/{}.png", name)) {
            return err(texture_error{
                texture_error_code::invalid_name,
                0,
            });
        }

        auto image = ImageLoader::load_png(*m_allocator, path.view());
        if (!image)
            return err(translate_image_error(image.error()));

        auto created = m_renderer->create_texture(
            name,
            image->width,
            image->height,
            image->channel_count,
            image->pixels.data(),
            image->has_transparency,
            &texture);
        if (!created) {
            return err(texture_error{
                texture_error_code::renderer_failed,
                created.error().native_code,
            });
        }
        texture.generation = 0;

        InfoLog(
            "Texture '{}' loaded ({}x{}, {} channels, generation {}).",
            name,
            texture.width,
            texture.height,
            texture.channel_count,
            texture.generation);
        return ok();
    }

    u32 TextureSystem::find_free_slot() const noexcept {
        for (u32 index = 0; index < m_textures.length(); ++index) {
            if (m_textures[index].m_internal_data == nullptr)
                return index;
        }
        return numeric::invalid_id;
    }
}
