#pragma once

#include "core/defines.h"

namespace nk {
    struct DelayedFrameTiming {
        u64 frame_number = 0;
        f64 milliseconds = -1.0;

        [[nodiscard]] bool valid() const noexcept {
            return milliseconds >= 0.0;
        }
    };

    struct PickingFrameLatency {
        u64 request_frame = 0;
        u64 retired_frame = 0;
        f64 frames = -1.0;

        [[nodiscard]] bool valid() const noexcept {
            return frames >= 0.0;
        }
    };

    struct FrameDrawCounters {
        u32 candidates = 0;
        u32 visible = 0;
        u32 culled = 0;
        u32 draws = 0;
    };

    struct FrameMetricSample {
        u64 frame_number = 0;
        f64 frame_ms = 0.0;
        f64 cpu_ms = 0.0;
        f64 update_ms = 0.0;
        f64 build_ms = 0.0;
        f64 render_ms = 0.0;
        DelayedFrameTiming gpu{};
        PickingFrameLatency picking{};
        FrameDrawCounters draw{};
    };

    struct FrameMetricsSnapshot {
        FrameMetricSample latest{};
        u32 sample_count = 0;
        u32 gpu_sample_count = 0;
        u32 picking_sample_count = 0;
        f64 average_frame_ms = 0.0;
        f64 average_fps = 0.0;
        f64 average_cpu_ms = 0.0;
        f64 average_update_ms = 0.0;
        f64 average_build_ms = 0.0;
        f64 average_render_ms = 0.0;
        f64 average_gpu_ms = -1.0;
        f64 average_picking_frames = -1.0;
    };

    // Allocation-free moving window. All averages are updated in O(1), and
    // delayed GPU/picking samples retain the frame identity they measured.
    class FrameMetrics final {
    public:
        static constexpr u32 window_capacity = 30;

        void push(FrameMetricSample sample) noexcept;
        [[nodiscard]] FrameMetricsSnapshot snapshot() const noexcept;
        void reset() noexcept;

    private:
        struct Sums {
            f64 frame_ms = 0.0;
            f64 cpu_ms = 0.0;
            f64 update_ms = 0.0;
            f64 build_ms = 0.0;
            f64 render_ms = 0.0;
            f64 gpu_ms = 0.0;
            f64 picking_frames = 0.0;
        };

        void add(const FrameMetricSample& sample) noexcept;
        void subtract(const FrameMetricSample& sample) noexcept;

        FrameMetricSample m_samples[window_capacity]{};
        Sums m_sums{};
        u32 m_next = 0;
        u32 m_count = 0;
        u32 m_gpu_count = 0;
        u32 m_picking_count = 0;
    };
}
