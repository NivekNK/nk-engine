#pragma once

#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_float3.hpp>

#include "collections/arr.h"
#include "collections/map.h"
#include "collections/slice.h"
#include "core/result.h"
#include "core/str.h"
#include "renderer/geometry_render_data.h"
#include "renderer/lighting.h"
#include "renderer/shader_config.h"

namespace nk {
    class Mesh;
    namespace mem { class Allocator; }

    inline constexpr strview world_render_view_name{"world", 5};
    inline constexpr strview ui_render_view_name{"ui", 2};

    enum class RenderViewType : u8 {
        world,
        ui,
    };

    enum class ProjectionType : u8 {
        perspective,
        orthographic,
    };

    struct RenderViewHandle {
        u32 index = numeric::invalid_id;
        u32 generation = numeric::invalid_id;

        [[nodiscard]] bool valid() const noexcept {
            return index != numeric::invalid_id &&
                   generation != numeric::invalid_id;
        }

        bool operator==(const RenderViewHandle&) const noexcept = default;
    };

    struct RenderViewConfig {
        strview name;
        RenderViewType type = RenderViewType::world;
        RenderPassKind pass = RenderPassKind::world;
        ProjectionType projection = ProjectionType::perspective;
        u8 order = 0;
        f32 field_of_view_radians = 0.7853981633974483f;
        f32 near_clip = 0.1f;
        f32 far_clip = 1000.0f;
    };

    struct RenderViewBuildData {
        const glm::mat4* world_view = nullptr;
        glm::vec3 world_view_position{};
        const SceneLighting* lighting = nullptr;
        u32 geometry_count = 0;
        const GeometryRenderData* geometries = nullptr;
        u32 mesh_count = 0;
        const Mesh* meshes = nullptr;
        u32 ui_geometry_count = 0;
        const GeometryRenderData* ui_geometries = nullptr;
    };

    struct RenderViewPacket {
        RenderViewHandle handle{};
        RenderViewType type = RenderViewType::world;
        RenderPassKind pass = RenderPassKind::world;
        glm::mat4 projection{1.0f};
        glm::mat4 view{1.0f};
        glm::vec3 view_position{};
        const SceneLighting* lighting = nullptr;
        u32 geometry_count = 0;
        const GeometryRenderData* geometries = nullptr;
        u32 mesh_count = 0;
        const Mesh* meshes = nullptr;
    };

    enum class render_view_error : u8 {
        already_initialized,
        not_initialized,
        invalid_name,
        invalid_configuration,
        invalid_extent,
        capacity_exceeded,
        duplicate_order,
        out_of_memory,
        invalid_handle,
        invalid_build_data,
        output_too_small,
    };

    class RenderViewSystem final {
    public:
        static constexpr u32 maximum_render_view_count = 8;

        RenderViewSystem() noexcept = default;
        ~RenderViewSystem();

        RenderViewSystem(const RenderViewSystem&) = delete;
        RenderViewSystem& operator=(const RenderViewSystem&) = delete;
        RenderViewSystem(RenderViewSystem&&) = delete;
        RenderViewSystem& operator=(RenderViewSystem&&) = delete;

        [[nodiscard]] result<void, render_view_error> init(
            mem::Allocator& allocator,
            u32 width,
            u32 height,
            u32 max_view_count = maximum_render_view_count);
        void shutdown() noexcept;

        [[nodiscard]] result<RenderViewHandle, render_view_error> create(
            const RenderViewConfig& config);
        [[nodiscard]] result<void, render_view_error> remove(
            RenderViewHandle handle);
        [[nodiscard]] result<void, render_view_error> resize(
            u32 width,
            u32 height) noexcept;
        [[nodiscard]] result<u32, render_view_error> build_packets(
            const RenderViewBuildData& input,
            cl::slice<RenderViewPacket> output) const noexcept;

        [[nodiscard]] RenderViewHandle handle(strview name) const noexcept;
        [[nodiscard]] bool valid(RenderViewHandle handle) const noexcept;
        [[nodiscard]] u32 view_count() const noexcept { return m_view_count; }
        [[nodiscard]] u32 width() const noexcept { return m_width; }
        [[nodiscard]] u32 height() const noexcept { return m_height; }

    private:
        struct ViewSlot {
            RenderViewConfig config{};
            glm::mat4 projection{1.0f};
            u32 generation = numeric::invalid_id;
            bool occupied = false;
        };

        [[nodiscard]] static bool valid_config(
            const RenderViewConfig& config) noexcept;
        [[nodiscard]] static glm::mat4 projection_for(
            const RenderViewConfig& config,
            u32 width,
            u32 height) noexcept;
        [[nodiscard]] u32 find_free_slot() const noexcept;
        [[nodiscard]] const ViewSlot* slot(RenderViewHandle handle) const noexcept;

        mem::Allocator* m_allocator = nullptr;
        cl::arr<ViewSlot> m_views;
        cl::map<str, RenderViewHandle> m_handles;
        u32 m_width = 0;
        u32 m_height = 0;
        u32 m_view_count = 0;
        bool m_initialized = false;
    };
}
