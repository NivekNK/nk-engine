#include "nkpch.h"

#include "systems/material_system.h"

#include "core/math.h"
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

        auto world_bindings = resolve_bindings(
            builtin_material_shader_name,
            MaterialType::world);
        if (!world_bindings) {
            const material_error error = world_bindings.error();
            shutdown();
            return err(error);
        }
        m_world_bindings = *world_bindings;

        auto ui_bindings = resolve_bindings(
            builtin_ui_shader_name,
            MaterialType::ui);
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
    MaterialSystem::resolve_bindings(
        const strview shader_name,
        const MaterialType type) {
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

        if (type == MaterialType::world) {
            auto ambient_color = m_shaders->uniform(*shader, "ambient_color");
            if (!ambient_color)
                return err(translate_shader_error(ambient_color.error()));
            resolved.ambient_color = *ambient_color;
            auto light_direction = m_shaders->uniform(
                *shader,
                "directional_light_direction");
            if (!light_direction)
                return err(translate_shader_error(light_direction.error()));
            resolved.directional_light_direction = *light_direction;
            auto light_color = m_shaders->uniform(
                *shader,
                "directional_light_color");
            if (!light_color)
                return err(translate_shader_error(light_color.error()));
            resolved.directional_light_color = *light_color;
            auto normal_matrix = m_shaders->uniform(*shader, "normal_matrix");
            if (!normal_matrix)
                return err(translate_shader_error(normal_matrix.error()));
            resolved.normal_matrix = *normal_matrix;
            auto specular_texture =
                m_shaders->uniform(*shader, "specular_texture");
            if (!specular_texture)
                return err(translate_shader_error(specular_texture.error()));
            resolved.specular_texture = *specular_texture;
            auto shininess = m_shaders->uniform(*shader, "shininess");
            if (!shininess)
                return err(translate_shader_error(shininess.error()));
            resolved.shininess = *shininess;
            auto view_position = m_shaders->uniform(*shader, "view_position");
            if (!view_position)
                return err(translate_shader_error(view_position.error()));
            resolved.view_position = *view_position;
        }
        return ok(resolved);
    }

    const MaterialSystem::UniformBindings* MaterialSystem::bindings(
        const MaterialType type) const noexcept {
        const UniformBindings* resolved = type == MaterialType::world
            ? &m_world_bindings
            : &m_ui_bindings;
        return resolved->valid(type) ? resolved : nullptr;
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
        world.specular_map = {
            .texture = &m_textures->default_specular_texture(),
            .use = TextureUse::specular,
        };
        world.specular_map_name.assign(default_specular_texture_name);
        world.shininess = 32.0f;
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

    result<void, material_error> MaterialSystem::set_texture_maps(
        Material& material,
        const strview diffuse_texture_name,
        const strview specular_texture_name) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (!material.valid() || material.type != MaterialType::world ||
            diffuse_texture_name.empty() || specular_texture_name.empty()) {
            return err(material_error{material_error_code::invalid_name, 0});
        }

        const bool diffuse_changed =
            material.diffuse_map_name.view() != diffuse_texture_name;
        const bool specular_changed =
            material.specular_map_name.view() != specular_texture_name;
        if (!diffuse_changed && !specular_changed)
            return ok();

        Texture* diffuse_replacement = material.diffuse_map.texture;
        bool diffuse_acquired = false;
        if (diffuse_changed) {
            if (diffuse_texture_name == default_texture_name) {
                diffuse_replacement = &m_textures->default_texture();
            } else {
                auto acquired = m_textures->acquire(diffuse_texture_name, true);
                if (!acquired) {
                    return err(material_error{
                        material_error_code::texture_failed,
                        acquired.error().native_code,
                    });
                }
                diffuse_replacement = *acquired;
                diffuse_acquired = true;
            }
        }

        Texture* specular_replacement = material.specular_map.texture;
        if (specular_changed) {
            if (specular_texture_name == default_specular_texture_name) {
                specular_replacement = &m_textures->default_specular_texture();
            } else {
                auto acquired = m_textures->acquire(specular_texture_name, true);
                if (!acquired) {
                    if (diffuse_acquired)
                        m_textures->release(diffuse_texture_name);
                    return err(material_error{
                        material_error_code::texture_failed,
                        acquired.error().native_code,
                    });
                }
                specular_replacement = *acquired;
            }
        }

        const strbuf<texture_name_capacity> previous_diffuse_name =
            material.diffuse_map_name;
        const strbuf<texture_name_capacity> previous_specular_name =
            material.specular_map_name;

        if (diffuse_changed) {
            material.diffuse_map.texture = diffuse_replacement;
            material.diffuse_map.use = TextureUse::diffuse;
            material.diffuse_map_name.assign(diffuse_texture_name);
        }
        if (specular_changed) {
            material.specular_map.texture = specular_replacement;
            material.specular_map.use = TextureUse::specular;
            material.specular_map_name.assign(specular_texture_name);
        }

        ++material.generation;
        if (material.generation == numeric::invalid_id)
            material.generation = 0;

        if (diffuse_changed && !previous_diffuse_name.empty() &&
            previous_diffuse_name.view() != default_texture_name) {
            m_textures->release(previous_diffuse_name.view());
        }
        if (specular_changed && !previous_specular_name.empty() &&
            previous_specular_name.view() != default_specular_texture_name) {
            m_textures->release(previous_specular_name.view());
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

    result<void, material_error> MaterialSystem::set_specular_texture(
        Material& material,
        const strview texture_name) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (!material.valid() || material.type != MaterialType::world ||
            texture_name.empty()) {
            return err(material_error{material_error_code::invalid_name, 0});
        }
        if (material.specular_map_name.view() == texture_name)
            return ok();

        Texture* replacement = nullptr;
        if (texture_name == default_specular_texture_name) {
            replacement = &m_textures->default_specular_texture();
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
            material.specular_map_name;
        material.specular_map.texture = replacement;
        material.specular_map.use = TextureUse::specular;
        material.specular_map_name.assign(texture_name);
        ++material.generation;
        if (material.generation == numeric::invalid_id)
            material.generation = 0;

        if (!previous_name.empty() &&
            previous_name.view() != default_specular_texture_name) {
            m_textures->release(previous_name.view());
        }
        return ok();
    }

    result<void, material_error> MaterialSystem::set_shininess(
        Material& material,
        const f32 shininess) {
        if (!m_initialized)
            return err(material_error{material_error_code::not_initialized, 0});
        if (!material.valid() || material.type != MaterialType::world ||
            !std::isfinite(shininess) || shininess <= 0.0f) {
            return err(material_error{material_error_code::invalid_config, 0});
        }
        if (material.shininess == shininess)
            return ok();
        material.shininess = shininess;
        ++material.generation;
        if (material.generation == numeric::invalid_id)
            material.generation = 0;
        return ok();
    }

    result<void, material_error> MaterialSystem::apply_global(
        const MaterialType type,
        const glm::mat4& projection,
        const glm::mat4& view,
        const glm::vec3& view_position,
        const SceneLighting& lighting) {
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
        if (type == MaterialType::world) {
            auto view_position_set = m_shaders->set_uniform(
                uniform->view_position,
                view_position);
            if (!view_position_set)
                return err(translate_shader_error(view_position_set.error()));
            auto ambient_set = m_shaders->set_uniform(
                uniform->ambient_color,
                lighting.ambient_color);
            if (!ambient_set)
                return err(translate_shader_error(ambient_set.error()));
            auto direction_set = m_shaders->set_uniform(
                uniform->directional_light_direction,
                lighting.directional.direction);
            if (!direction_set)
                return err(translate_shader_error(direction_set.error()));
            auto light_color_set = m_shaders->set_uniform(
                uniform->directional_light_color,
                lighting.directional.color);
            if (!light_color_set)
                return err(translate_shader_error(light_color_set.error()));
        }
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

        Texture* diffuse_texture = material.diffuse_map.texture;
        const u32 diffuse_texture_id = diffuse_texture == nullptr
            ? numeric::invalid_id
            : diffuse_texture->id;
        const u32 diffuse_texture_generation = diffuse_texture == nullptr
            ? numeric::invalid_id
            : diffuse_texture->generation;
        Texture* specular_texture = material.type == MaterialType::world
            ? material.specular_map.texture
            : nullptr;
        const u32 specular_texture_id = specular_texture == nullptr
            ? numeric::invalid_id
            : specular_texture->id;
        const u32 specular_texture_generation = specular_texture == nullptr
            ? numeric::invalid_id
            : specular_texture->generation;
        const bool needs_update =
            material.apply_state.frame_number != frame_number ||
            material.apply_state.material_generation != material.generation ||
            material.apply_state.diffuse_texture_id != diffuse_texture_id ||
            material.apply_state.diffuse_texture_generation !=
                diffuse_texture_generation ||
            material.apply_state.specular_texture_id != specular_texture_id ||
            material.apply_state.specular_texture_generation !=
                specular_texture_generation ||
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
                diffuse_texture);
            if (!texture_set)
                return err(translate_shader_error(texture_set.error()));
            if (material.type == MaterialType::world) {
                auto specular_set = m_shaders->set_sampler(
                    uniform->specular_texture,
                    specular_texture);
                if (!specular_set)
                    return err(translate_shader_error(specular_set.error()));
                auto shininess_set = m_shaders->set_uniform(
                    uniform->shininess,
                    material.shininess);
                if (!shininess_set)
                    return err(translate_shader_error(shininess_set.error()));
            }
        }
        auto applied = m_shaders->apply_instance(needs_update);
        if (!applied)
            return err(translate_shader_error(applied.error()));

        material.apply_state = {
            .frame_number = frame_number,
            .material_generation = material.generation,
            .diffuse_texture_id = diffuse_texture_id,
            .diffuse_texture_generation = diffuse_texture_generation,
            .specular_texture_id = specular_texture_id,
            .specular_texture_generation = specular_texture_generation,
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
        if (material.type == MaterialType::world) {
            const glm::mat4 normal_matrix = math::normal_matrix(model);
            auto normal_set = m_shaders->set_uniform(
                uniform->normal_matrix,
                normal_matrix);
            if (!normal_set)
                return err(translate_shader_error(normal_set.error()));
        }
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
        material.shininess = config.shininess;
        if (!std::isfinite(material.shininess) || material.shininess <= 0.0f)
            return err(material_error{material_error_code::invalid_config, 0});
        if (config.type == MaterialType::world) {
            material.specular_map.use = TextureUse::specular;
            material.specular_map_name.assign(config.specular_map_name.view());
        }

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

        bool diffuse_acquired = false;
        bool specular_acquired = false;
        if (config.diffuse_map_name.empty() ||
            config.diffuse_map_name.view() == default_texture_name) {
            material.diffuse_map.texture = &m_textures->default_texture();
            material.diffuse_map_name.assign(default_texture_name);
        } else {
            auto texture = m_textures->acquire(
                config.diffuse_map_name.view(),
                true);
            if (!texture) {
                material = {};
                return err(material_error{
                    material_error_code::texture_failed,
                    texture.error().native_code,
                });
            }
            material.diffuse_map.texture = *texture;
            diffuse_acquired = true;
        }

        if (config.type == MaterialType::world) {
            if (config.specular_map_name.empty() ||
                config.specular_map_name.view() ==
                    default_specular_texture_name) {
                material.specular_map.texture =
                    &m_textures->default_specular_texture();
                material.specular_map_name.assign(
                    default_specular_texture_name);
            } else {
                auto texture = m_textures->acquire(
                    config.specular_map_name.view(),
                    true);
                if (!texture) {
                    if (diffuse_acquired) {
                        m_textures->release(
                            material.diffuse_map_name.view());
                    }
                    material = {};
                    return err(material_error{
                        material_error_code::texture_failed,
                        texture.error().native_code,
                    });
                }
                material.specular_map.texture = *texture;
                specular_acquired = true;
            }
        }

        material.generation = 0;
        auto created = m_shaders->acquire_instance(material.shader);
        if (!created) {
            if (specular_acquired)
                m_textures->release(material.specular_map_name.view());
            if (diffuse_acquired)
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
        const strbuf<texture_name_capacity> specular_texture_name =
            material.specular_map_name;
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
        if (!specular_texture_name.empty() &&
            specular_texture_name.view() != default_specular_texture_name) {
            m_textures->release(specular_texture_name.view());
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
