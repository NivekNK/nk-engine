#pragma once

#include "resources/resource_loader.h"

namespace nk {
    class TextResourceLoader final : public ResourceLoader {
    public:
        [[nodiscard]] ResourceType type() const noexcept override {
            return ResourceType::text;
        }
        [[nodiscard]] strview type_path() const noexcept override { return {}; }
        [[nodiscard]] result<void, resource_error> load(
            mem::Allocator& allocator,
            strview asset_base_path,
            strview name,
            Resource& out_resource) const override;
        void unload(
            mem::Allocator& allocator,
            Resource& resource) const noexcept override;
    };

    class BinaryResourceLoader final : public ResourceLoader {
    public:
        [[nodiscard]] ResourceType type() const noexcept override {
            return ResourceType::binary;
        }
        [[nodiscard]] strview type_path() const noexcept override { return {}; }
        [[nodiscard]] result<void, resource_error> load(
            mem::Allocator& allocator,
            strview asset_base_path,
            strview name,
            Resource& out_resource) const override;
        void unload(
            mem::Allocator& allocator,
            Resource& resource) const noexcept override;
    };

    class ImageResourceLoader final : public ResourceLoader {
    public:
        [[nodiscard]] ResourceType type() const noexcept override {
            return ResourceType::image;
        }
        [[nodiscard]] strview type_path() const noexcept override {
            return {"textures", 8};
        }
        [[nodiscard]] result<void, resource_error> load(
            mem::Allocator& allocator,
            strview asset_base_path,
            strview name,
            Resource& out_resource) const override;
        void unload(
            mem::Allocator& allocator,
            Resource& resource) const noexcept override;
    };

    class MaterialResourceLoader final : public ResourceLoader {
    public:
        [[nodiscard]] ResourceType type() const noexcept override {
            return ResourceType::material;
        }
        [[nodiscard]] strview type_path() const noexcept override {
            return {"materials", 9};
        }
        [[nodiscard]] result<void, resource_error> load(
            mem::Allocator& allocator,
            strview asset_base_path,
            strview name,
            Resource& out_resource) const override;
        void unload(
            mem::Allocator& allocator,
            Resource& resource) const noexcept override;
    };

    class ShaderResourceLoader final : public ResourceLoader {
    public:
        [[nodiscard]] ResourceType type() const noexcept override {
            return ResourceType::shader;
        }
        [[nodiscard]] strview type_path() const noexcept override {
            return {"shaders", 7};
        }
        [[nodiscard]] result<void, resource_error> load(
            mem::Allocator& allocator,
            strview asset_base_path,
            strview name,
            Resource& out_resource) const override;
        void unload(
            mem::Allocator& allocator,
            Resource& resource) const noexcept override;
    };

    class StaticMeshResourceLoader final : public ResourceLoader {
    public:
        [[nodiscard]] ResourceType type() const noexcept override {
            return ResourceType::static_mesh;
        }
        [[nodiscard]] strview type_path() const noexcept override {
            return {"models", 6};
        }
        [[nodiscard]] result<void, resource_error> load(
            mem::Allocator& allocator,
            strview asset_base_path,
            strview name,
            Resource& out_resource) const override;
        void unload(
            mem::Allocator& allocator,
            Resource& resource) const noexcept override;
    };
}
