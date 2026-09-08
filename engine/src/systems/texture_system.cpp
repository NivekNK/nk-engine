#include "nkpch.h"

#include "systems/texture_system.h"

#include "collections/dyarr.h"
#include "memory/allocator.h"
#include "renderer/renderer.h"
#include "resources/image_loader.h"
#include "resources/material.h"
#include "systems/resource_system.h"

namespace nk {
    result<void, texture_error> TextureSystem::acquire_map_resources(TextureMap& map) {
        if (!m_initialized) return err(texture_error{texture_error_code::not_initialized, 0});
        if (map.sampler.valid()) return err(texture_error{texture_error_code::renderer_failed, 0});
        auto created = m_renderer->create_sampler(map.sampling);
        if (!created) return err(texture_error{
            created.error().code == renderer_error_code::out_of_memory ? texture_error_code::out_of_memory :
                texture_error_code::renderer_failed, created.error().native_code});
        map.sampler = *created;
        return ok();
    }

    result<void, texture_error> TextureSystem::release_map_resources(TextureMap& map) {
        if (!map.sampler.valid()) return ok();
        if (!m_initialized) return err(texture_error{texture_error_code::not_initialized, 0});
        auto released = m_renderer->release_sampler(map.sampler);
        if (!released) return err(texture_error{texture_error_code::renderer_failed, released.error().native_code});
        map.sampler = {};
        return ok();
    }

    namespace {
        u32 next_generation(const u32 generation) noexcept {
            if (generation == numeric::invalid_id)
                return 0;
            const u32 next = generation + 1;
            return next == numeric::invalid_id ? 0 : next;
        }

        bool valid_texture_extent(
            const u32 width,
            const u32 height,
            const u8 channel_count) noexcept {
            return width != 0 && height != 0 && channel_count >= 1 &&
                   channel_count <= 4;
        }

        bool valid_region(
            const Texture& texture,
            const TextureRegion region,
            const cl::slice<const u8> pixels) noexcept {
            if (texture.channel_count == 0 || texture.channel_count > 4 ||
                pixels.data() == nullptr || region.width == 0 ||
                region.height == 0 || region.x >= texture.width ||
                region.y >= texture.height ||
                region.width > texture.width - region.x ||
                region.height > texture.height - region.y) {
                return false;
            }
            const u64 texels =
                static_cast<u64>(region.width) * region.height;
            return texels <= numeric::u64_max / texture.channel_count &&
                   pixels.length() == texels * texture.channel_count;
        }

        texture_error translate_resource_error(
            const resource_error& error) noexcept {
            switch (error.code) {
                case resource_error_code::file_failed:
                    return {
                        texture_error_code::file_failed,
                        error.native_code,
                    };
                case resource_error_code::decode_failed:
                case resource_error_code::invalid_data:
                    return {
                        texture_error_code::decode_failed,
                        error.native_code,
                    };
                case resource_error_code::out_of_memory:
                    return {
                        texture_error_code::out_of_memory,
                        error.native_code,
                    };
                case resource_error_code::invalid_name:
                    return {texture_error_code::invalid_name, error.native_code};
                default:
                    return {texture_error_code::resource_failed, error.native_code};
            }
        }
    }

    TextureSystem::~TextureSystem() {
        shutdown();
    }

