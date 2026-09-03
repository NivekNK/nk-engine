#include "nkpch.h"

#include "systems/material_system.h"

#include "memory/allocator.h"
#include "systems/resource_system.h"
#include "systems/shader_system.h"
#include "systems/texture_system.h"

namespace nk {
    namespace {
        material_error translate_resource_error(
            const resource_error& error) noexcept {
            switch (error.code) {
                case resource_error_code::invalid_name:
                    return {material_error_code::invalid_name, error.native_code};
                case resource_error_code::file_failed:
                    return {material_error_code::file_failed, error.native_code};
                case resource_error_code::invalid_data:
                    return {material_error_code::invalid_config, error.native_code};
                case resource_error_code::out_of_memory:
                    return {material_error_code::out_of_memory, error.native_code};
                default:
                    return {material_error_code::resource_failed, error.native_code};
            }
        }

        material_error translate_shader_error(
            const shader_system_error& error) noexcept {
            switch (error.code) {
                case shader_system_error_code::invalid_name:
                    return {material_error_code::invalid_name, error.native_code};
                case shader_system_error_code::capacity_exceeded:
                    return {
                        material_error_code::capacity_exceeded,
                        error.native_code,
                    };
                case shader_system_error_code::out_of_memory:
                    return {
                        material_error_code::out_of_memory,
                        error.native_code,
                    };
                case shader_system_error_code::invalid_config:
                case shader_system_error_code::invalid_uniform:
                case shader_system_error_code::uniform_not_found:
                    return {
                        material_error_code::invalid_config,
                        error.native_code,
                    };
                default:
                    return {
                        material_error_code::shader_failed,
                        error.native_code,
                    };
            }
        }
    }

    MaterialSystem::~MaterialSystem() {
        shutdown();
    }

