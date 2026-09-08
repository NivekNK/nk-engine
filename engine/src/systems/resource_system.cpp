#include "nkpch.h"

#include "systems/resource_system.h"

#include "memory/allocator.h"

namespace nk {
    ResourceSystem::~ResourceSystem() {
        shutdown();
    }

    result<ResourceSystem*, resource_error> ResourceSystem::create(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const u32 max_loader_count) {
        ResourceSystem* system = allocator.construct_t(ResourceSystem);
        if (system == nullptr)
            return err(resource_error{resource_error_code::out_of_memory, 0});

        auto initialized = system->init(
            allocator,
            asset_base_path,
            max_loader_count);
        if (!initialized) {
            const resource_error error = initialized.error();
            allocator.deconstruct_t(ResourceSystem, system);
            return err(error);
        }
        return ok(system);
    }

    void ResourceSystem::destroy(
        mem::Allocator& allocator,
        ResourceSystem* system) {
        if (system != nullptr)
            allocator.deconstruct_t(ResourceSystem, system);
    }

    result<void, resource_error> ResourceSystem::init(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const u32 max_loader_count) {
        if (asset_base_path.empty())
            return err(resource_error{resource_error_code::invalid_name, 0});
        if (max_loader_count < 6) {
            return err(resource_error{
                resource_error_code::capacity_exceeded,
                0,
            });
        }
        if (!m_asset_base_path.assign(asset_base_path))
            return err(resource_error{resource_error_code::invalid_name, 0});

        m_allocator = &allocator;
        if (!m_loaders.arr_init(&allocator, max_loader_count)) {
            shutdown();
            return err(resource_error{resource_error_code::out_of_memory, 0});
        }
        m_initialized = true;

        ResourceLoader* known_loaders[]{
            &m_text_loader,
            &m_binary_loader,
            &m_image_loader,
            &m_material_loader,
            &m_shader_loader,
            &m_static_mesh_loader,
        };
        for (ResourceLoader* loader : known_loaders) {
            auto registered = register_loader(*loader);
            if (!registered) {
                const resource_error error = registered.error();
                shutdown();
                return err(error);
            }
        }

        InfoLog(
            "Resource system initialized at '{}' with {} loaders.",
            m_asset_base_path.view(),
            m_registered_loader_count);
        return ok();
    }

    void ResourceSystem::shutdown() {
        if (m_active_resource_count != 0) {
            ErrorLog(
                "Resource system shutdown with {} resources still loaded.",
                m_active_resource_count);
        }
        if (m_loaders.allocator() != nullptr)
            (void)m_loaders.arr_shutdown();

        m_active_resource_count = 0;
        m_registered_loader_count = 0;
        m_initialized = false;
        m_asset_base_path.clear();
        m_allocator = nullptr;
    }

    result<void, resource_error> ResourceSystem::register_loader(
        ResourceLoader& loader) {
        if (!m_initialized)
            return err(resource_error{resource_error_code::not_initialized, 0});

        const ResourceType type = loader.type();
        if (type == ResourceType::unknown ||
            (type == ResourceType::custom && loader.custom_type().empty())) {
            return err(resource_error{resource_error_code::invalid_data, 0});
        }

        const u32 duplicate = type == ResourceType::custom
            ? find_custom_loader(loader.custom_type())
            : find_loader(type);
        if (duplicate != numeric::invalid_id) {
            return err(resource_error{
                resource_error_code::duplicate_loader,
                0,
            });
        }

        for (u32 index = 0; index < m_loaders.length(); ++index) {
            if (m_loaders[index] == nullptr) {
                m_loaders[index] = &loader;
                ++m_registered_loader_count;
                return ok();
            }
        }
        return err(resource_error{resource_error_code::capacity_exceeded, 0});
    }

    result<Resource, resource_error> ResourceSystem::load(
        const strview name,
        const ResourceType type) {
        if (!m_initialized)
            return err(resource_error{resource_error_code::not_initialized, 0});
        if (name.empty())
            return err(resource_error{resource_error_code::invalid_name, 0});

        const u32 loader_id = find_loader(type);
        if (loader_id == numeric::invalid_id)
            return err(resource_error{resource_error_code::no_loader, 0});
        return load_with(name, loader_id);
    }