    result<TextureSystem*, texture_error> TextureSystem::create(
        mem::Allocator& allocator,
        Renderer& renderer,
        ResourceSystem& resources,
        const u32 max_texture_count) {
        TextureSystem* system = allocator.construct_t(TextureSystem);
        if (system == nullptr)
            return err(texture_error{texture_error_code::out_of_memory, 0});

        auto initialized = system->init(
            allocator,
            renderer,
            resources,
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
        ResourceSystem& resources,
        const u32 max_texture_count) {
        if (max_texture_count == 0)
            return err(texture_error{texture_error_code::capacity_exceeded, 0});

        m_allocator = &allocator;
        m_renderer = &renderer;
        m_resources = &resources;
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

        auto default_created = create_default_textures();
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
            if (m_default_specular_texture.m_internal_data != nullptr)
                m_renderer->destroy_texture(&m_default_specular_texture);
            if (m_default_normal_texture.m_internal_data != nullptr)
                m_renderer->destroy_texture(&m_default_normal_texture);
        }

        if (m_references.allocator() != nullptr)
            (void)m_references.map_shutdown();
        if (m_textures.allocator() != nullptr)
            (void)m_textures.arr_shutdown();

        m_default_texture = {};
        m_default_specular_texture = {};
        m_default_normal_texture = {};
        m_loaded_count = 0;
        m_runtime_count = 0;
        m_initialized = false;
        m_renderer = nullptr;
        m_resources = nullptr;
        m_allocator = nullptr;
    }

    result<void, texture_error> TextureSystem::create_default_textures() {
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

        constexpr u8 specular_pixel[]{0, 0, 0, 255};
        created = m_renderer->create_texture(
            default_specular_texture_name,
            1,
            1,
            4,
            specular_pixel,
            false,
            &m_default_specular_texture);
        if (!created) {
            m_renderer->destroy_texture(&m_default_texture);
            return err(texture_error{
                texture_error_code::renderer_failed,
                created.error().native_code,
            });
        }
        m_default_specular_texture.id = numeric::invalid_id;
        m_default_specular_texture.generation = 0;

        constexpr u8 normal_pixel[]{128, 128, 255, 255};
        created = m_renderer->create_texture(
            default_normal_texture_name,
            1,
            1,
            4,
            normal_pixel,
            false,
            &m_default_normal_texture);
        if (!created) {
            m_renderer->destroy_texture(&m_default_specular_texture);
            m_renderer->destroy_texture(&m_default_texture);
            return err(texture_error{
                texture_error_code::renderer_failed,
                created.error().native_code,
            });
        }
        m_default_normal_texture.id = numeric::invalid_id;
        m_default_normal_texture.generation = 0;
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
        if (name == default_specular_texture_name)
            return ok(&m_default_specular_texture);
        if (name == default_normal_texture_name)
            return ok(&m_default_normal_texture);

        if (TextureReference* reference = m_references.find(name);
            reference != nullptr) {
            if (reference->source != TextureSource::file)
                return err(texture_error{
                    texture_error_code::incompatible_texture,
                    0,
                });
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
                .source = TextureSource::file,
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

    result<Texture*, texture_error> TextureSystem::acquire_writable(
        const strview name,
        const u32 width,
        const u32 height,
        const u8 channel_count,
        const bool has_transparency,
        const bool auto_release) {
        if (!m_initialized)
            return err(texture_error{texture_error_code::not_initialized, 0});
        if (name.empty())
            return err(texture_error{texture_error_code::invalid_name, 0});
        if (name == default_texture_name ||
            name == default_specular_texture_name ||
            name == default_normal_texture_name) {
            return err(texture_error{
                texture_error_code::incompatible_texture,
                0,
            });
        }
        if (!valid_texture_extent(width, height, channel_count))
            return err(texture_error{texture_error_code::invalid_dimensions, 0});

        if (TextureReference* reference = m_references.find(name);
            reference != nullptr) {
            Texture& existing = m_textures[reference->slot];
            if (reference->source != TextureSource::runtime ||
                existing.width != width || existing.height != height ||
                existing.channel_count != channel_count ||
                existing.has_transparency() != has_transparency) {
                return err(texture_error{
                    texture_error_code::incompatible_texture,
                    0,
                });
            }
            ++reference->reference_count;
            return ok(&existing);
        }

        const u32 slot = find_free_slot();
        if (slot == numeric::invalid_id)
            return err(texture_error{
                texture_error_code::capacity_exceeded,
                0,
            });

        Texture texture{
            .id = slot,
            .width = width,
            .height = height,
            .channel_count = channel_count,
            .format = unorm_texture_format(channel_count),
            .flags = TextureFlag::writable |
                (has_transparency
                    ? TextureFlag::has_transparency
                    : TextureFlag::none),
        };
        auto created = m_renderer->create_writable_texture(&texture);
        if (!created)
            return err(texture_error{
                texture_error_code::renderer_failed,
                created.error().native_code,
            });
        texture.generation = 0;

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
                .source = TextureSource::runtime,
            });
        if (!inserted) {
            m_renderer->destroy_texture(&texture);
            return err(texture_error{texture_error_code::out_of_memory, 0});
        }

        m_textures[slot] = texture;
        ++m_runtime_count;
        return ok(&m_textures[slot]);
    }

