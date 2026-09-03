#include "nkpch.h"

#include "systems/shader_system.h"

#include "memory/allocator.h"
#include "resources/shader_resource.h"
#include "systems/resource_system.h"

namespace nk {
    namespace {
        shader_system_error resource_failure(
            const resource_error error) noexcept {
            if (error.code == resource_error_code::invalid_data) {
                return {
                    shader_system_error_code::invalid_config,
                    error.native_code,
                };
            }
            if (error.code == resource_error_code::out_of_memory) {
                return {
                    shader_system_error_code::out_of_memory,
                    error.native_code,
                };
            }
            if (error.code == resource_error_code::invalid_name) {
                return {
                    shader_system_error_code::invalid_name,
                    error.native_code,
                };
            }
            return {
                shader_system_error_code::resource_failed,
                static_cast<i32>(error.code),
            };
        }
    }

    ShaderSystem::~ShaderSystem() {
        shutdown();
    }

    result<ShaderSystem*, shader_system_error> ShaderSystem::create(
        mem::Allocator& allocator,
        Renderer& renderer,
        ResourceSystem& resources,
        const ShaderSystemConfig config) {
        ShaderSystem* system = allocator.construct_t(ShaderSystem);
        if (system == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::out_of_memory,
                0,
            });
        }

        auto initialized = system->init(
            allocator,
            renderer,
            resources,
            config);
        if (!initialized) {
            const shader_system_error error = initialized.error();
            allocator.deconstruct_t(ShaderSystem, system);
            return err(error);
        }
        return ok(system);
    }

    void ShaderSystem::destroy(
        mem::Allocator& allocator,
        ShaderSystem* system) {
        if (system != nullptr)
            allocator.deconstruct_t(ShaderSystem, system);
    }

    result<void, shader_system_error> ShaderSystem::init(
        mem::Allocator& allocator,
        Renderer& renderer,
        ResourceSystem& resources,
        const ShaderSystemConfig config) {
        if (config.max_shader_count == 0 ||
            config.max_shader_count == numeric::u16_max ||
            config.max_uniform_count == 0 ||
            config.max_uniform_count == numeric::u16_max) {
            return err(shader_system_error{
                shader_system_error_code::invalid_config,
                0,
            });
        }

        m_allocator = &allocator;
        m_renderer = &renderer;
        m_resources = &resources;
        m_config = config;
        if (!m_shaders.arr_init(&allocator, config.max_shader_count)) {
            shutdown();
            return err(shader_system_error{
                shader_system_error_code::out_of_memory,
                0,
            });
        }
        auto lookup_initialized = m_shader_lookup.map_init(
            &allocator,
            config.max_shader_count,
            hash_seed::deterministic);
        if (!lookup_initialized) {
            shutdown();
            return err(shader_system_error{
                shader_system_error_code::out_of_memory,
                0,
            });
        }

        m_initialized = true;
        InfoLog(
            "Shader system initialized with capacity {}.",
            config.max_shader_count);
        return ok();
    }

    void ShaderSystem::shutdown() noexcept {
        if (m_renderer != nullptr) {
            for (ShaderRecord& record : m_shaders) {
                if (!record.occupied)
                    continue;
                auto destroyed = m_renderer->destroy_shader(record.handle);
                if (!destroyed) {
                    ErrorLog(
                        "Failed to destroy shader '{}': renderer_error={}, native_code={}.",
                        record.name.view(),
                        static_cast<u32>(destroyed.error().code),
                        destroyed.error().native_code);
                }
                reset_record(record);
            }
        }

        if (m_shader_lookup.allocator() != nullptr)
            (void)m_shader_lookup.map_shutdown();
        if (m_shaders.allocator() != nullptr)
            (void)m_shaders.arr_shutdown();

        m_current_shader = {};
        m_loaded_count = 0;
        m_initialized = false;
        m_config = {};
        m_resources = nullptr;
        m_renderer = nullptr;
        m_allocator = nullptr;
    }

    result<ShaderHandle, shader_system_error> ShaderSystem::load(
        const strview resource_name) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (resource_name.empty()) {
            return err(shader_system_error{
                shader_system_error_code::invalid_name,
                0,
            });
        }

        auto resource = m_resources->load(
            resource_name,
            ResourceType::shader);
        if (!resource)
            return err(resource_failure(resource.error()));

        const ShaderResourceConfig* loaded =
            resource->as<ShaderResourceConfig>();
        if (loaded == nullptr) {
            (void)m_resources->unload(*resource);
            return err(shader_system_error{
                shader_system_error_code::invalid_config,
                0,
            });
        }

        auto created = create(loaded->config(), loaded->render_pass());
        auto unloaded = m_resources->unload(*resource);
        if (!unloaded) {
            if (created)
                (void)destroy(*created);
            return err(resource_failure(unloaded.error()));
        }
        return created;
    }

    result<ShaderHandle, shader_system_error> ShaderSystem::create(
        const ShaderConfig& config,
        const RenderPassKind render_pass) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (config.name.empty() || config.name.length() > 127) {
            return err(shader_system_error{
                shader_system_error_code::invalid_name,
                0,
            });
        }
        if (m_shader_lookup.contains(config.name)) {
            return err(shader_system_error{
                shader_system_error_code::duplicate_shader,
                0,
            });
        }
        if (config.uniforms.length() > m_config.max_uniform_count) {
            return err(shader_system_error{
                shader_system_error_code::capacity_exceeded,
                0,
            });
        }

        u32 global_samplers = 0;
        u32 instance_samplers = 0;
        for (const ShaderUniformConfig& uniform : config.uniforms) {
            if (uniform.type != ShaderUniformType::sampler_2d)
                continue;
            u32* count = uniform.scope == ShaderScope::global
                ? &global_samplers
                : &instance_samplers;
            if (uniform.scope == ShaderScope::local ||
                *count > numeric::u32_max - uniform.array_length) {
                return err(shader_system_error{
                    shader_system_error_code::invalid_config,
                    static_cast<i32>(shader_config_error::invalid_uniform),
                });
            }
            *count += uniform.array_length;
        }
        if (global_samplers > m_config.max_global_samplers ||
            instance_samplers > m_config.max_instance_samplers) {
            return err(shader_system_error{
                shader_system_error_code::capacity_exceeded,
                0,
            });
        }

        auto validated = validate_shader_config(config);
        if (!validated) {
            return err(shader_system_error{
                shader_system_error_code::invalid_config,
                static_cast<i32>(validated.error()),
            });
        }

        const u16 slot = find_free_slot();
        if (slot == numeric::u16_max) {
            return err(shader_system_error{
                shader_system_error_code::capacity_exceeded,
                0,
            });
        }

        ShaderRecord& record = m_shaders[slot];
        if (!record.name.assign(config.name)) {
            return err(shader_system_error{
                shader_system_error_code::invalid_name,
                0,
            });
        }
        auto uniform_lookup_initialized = record.uniform_lookup.map_init(
            m_allocator,
            config.uniforms.length(),
            hash_seed::deterministic);
        if (!uniform_lookup_initialized) {
            reset_record(record);
            return err(shader_system_error{
                shader_system_error_code::out_of_memory,
                0,
            });
        }
        auto metadata_initialized = record.metadata.init(m_allocator, config);
        if (!metadata_initialized) {
            const shader_system_error error =
                from_renderer(metadata_initialized.error());
            reset_record(record);
            return err(error);
        }

        for (u64 index = 0; index < config.uniforms.length(); ++index) {
            str uniform_name{*m_allocator};
            if (!uniform_name.assign(config.uniforms[index].name)) {
                reset_record(record);
                return err(shader_system_error{
                    shader_system_error_code::out_of_memory,
                    0,
                });
            }
            auto inserted = record.uniform_lookup.try_emplace(
                std::move(uniform_name),
                ShaderUniformHandle{static_cast<u16>(index)});
            if (!inserted) {
                reset_record(record);
                return err(shader_system_error{
                    shader_system_error_code::out_of_memory,
                    0,
                });
            }
            if (*inserted != cl::insert_outcome::inserted) {
                reset_record(record);
                return err(shader_system_error{
                    shader_system_error_code::invalid_config,
                    static_cast<i32>(
                        shader_config_error::duplicate_uniform_name),
                });
            }
        }

        str shader_name{*m_allocator};
        if (!shader_name.assign(config.name)) {
            reset_record(record);
            return err(shader_system_error{
                shader_system_error_code::out_of_memory,
                0,
            });
        }
        auto inserted = m_shader_lookup.try_emplace(
            std::move(shader_name),
            slot);
        if (!inserted) {
            reset_record(record);
            return err(shader_system_error{
                shader_system_error_code::out_of_memory,
                0,
            });
        }
        if (*inserted != cl::insert_outcome::inserted) {
            reset_record(record);
            return err(shader_system_error{
                shader_system_error_code::duplicate_shader,
                0,
            });
        }

        auto renderer_shader = m_renderer->create_shader(config, render_pass);
        if (!renderer_shader) {
            m_shader_lookup.remove(config.name);
            const shader_system_error error =
                from_renderer(renderer_shader.error());
            reset_record(record);
            return err(error);
        }
        if (!renderer_shader->valid() || find_record(*renderer_shader) != nullptr) {
            if (renderer_shader->valid())
                (void)m_renderer->destroy_shader(*renderer_shader);
            m_shader_lookup.remove(config.name);
            reset_record(record);
            return err(shader_system_error{
                shader_system_error_code::renderer_failed,
                0,
            });
        }

        record.handle = *renderer_shader;
        record.occupied = true;
        ++m_loaded_count;
        return ok(record.handle);
    }

    result<void, shader_system_error> ShaderSystem::destroy(
        const ShaderHandle shader) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        ShaderRecord* record = find_record(shader);
        if (record == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }

        auto destroyed = m_renderer->destroy_shader(shader);
        if (!destroyed)
            return err(from_renderer(destroyed.error()));

        m_shader_lookup.remove(record->name.view());
        if (m_current_shader == shader)
            m_current_shader = {};
        reset_record(*record);
        --m_loaded_count;
        return ok();
    }

    result<void, shader_system_error> ShaderSystem::destroy(
        const strview name) {
        auto found = handle(name);
        if (!found)
            return err(found.error());
        return destroy(*found);
    }

    result<ShaderHandle, shader_system_error> ShaderSystem::handle(
        const strview name) const {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (name.empty()) {
            return err(shader_system_error{
                shader_system_error_code::invalid_name,
                0,
            });
        }
        const u16* slot = m_shader_lookup.find(name);
        if (slot == nullptr || *slot >= m_shaders.length() ||
            !m_shaders[*slot].occupied) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        return ok(m_shaders[*slot].handle);
    }

    result<ShaderUniformHandle, shader_system_error> ShaderSystem::uniform(
        const ShaderHandle shader,
        const strview name) const {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        const ShaderRecord* record = find_record(shader);
        if (record == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        if (name.empty()) {
            return err(shader_system_error{
                shader_system_error_code::uniform_not_found,
                0,
            });
        }
        const ShaderUniformHandle* found = record->uniform_lookup.find(name);
        if (found == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::uniform_not_found,
                0,
            });
        }
        return ok(*found);
    }

    result<ShaderUniformMetadata, shader_system_error>
    ShaderSystem::uniform_metadata(
        const ShaderHandle shader,
        const ShaderUniformHandle uniform) const {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        const ShaderRecord* record = find_record(shader);
        if (record == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        const ShaderUniformMetadata* metadata = record->metadata.uniform(uniform);
        if (metadata == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::uniform_not_found,
                0,
            });
        }
        return ok(*metadata);
    }

    result<void, shader_system_error> ShaderSystem::use(
        const ShaderHandle shader) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (find_record(shader) == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        auto used = m_renderer->use_shader(shader);
        if (!used)
            return err(from_renderer(used.error()));
        m_current_shader = shader;
        return ok();
    }

    result<void, shader_system_error> ShaderSystem::use(const strview name) {
        auto found = handle(name);
        if (!found)
            return err(found.error());
        return use(*found);
    }

    result<void, shader_system_error> ShaderSystem::bind_globals() {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (find_record(m_current_shader) == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        auto bound = m_renderer->bind_shader_globals(m_current_shader);
        if (!bound)
            return err(from_renderer(bound.error()));
        return ok();
    }

    result<void, shader_system_error> ShaderSystem::bind_instance(
        const u32 instance_id) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (find_record(m_current_shader) == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        auto bound = m_renderer->bind_shader_instance(
            m_current_shader,
            instance_id);
        if (!bound)
            return err(from_renderer(bound.error()));
        return ok();
    }

    result<void, shader_system_error> ShaderSystem::apply_globals() {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (find_record(m_current_shader) == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        auto applied = m_renderer->apply_shader_globals(m_current_shader);
        if (!applied)
            return err(from_renderer(applied.error()));
        return ok();
    }

    result<void, shader_system_error> ShaderSystem::apply_instance(
        const bool needs_update) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (find_record(m_current_shader) == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        auto applied = m_renderer->apply_shader_instance(
            m_current_shader,
            needs_update);
        if (!applied)
            return err(from_renderer(applied.error()));
        return ok();
    }

    result<u32, shader_system_error> ShaderSystem::acquire_instance(
        const ShaderHandle shader) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (find_record(shader) == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        auto acquired = m_renderer->acquire_shader_instance(shader);
        if (!acquired)
            return err(from_renderer(acquired.error()));
        return ok(*acquired);
    }

    result<void, shader_system_error> ShaderSystem::release_instance(
        const ShaderHandle shader,
        const u32 instance_id) {
        if (!m_initialized) {
            return err(shader_system_error{
                shader_system_error_code::not_initialized,
                0,
            });
        }
        if (find_record(shader) == nullptr) {
            return err(shader_system_error{
                shader_system_error_code::shader_not_found,
                0,
            });
        }
        auto released = m_renderer->release_shader_instance(
            shader,
            instance_id);
        if (!released)
            return err(from_renderer(released.error()));
        return ok();
    }

    result<void, shader_system_error> ShaderSystem::set_uniform_custom(
        const ShaderUniformHandle uniform,
        const void* data,
        const u32 size) {
        const ShaderUniformMetadata* metadata = current_uniform(uniform);
        if (metadata == nullptr)
            return err(current_uniform_error(uniform));
        if (metadata->type != ShaderUniformType::custom ||
            metadata->size != size || data == nullptr || size == 0) {
            return err(shader_system_error{
                shader_system_error_code::invalid_uniform,
                0,
            });
        }
        auto set = m_renderer->set_shader_uniform_custom(
            m_current_shader,
            uniform,
            data,
            size);
        if (!set)
            return err(from_renderer(set.error()));
        return ok();
    }

    result<void, shader_system_error> ShaderSystem::set_sampler(
        const ShaderUniformHandle uniform,
        Texture* texture,
        const u32 array_index) {
        const ShaderUniformMetadata* metadata = current_uniform(uniform);
        if (metadata == nullptr)
            return err(current_uniform_error(uniform));
        if (metadata->type != ShaderUniformType::sampler_2d ||
            array_index >= metadata->array_length) {
            return err(shader_system_error{
                shader_system_error_code::invalid_uniform,
                0,
            });
        }
        auto set = m_renderer->set_shader_sampler(
            m_current_shader,
            uniform,
            texture,
            array_index);
        if (!set)
            return err(from_renderer(set.error()));
        return ok();
    }

    void ShaderSystem::reset_record(ShaderRecord& record) noexcept {
        if (record.uniform_lookup.allocator() != nullptr)
            (void)record.uniform_lookup.map_shutdown();
        record.metadata.shutdown();
        record.name.clear();
        record.handle = {};
        record.occupied = false;
    }

    u16 ShaderSystem::find_free_slot() const noexcept {
        for (u16 index = 0; index < m_shaders.length(); ++index) {
            if (!m_shaders[index].occupied)
                return index;
        }
        return numeric::u16_max;
    }

    ShaderSystem::ShaderRecord* ShaderSystem::find_record(
        const ShaderHandle shader) noexcept {
        for (ShaderRecord& record : m_shaders) {
            if (record.occupied && record.handle == shader)
                return &record;
        }
        return nullptr;
    }

    const ShaderSystem::ShaderRecord* ShaderSystem::find_record(
        const ShaderHandle shader) const noexcept {
        for (const ShaderRecord& record : m_shaders) {
            if (record.occupied && record.handle == shader)
                return &record;
        }
        return nullptr;
    }

    const ShaderUniformMetadata* ShaderSystem::current_uniform(
        const ShaderUniformHandle uniform) const noexcept {
        if (!m_initialized)
            return nullptr;
        const ShaderRecord* record = find_record(m_current_shader);
        return record == nullptr ? nullptr : record->metadata.uniform(uniform);
    }

    shader_system_error ShaderSystem::current_uniform_error(
        const ShaderUniformHandle uniform) const noexcept {
        if (!m_initialized) {
            return {
                shader_system_error_code::not_initialized,
                0,
            };
        }
        if (find_record(m_current_shader) == nullptr) {
            return {
                shader_system_error_code::shader_not_found,
                0,
            };
        }
        static_cast<void>(uniform);
        return {
            shader_system_error_code::uniform_not_found,
            0,
        };
    }

    shader_system_error ShaderSystem::from_renderer(
        const renderer_error error) noexcept {
        if (error.code == renderer_error_code::out_of_memory) {
            return {
                shader_system_error_code::out_of_memory,
                error.native_code,
            };
        }
        if (error.code == renderer_error_code::shader_config_invalid) {
            return {
                shader_system_error_code::invalid_config,
                error.native_code,
            };
        }
        return {
            shader_system_error_code::renderer_failed,
            error.native_code,
        };
    }
}