    result<MaterialSystem*, material_error> MaterialSystem::create(
        mem::Allocator& allocator,
        ShaderSystem& shaders,
        TextureSystem& textures,
        ResourceSystem& resources,
        const u32 max_material_count) {
        MaterialSystem* system = allocator.construct_t(MaterialSystem);
        if (system == nullptr)
            return err(material_error{material_error_code::out_of_memory, 0});

        auto initialized = system->init(
            allocator,
            shaders,
            textures,
            resources,
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
        ShaderSystem& shaders,
        TextureSystem& textures,
        ResourceSystem& resources,
        const u32 max_material_count) {
        if (max_material_count == 0)
            return err(material_error{material_error_code::capacity_exceeded, 0});

        m_allocator = &allocator;
        m_shaders = &shaders;
        m_textures = &textures;
        m_resources = &resources;
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

        auto world_bindings = resolve_bindings(builtin_material_shader_name);
        if (!world_bindings) {
            const material_error error = world_bindings.error();
            shutdown();
            return err(error);
        }
        m_world_bindings = *world_bindings;

        auto ui_bindings = resolve_bindings(builtin_ui_shader_name);
        if (!ui_bindings) {
            const material_error error = ui_bindings.error();
            shutdown();
            return err(error);
        }
        m_ui_bindings = *ui_bindings;

        auto default_created = create_default_materials();
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
        if (m_shaders != nullptr) {
            for (Material& material : m_materials) {
                if (material.valid())
                    destroy_material(material);
            }
            if (m_default_ui_material.valid())
                destroy_material(m_default_ui_material);
            if (m_default_material.valid())
                destroy_material(m_default_material);
        }

        if (m_references.allocator() != nullptr)
            (void)m_references.map_shutdown();
        if (m_materials.allocator() != nullptr)
            (void)m_materials.arr_shutdown();

        m_default_material = {};
        m_default_ui_material = {};
        m_world_bindings = {};
        m_ui_bindings = {};
        m_loaded_count = 0;
        m_initialized = false;
        m_textures = nullptr;
        m_resources = nullptr;
        m_shaders = nullptr;
        m_allocator = nullptr;
    }

    result<MaterialSystem::UniformBindings, material_error>
    MaterialSystem::resolve_bindings(const strview shader_name) {
        auto shader = m_shaders->handle(shader_name);
        if (!shader)
            return err(translate_shader_error(shader.error()));

        UniformBindings resolved{};
        resolved.shader = *shader;
        auto projection = m_shaders->uniform(*shader, "projection");
        if (!projection)
            return err(translate_shader_error(projection.error()));
        resolved.projection = *projection;
        auto view = m_shaders->uniform(*shader, "view");
        if (!view)
            return err(translate_shader_error(view.error()));
        resolved.view = *view;
        auto diffuse_color = m_shaders->uniform(*shader, "diffuse_color");
        if (!diffuse_color)
            return err(translate_shader_error(diffuse_color.error()));
        resolved.diffuse_color = *diffuse_color;
        auto diffuse_texture =
            m_shaders->uniform(*shader, "diffuse_texture");
        if (!diffuse_texture)
            return err(translate_shader_error(diffuse_texture.error()));
        resolved.diffuse_texture = *diffuse_texture;
        auto model = m_shaders->uniform(*shader, "model");
        if (!model)
            return err(translate_shader_error(model.error()));
        resolved.model = *model;
        return ok(resolved);
    }

    const MaterialSystem::UniformBindings* MaterialSystem::bindings(
        const MaterialType type) const noexcept {
        const UniformBindings* resolved = type == MaterialType::world
            ? &m_world_bindings
            : &m_ui_bindings;
        return resolved->valid() ? resolved : nullptr;
    }

    result<void, material_error> MaterialSystem::create_default_materials() {
        Material world{};
        world.name.assign(default_material_name);
        world.shader = m_world_bindings.shader;
        world.type = MaterialType::world;
        world.diffuse_color = glm::vec4{1.0f};
        world.diffuse_map = {
            .texture = &m_textures->default_texture(),
            .use = TextureUse::diffuse,
        };
        world.diffuse_map_name.assign(default_texture_name);
        world.generation = 0;

        auto world_created = m_shaders->acquire_instance(world.shader);
        if (!world_created) {
            return err(translate_shader_error(world_created.error()));
        }
        world.internal_id = *world_created;

        Material ui{};
        ui.name.assign(default_ui_material_name);
        ui.shader = m_ui_bindings.shader;
        ui.type = MaterialType::ui;
        ui.diffuse_color = glm::vec4{1.0f};
        ui.diffuse_map = {
            .texture = &m_textures->default_texture(),
            .use = TextureUse::diffuse,
        };
        ui.diffuse_map_name.assign(default_texture_name);
        ui.generation = 0;

        auto ui_created = m_shaders->acquire_instance(ui.shader);
        if (!ui_created) {
            (void)m_shaders->release_instance(
                world.shader,
                world.internal_id);
            return err(translate_shader_error(ui_created.error()));
        }
        ui.internal_id = *ui_created;

        m_default_material = world;
        m_default_ui_material = ui;
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
        if (name == default_ui_material_name)
            return ok(&m_default_ui_material);

        if (MaterialReference* reference = m_references.find(name);
            reference != nullptr) {
            ++reference->reference_count;
            return ok(&m_materials[reference->slot]);
        }

        auto resource = m_resources->load(name, ResourceType::material);
        if (!resource)
            return err(translate_resource_error(resource.error()));

        auto acquired = acquire(*resource->as<MaterialConfig>());
        auto unloaded = m_resources->unload(*resource);
        if (!unloaded) {
            if (acquired)
                release((*acquired)->name.view());
            return err(material_error{
                material_error_code::resource_failed,
                unloaded.error().native_code,
            });
        }
        return acquired;
    }

    result<Material*, material_error> MaterialSystem::acquire(
        const MaterialConfig& config) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (config.name.empty())
            return err(material_error{material_error_code::invalid_name, 0});
        if (config.name.view() == default_material_name)
            return ok(&m_default_material);
        if (config.name.view() == default_ui_material_name)
            return ok(&m_default_ui_material);

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
        if (!m_initialized || name.empty() ||
            name == default_material_name ||
            name == default_ui_material_name) {
            return;
        }

        MaterialReference* reference = m_references.find(name);
        if (reference == nullptr || reference->reference_count == 0) {
            WarnLog("Attempted to release unknown material '{}'.", name);
            return;
        }

        --reference->reference_count;
        if (reference->reference_count != 0 || !reference->auto_release)
            return;

        const strbuf<material_name_capacity> released_name{name};
        destroy_material(m_materials[reference->slot]);
        m_references.remove(released_name.view());
        --m_loaded_count;
        TraceLog("Material '{}' unloaded.", released_name.view());
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

    result<void, material_error> MaterialSystem::set_diffuse_color(
        Material& material,
        const glm::vec4& color) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (!material.valid())
            return err(material_error{material_error_code::invalid_name, 0});
        if (material.diffuse_color == color)
            return ok();
        material.diffuse_color = color;
        ++material.generation;
        if (material.generation == numeric::invalid_id)
            material.generation = 0;
        return ok();
    }

    result<void, material_error> MaterialSystem::apply_global(
        const MaterialType type,
        const glm::mat4& projection,
        const glm::mat4& view) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        const UniformBindings* uniform = bindings(type);
        if (uniform == nullptr)
            return err(material_error{material_error_code::invalid_config, 0});