    result<Resource, resource_error> ResourceSystem::load_custom(
        const strview name,
        const strview custom_type) {
        if (!m_initialized)
            return err(resource_error{resource_error_code::not_initialized, 0});
        if (name.empty() || custom_type.empty())
            return err(resource_error{resource_error_code::invalid_name, 0});

        const u32 loader_id = find_custom_loader(custom_type);
        if (loader_id == numeric::invalid_id)
            return err(resource_error{resource_error_code::no_loader, 0});
        return load_with(name, loader_id);
    }

    result<Resource, resource_error> ResourceSystem::load_with(
        const strview name,
        const u32 loader_id) {
        ResourceLoader* loader = m_loaders[loader_id];
        Resource resource;
        auto loaded = loader->load(
            *m_allocator,
            m_asset_base_path.view(),
            name,
            resource);
        if (!loaded) {
            if (resource.data != nullptr)
                loader->unload(*m_allocator, resource);
            return err(loaded.error());
        }
        if (resource.data == nullptr) {
            return err(resource_error{resource_error_code::invalid_data, 0});
        }

        resource.loader_id = loader_id;
        resource.type = loader->type();
        resource.owner = this;
        ++m_active_resource_count;
        return ok(std::move(resource));
    }

    result<void, resource_error> ResourceSystem::unload(Resource& resource) {
        if (!m_initialized)
            return err(resource_error{resource_error_code::not_initialized, 0});
        if (!resource.loaded())
            return ok();
        if (resource.owner != this || resource.loader_id >= m_loaders.length() ||
            m_loaders[resource.loader_id] == nullptr) {
            return err(resource_error{resource_error_code::foreign_resource, 0});
        }

        m_loaders[resource.loader_id]->unload(*m_allocator, resource);
        resource.reset();
        if (m_active_resource_count != 0)
            --m_active_resource_count;
        return ok();
    }

    result<Resource, resource_error> ResourceSystem::load_detached(
        mem::Allocator& allocator,
        const strview name,
        const ResourceType type) const {
        if (!m_initialized)
            return err(resource_error{resource_error_code::not_initialized, 0});
        if (name.empty())
            return err(resource_error{resource_error_code::invalid_name, 0});
        const u32 loader_id = find_loader(type);
        if (loader_id == numeric::invalid_id)
            return err(resource_error{resource_error_code::no_loader, 0});

        Resource resource;
        ResourceLoader* loader = m_loaders[loader_id];
        auto loaded = loader->load(
            allocator,
            m_asset_base_path.view(),
            name,
            resource);
        if (!loaded) {
            if (resource.data != nullptr)
                loader->unload(allocator, resource);
            return err(loaded.error());
        }
        if (resource.data == nullptr) {
            return err(resource_error{
                resource_error_code::invalid_data,
                0,
            });
        }
        resource.loader_id = loader_id;
        resource.type = loader->type();
        return ok(std::move(resource));
    }

    result<void, resource_error> ResourceSystem::unload_detached(
        mem::Allocator& allocator,
        Resource& resource) const noexcept {
        if (resource.data == nullptr)
            return ok();
        if (!m_initialized || resource.loader_id >= m_loaders.length() ||
            m_loaders[resource.loader_id] == nullptr) {
            return err(resource_error{
                resource_error_code::foreign_resource,
                0,
            });
        }
        m_loaders[resource.loader_id]->unload(allocator, resource);
        resource.reset();
        return ok();
    }

    u32 ResourceSystem::find_loader(const ResourceType type) const noexcept {
        if (type == ResourceType::unknown || type == ResourceType::custom)
            return numeric::invalid_id;
        for (u32 index = 0; index < m_loaders.length(); ++index) {
            if (m_loaders[index] != nullptr && m_loaders[index]->type() == type)
                return index;
        }
        return numeric::invalid_id;
    }

    u32 ResourceSystem::find_custom_loader(
        const strview custom_type) const noexcept {
        for (u32 index = 0; index < m_loaders.length(); ++index) {
            if (m_loaders[index] != nullptr &&
                m_loaders[index]->type() == ResourceType::custom &&
                m_loaders[index]->custom_type() == custom_type) {
                return index;
            }
        }
        return numeric::invalid_id;
    }
}
