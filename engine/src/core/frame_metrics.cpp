#include "nkpch.h"

#include "core/frame_metrics.h"

namespace nk {
    void FrameMetrics::add(const FrameMetricSample& sample) noexcept {
        m_sums.frame_ms += sample.frame_ms;
        m_sums.cpu_ms += sample.cpu_ms;
        m_sums.update_ms += sample.update_ms;
        m_sums.build_ms += sample.build_ms;
        m_sums.render_ms += sample.render_ms;
        if (sample.gpu.valid()) {
            m_sums.gpu_ms += sample.gpu.milliseconds;
            ++m_gpu_count;
        }
        if (sample.picking.valid()) {
            m_sums.picking_frames += sample.picking.frames;
            ++m_picking_count;
        }
    }

    void FrameMetrics::subtract(const FrameMetricSample& sample) noexcept {
        m_sums.frame_ms -= sample.frame_ms;
        m_sums.cpu_ms -= sample.cpu_ms;
        m_sums.update_ms -= sample.update_ms;
        m_sums.build_ms -= sample.build_ms;
        m_sums.render_ms -= sample.render_ms;
        if (sample.gpu.valid()) {
            m_sums.gpu_ms -= sample.gpu.milliseconds;
            --m_gpu_count;
        }
        if (sample.picking.valid()) {
            m_sums.picking_frames -= sample.picking.frames;
            --m_picking_count;
        }
    }

    void FrameMetrics::push(const FrameMetricSample sample) noexcept {
        if (m_count == window_capacity)
            subtract(m_samples[m_next]);
        else
            ++m_count;
        m_samples[m_next] = sample;
        add(sample);
        m_next = (m_next + 1) % window_capacity;
    }

    FrameMetricsSnapshot FrameMetrics::snapshot() const noexcept {
        FrameMetricsSnapshot result{};
        if (m_count == 0)
            return result;
        const f64 count = static_cast<f64>(m_count);
        result.latest = m_samples[
            (m_next + window_capacity - 1) % window_capacity];
        result.sample_count = m_count;
        result.gpu_sample_count = m_gpu_count;
        result.picking_sample_count = m_picking_count;
        result.average_frame_ms = m_sums.frame_ms / count;
        result.average_fps = result.average_frame_ms > 0.0
            ? 1000.0 / result.average_frame_ms
            : 0.0;
        result.average_cpu_ms = m_sums.cpu_ms / count;
        result.average_update_ms = m_sums.update_ms / count;
        result.average_build_ms = m_sums.build_ms / count;
        result.average_render_ms = m_sums.render_ms / count;
        result.average_gpu_ms = m_gpu_count == 0
            ? -1.0
            : m_sums.gpu_ms / static_cast<f64>(m_gpu_count);
        result.average_picking_frames = m_picking_count == 0
            ? -1.0
            : m_sums.picking_frames / static_cast<f64>(m_picking_count);
        return result;
    }

    void FrameMetrics::reset() noexcept {
        for (FrameMetricSample& sample : m_samples)
            sample = {};
        m_sums = {};
        m_next = 0;
        m_count = 0;
        m_gpu_count = 0;
        m_picking_count = 0;
    }
}
