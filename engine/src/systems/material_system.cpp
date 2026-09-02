#include "nkpch.h"

#include "systems/material_system.h"

#include <cerrno>

#include "core/format.h"
#include "memory/allocator.h"
#include "platform/file.h"
#include "renderer/renderer.h"
#include "systems/texture_system.h"

namespace nk {
    namespace {
        strview trim(const strview text) noexcept {
            u64 begin = 0;
            u64 end = text.length();
            while (begin < end) {
                const char character = text[begin];
                if (character != ' ' && character != '\t' &&
                    character != '\r' && character != '\n') {
                    break;
                }
                ++begin;
            }
            while (end > begin) {
                const char character = text[end - 1];
                if (character != ' ' && character != '\t' &&
                    character != '\r' && character != '\n') {
                    break;
                }
                --end;
            }
            return text.substr(begin, end - begin);
        }

        bool parse_color(const strview text, glm::vec4& color) noexcept {
            strbuf<255> buffer{text};
            if (buffer.truncated())
                return false;

            const char* cursor = buffer.cstr();
            char* end = nullptr;
            glm::vec4 parsed{};
            for (u32 component = 0; component < 4; ++component) {
                errno = 0;
                parsed[component] = std::strtof(cursor, &end);
                if (end == cursor || errno == ERANGE)
                    return false;
                cursor = end;
            }
            while (*cursor == ' ' || *cursor == '\t' ||
                   *cursor == '\r' || *cursor == '\n') {
                ++cursor;
            }
            if (*cursor != '\0')
                return false;
            color = parsed;
            return true;
        }
    }

    MaterialSystem::~MaterialSystem() {
        shutdown();
    }

    result<MaterialSystem*, material_error> MaterialSystem::create(
        mem::Allocator& allocator,
        Renderer& renderer,
        TextureSystem& textures,
        const u32 max_material_count) {
        MaterialSystem* system = allocator.construct_t(MaterialSystem);
        if (system == nullptr)
            return err(material_error{material_error_code::out_of_memory, 0});

        auto initialized = system->init(
            allocator,
            renderer,
            textures,
            max_material_count);
        if (!initialized) {
            const material_error error = initialized.error();
            allocator.deconstruct_t(MaterialSystem, system);
            return err(error);
        }
        return ok(system);
    }

    void MaterialSystem::destroy(
        mem::Allocator& allocator,
        MaterialSystem* system) {
        if (system != nullptr)
            allocator.deconstruct_t(MaterialSystem, system);
    }

    result<void, material_error> MaterialSystem::init(
        mem::Allocator& allocator,
        Renderer& renderer,
        TextureSystem& textures,
        const u32 max_material_count) {
        if (max_material_count == 0)
            return err(material_error{material_error_code::capacity_exceeded, 0});

        m_allocator = &allocator;
        m_renderer = &renderer;
        m_textures = &textures;
        if (!m_materials.arr_init(&allocator, max_material_count)) {
            shutdown();
            return err(material_error{material_error_code::out_of_memory, 0});
        }
        auto map_initialized = m_references.map_init(
            &allocator,
            max_material_count,
            hash_seed::deterministic);
        if (!map_initialized) {
            shutdown();
            return err(material_error{material_error_code::out_of_memory, 0});
        }

        auto default_created = create_default_material();
        if (!default_created) {
            const material_error error = default_created.error();
            shutdown();
            return err(error);
        }

        m_initialized = true;
        InfoLog(
            "Material system initialized with capacity {}.",
            max_material_count);
        return ok();
    }

    void MaterialSystem::shutdown() {
        if (m_renderer != nullptr) {
            for (Material& material : m_materials) {
                if (material.valid())
                    destroy_material(material);
            }
            if (m_default_material.valid())
                destroy_material(m_default_material);
        }

        if (m_references.allocator() != nullptr)
            (void)m_references.map_shutdown();
        if (m_materials.allocator() != nullptr)
            (void)m_materials.arr_shutdown();

        m_default_material = {};
        m_loaded_count = 0;
        m_initialized = false;
        m_textures = nullptr;
        m_renderer = nullptr;
        m_allocator = nullptr;
    }