    result<void, texture_error> TextureSystem::write(
        Texture& texture,
        const TextureRegion region,
        const cl::slice<const u8> pixels) {
        if (!m_initialized)
            return err(texture_error{texture_error_code::not_initialized, 0});
        if (texture.id >= m_textures.length() ||
            &m_textures[texture.id] != &texture || !texture.valid() ||
            !texture.writable() || texture.external()) {
            return err(texture_error{texture_error_code::invalid_operation, 0});
        }
        if (!valid_region(texture, region, pixels))
            return err(texture_error{texture_error_code::invalid_region, 0});

        auto written = m_renderer->write_texture(texture, region, pixels);
        if (!written)
            return err(texture_error{
                texture_error_code::renderer_failed,
                written.error().native_code,
            });
        texture.generation = next_generation(texture.generation);
        return ok();
    }

    result<void, texture_error> TextureSystem::resize(
        Texture& texture,
        const u32 width,
        const u32 height) {
        if (!m_initialized)
            return err(texture_error{texture_error_code::not_initialized, 0});
        if (texture.id >= m_textures.length() ||
            &m_textures[texture.id] != &texture || !texture.valid() ||
            !texture.writable() || texture.external()) {
            return err(texture_error{texture_error_code::invalid_operation, 0});
        }
        if (!valid_texture_extent(width, height, texture.channel_count))
            return err(texture_error{texture_error_code::invalid_dimensions, 0});
        if (texture.width == width && texture.height == height)
            return ok();

        auto resized = m_renderer->resize_texture(texture, width, height);
        if (!resized)
            return err(texture_error{
                texture_error_code::renderer_failed,
                resized.error().native_code,
            });
        texture.width = width;
        texture.height = height;
        texture.generation = next_generation(texture.generation);
        return ok();
    }

    void TextureSystem::release(const strview name) {
        if (!m_initialized || name.empty() || name == default_texture_name ||
            name == default_specular_texture_name ||
            name == default_normal_texture_name) {
            return;
        }

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
        if (reference->source == TextureSource::runtime)
            --m_runtime_count;
        else
            --m_loaded_count;
        m_references.remove(name);
        TraceLog("Texture '{}' unloaded.", name);
    }

    u32 TextureSystem::reference_count(const strview name) const noexcept {
        if (!m_initialized || name.empty() || name == default_texture_name ||
            name == default_specular_texture_name ||
            name == default_normal_texture_name) {
            return 0;
        }
        const TextureReference* reference = m_references.find(name);
        return reference == nullptr
            ? 0
            : static_cast<u32>(reference->reference_count);
    }

    result<void, texture_error> TextureSystem::load_texture(
        const strview name,
        Texture& texture) {
        auto resource = m_resources->load(name, ResourceType::image);
        if (!resource)
            return err(translate_resource_error(resource.error()));
        DecodedImage* image = resource->as<DecodedImage>();

        auto created = m_renderer->create_texture(
            name,
            image->width,
            image->height,
            image->channel_count,
            image->pixels.data(),
            image->has_transparency,
            &texture);
        auto unloaded = m_resources->unload(*resource);
        if (!unloaded) {
            if (created)
                m_renderer->destroy_texture(&texture);
            return err(texture_error{
                texture_error_code::resource_failed,
                unloaded.error().native_code,
            });
        }
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