        auto used = m_shaders->use(uniform->shader);
        if (!used)
            return err(translate_shader_error(used.error()));
        auto globals_bound = m_shaders->bind_globals();
        if (!globals_bound)
            return err(translate_shader_error(globals_bound.error()));
        auto projection_set = m_shaders->set_uniform(
            uniform->projection,
            projection);
        if (!projection_set)
            return err(translate_shader_error(projection_set.error()));
        auto view_set = m_shaders->set_uniform(uniform->view, view);
        if (!view_set)
            return err(translate_shader_error(view_set.error()));
        auto applied = m_shaders->apply_globals();
        if (!applied)
            return err(translate_shader_error(applied.error()));
        return ok();
    }

    result<void, material_error> MaterialSystem::apply_instance(
        Material& material,
        const u64 frame_number) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (!material.valid())
            return err(material_error{material_error_code::invalid_config, 0});
        const UniformBindings* uniform = bindings(material.type);
        if (uniform == nullptr || material.shader != uniform->shader ||
            m_shaders->current_shader() != material.shader) {
            return err(material_error{material_error_code::invalid_config, 0});
        }

        Texture* texture = material.diffuse_map.texture;
        const u32 texture_id = texture == nullptr
            ? numeric::invalid_id
            : texture->id;
        const u32 texture_generation = texture == nullptr
            ? numeric::invalid_id
            : texture->generation;
        const bool needs_update =
            material.apply_state.frame_number != frame_number ||
            material.apply_state.material_generation != material.generation ||
            material.apply_state.texture_id != texture_id ||
            material.apply_state.texture_generation != texture_generation ||
            material.apply_state.instance_id != material.internal_id ||
            material.apply_state.shader != material.shader;

        auto instance_bound =
            m_shaders->bind_instance(material.internal_id);
        if (!instance_bound)
            return err(translate_shader_error(instance_bound.error()));
        if (needs_update) {
            auto color_set = m_shaders->set_uniform(
                uniform->diffuse_color,
                material.diffuse_color);
            if (!color_set)
                return err(translate_shader_error(color_set.error()));
            auto texture_set = m_shaders->set_sampler(
                uniform->diffuse_texture,
                texture);
            if (!texture_set)
                return err(translate_shader_error(texture_set.error()));
        }
        auto applied = m_shaders->apply_instance(needs_update);
        if (!applied)
            return err(translate_shader_error(applied.error()));

        material.apply_state = {
            .frame_number = frame_number,
            .material_generation = material.generation,
            .texture_id = texture_id,
            .texture_generation = texture_generation,
            .instance_id = material.internal_id,
            .shader = material.shader,
        };
        return ok();
    }

    result<void, material_error> MaterialSystem::apply_local(
        const Material& material,
        const glm::mat4& model) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        const UniformBindings* uniform = bindings(material.type);
        if (!material.valid() || uniform == nullptr ||
            material.shader != uniform->shader ||
            m_shaders->current_shader() != material.shader) {
            return err(material_error{material_error_code::invalid_config, 0});
        }
        auto set = m_shaders->set_uniform(uniform->model, model);
        if (!set)
            return err(translate_shader_error(set.error()));
        return ok();
    }

    result<void, material_error> MaterialSystem::load_material(
        const MaterialConfig& config,
        Material& material) {
        material.name.assign(config.name.view());
        material.type = config.type;
        material.diffuse_color = config.diffuse_color;
        material.diffuse_map.use = TextureUse::diffuse;
        material.diffuse_map_name.assign(config.diffuse_map_name.view());

        const strview shader_name = config.shader_name.empty()
            ? (config.type == MaterialType::world
                ? builtin_material_shader_name
                : builtin_ui_shader_name)
            : config.shader_name.view();
        auto shader = m_shaders->handle(shader_name);
        if (!shader)
            return err(translate_shader_error(shader.error()));
        const UniformBindings* expected = bindings(config.type);
        if (expected == nullptr || *shader != expected->shader)
            return err(material_error{material_error_code::invalid_config, 0});
        material.shader = *shader;

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
        auto created = m_shaders->acquire_instance(material.shader);
        if (!created) {
            if (material.diffuse_map_name.view() != default_texture_name)
                m_textures->release(material.diffuse_map_name.view());
            material = {};
            return err(translate_shader_error(created.error()));
        }
        material.internal_id = *created;
        return ok();
    }

    void MaterialSystem::destroy_material(Material& material) {
        const strbuf<texture_name_capacity> texture_name =
            material.diffuse_map_name;
        if (material.shader.valid() &&
            material.internal_id != numeric::invalid_id) {
            auto released = m_shaders->release_instance(
                material.shader,
                material.internal_id);
            if (!released) {
                ErrorLog(
                    "Failed to release material shader instance: shader_error={}, native_code={}.",
                    static_cast<u32>(released.error().code),
                    released.error().native_code);
            }
        }
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
        if (!m_initialized || name.empty() ||
            name == default_material_name ||
            name == default_ui_material_name) {
            return 0;
        }
        const MaterialReference* reference = m_references.find(name);
        return reference == nullptr
            ? 0
            : static_cast<u32>(reference->reference_count);
    }
}