    result<void, material_error> MaterialSystem::create_default_material() {
        Material material{};
        material.name.assign(default_material_name);
        material.diffuse_color = glm::vec4{1.0f};
        material.diffuse_map = {
            .texture = &m_textures->default_texture(),
            .use = TextureUse::diffuse,
        };
        material.diffuse_map_name.assign(default_texture_name);
        material.generation = 0;

        auto created = m_renderer->create_material(material);
        if (!created) {
            return err(material_error{
                material_error_code::renderer_failed,
                created.error().native_code,
            });
        }
        m_default_material = material;
        return ok();
    }

    result<Material*, material_error> MaterialSystem::acquire(
        const strview name) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (name.empty())
            return err(material_error{material_error_code::invalid_name, 0});
        if (name == default_material_name)
            return ok(&m_default_material);

        if (MaterialReference* reference = m_references.find(name);
            reference != nullptr) {
            ++reference->reference_count;
            return ok(&m_materials[reference->slot]);
        }

        auto config = load_config(name);
        if (!config)
            return err(config.error());
        return acquire(*config);
    }

    result<Material*, material_error> MaterialSystem::acquire(
        const MaterialConfig& config) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (config.name.empty())
            return err(material_error{material_error_code::invalid_name, 0});
        if (config.name.view() == default_material_name)
            return ok(&m_default_material);

        if (MaterialReference* reference =
                m_references.find(config.name.view());
            reference != nullptr) {
            ++reference->reference_count;
            return ok(&m_materials[reference->slot]);
        }

        const u32 slot = find_free_slot();
        if (slot == numeric::invalid_id) {
            return err(material_error{
                material_error_code::capacity_exceeded,
                0,
            });
        }

        Material material{};
        auto loaded = load_material(config, material);
        if (!loaded)
            return err(loaded.error());
        material.id = slot;

        str owned_name{*m_allocator};
        if (!owned_name.assign(config.name.view())) {
            destroy_material(material);
            return err(material_error{material_error_code::out_of_memory, 0});
        }
        auto inserted = m_references.try_emplace(
            std::move(owned_name),
            MaterialReference{
                .reference_count = 1,
                .slot = slot,
                .auto_release = config.auto_release,
            });
        if (!inserted) {
            destroy_material(material);
            return err(material_error{material_error_code::out_of_memory, 0});
        }

        m_materials[slot] = material;
        ++m_loaded_count;
        TraceLog(
            "Material '{}' acquired with reference count 1.",
            config.name.view());
        return ok(&m_materials[slot]);
    }

    void MaterialSystem::release(const strview name) {
        if (!m_initialized || name.empty() || name == default_material_name)
            return;

        MaterialReference* reference = m_references.find(name);
        if (reference == nullptr || reference->reference_count == 0) {
            WarnLog("Attempted to release unknown material '{}'.", name);
            return;
        }

        --reference->reference_count;
        if (reference->reference_count != 0 || !reference->auto_release)
            return;

        destroy_material(m_materials[reference->slot]);
        m_references.remove(name);
        --m_loaded_count;
        TraceLog("Material '{}' unloaded.", name);
    }

    result<void, material_error> MaterialSystem::set_diffuse_texture(
        Material& material,
        const strview texture_name) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (!material.valid() || texture_name.empty())
            return err(material_error{material_error_code::invalid_name, 0});
        if (material.diffuse_map_name.view() == texture_name)
            return ok();

        Texture* replacement = nullptr;
        if (texture_name == default_texture_name) {
            replacement = &m_textures->default_texture();
        } else {
            auto acquired = m_textures->acquire(texture_name, true);
            if (!acquired) {
                return err(material_error{
                    material_error_code::texture_failed,
                    acquired.error().native_code,
                });
            }
            replacement = *acquired;
        }

        const strbuf<texture_name_capacity> previous_name =
            material.diffuse_map_name;
        material.diffuse_map.texture = replacement;
        material.diffuse_map.use = TextureUse::diffuse;
        material.diffuse_map_name.assign(texture_name);
        ++material.generation;
        if (material.generation == numeric::invalid_id)
            material.generation = 0;

        if (!previous_name.empty() &&
            previous_name.view() != default_texture_name) {
            m_textures->release(previous_name.view());
        }
        return ok();
    }

    result<MaterialConfig, material_error> MaterialSystem::load_config(
        const strview name) {
        strbuf<512> path;
        if (!format_to(path, "assets/materials/{}.kmt", name)) {
            return err(material_error{
                material_error_code::invalid_name,
                0,
            });
        }

        File file{*m_allocator};
        auto opened = file.open(path.view(), FileMode::Read, false);
        if (!opened) {
            return err(material_error{
                material_error_code::file_failed,
                static_cast<i32>(opened.error()),
            });
        }

        MaterialConfig config{};
        config.name.assign(name);
        str line{*m_allocator};
        while (true) {
            auto read = file.read_line(line);
            if (!read) {
                return err(material_error{
                    material_error_code::file_failed,
                    static_cast<i32>(read.error()),
                });
            }
            if (*read == read_line_outcome::end_of_file)
                break;

            const strview content = trim(line.view());
            if (content.empty() || content[0] == '#')
                continue;
            const u64 separator = content.find('=');
            if (separator == strview::npos)
                continue;

            const strview key = trim(content.substr(0, separator));
            const strview value = trim(content.substr(separator + 1));
            if (key == strview{"name", 4}) {
                if (!config.name.assign(value))
                    return err(material_error{material_error_code::invalid_config, 0});
            } else if (key == strview{"diffuse_map_name", 16}) {
                if (!config.diffuse_map_name.assign(value))
                    return err(material_error{material_error_code::invalid_config, 0});
            } else if (key == strview{"diffuse_colour", 14}) {
                if (!parse_color(value, config.diffuse_color))
                    return err(material_error{material_error_code::invalid_config, 0});
            }
        }

        auto closed = file.close();
        if (!closed) {
            return err(material_error{
                material_error_code::file_failed,
                static_cast<i32>(closed.error()),
            });
        }
        return ok(config);
    }

    result<void, material_error> MaterialSystem::load_material(
        const MaterialConfig& config,
        Material& material) {
        material.name.assign(config.name.view());
        material.diffuse_color = config.diffuse_color;
        material.diffuse_map.use = TextureUse::diffuse;
        material.diffuse_map_name.assign(config.diffuse_map_name.view());

        if (config.diffuse_map_name.empty() ||
            config.diffuse_map_name.view() == default_texture_name) {
            material.diffuse_map.texture = &m_textures->default_texture();
            material.diffuse_map_name.assign(default_texture_name);
        } else {
            auto texture = m_textures->acquire(
                config.diffuse_map_name.view(),
                true);
            if (!texture) {
                WarnLog(
                    "Unable to load texture '{}' for material '{}'; using default.",
                    config.diffuse_map_name.view(),
                    config.name.view());
                material.diffuse_map.texture = &m_textures->default_texture();
                material.diffuse_map_name.assign(default_texture_name);
            } else {
                material.diffuse_map.texture = *texture;
            }
        }

        material.generation = 0;
        auto created = m_renderer->create_material(material);
        if (!created) {
            if (material.diffuse_map_name.view() != default_texture_name)
                m_textures->release(material.diffuse_map_name.view());
            material = {};
            return err(material_error{
                material_error_code::renderer_failed,
                created.error().native_code,
            });
        }
        return ok();
    }

    void MaterialSystem::destroy_material(Material& material) {
        const strbuf<texture_name_capacity> texture_name =
            material.diffuse_map_name;
        m_renderer->destroy_material(material);
        if (!texture_name.empty() &&
            texture_name.view() != default_texture_name) {
            m_textures->release(texture_name.view());
        }
        material = {};
    }

    u32 MaterialSystem::find_free_slot() const noexcept {
        for (u32 index = 0; index < m_materials.length(); ++index) {
            if (!m_materials[index].valid())
                return index;
        }
        return numeric::invalid_id;
    }

    u32 MaterialSystem::reference_count(const strview name) const noexcept {
        if (!m_initialized || name.empty() || name == default_material_name)
            return 0;
        const MaterialReference* reference = m_references.find(name);
        return reference == nullptr
            ? 0
            : static_cast<u32>(reference->reference_count);
    }
}
