#include <gtest/gtest.h>

#include "core/frame_metrics.h"

TEST(FrameMetrics, AveragesTheFirstSamplesWithoutAllocations) {
    nk::FrameMetrics metrics;
    metrics.push({
        .frame_number = 7,
        .frame_ms = 10.0,
        .cpu_ms = 8.0,
        .update_ms = 2.0,
        .build_ms = 1.0,
        .render_ms = 5.0,
        .gpu = {4, 6.0},
        .picking = {3, 5, 2.0},
        .draw = {10, 8, 2, 9},
    });
    const auto snapshot = metrics.snapshot();
    EXPECT_EQ(snapshot.sample_count, 1u);
    EXPECT_EQ(snapshot.latest.frame_number, 7u);
    EXPECT_DOUBLE_EQ(snapshot.average_frame_ms, 10.0);
    EXPECT_DOUBLE_EQ(snapshot.average_fps, 100.0);
    EXPECT_DOUBLE_EQ(snapshot.average_gpu_ms, 6.0);
    EXPECT_EQ(snapshot.latest.gpu.frame_number, 4u);
    EXPECT_EQ(snapshot.latest.draw.culled, 2u);
}

TEST(FrameMetrics, ReplacesTheOldestSampleInConstantTimeWindow) {
    nk::FrameMetrics metrics;
    for (nk::u32 index = 0;
         index < nk::FrameMetrics::window_capacity + 5;
         ++index) {
        metrics.push({
            .frame_number = index,
            .frame_ms = static_cast<nk::f64>(index + 1),
        });
    }
    const auto snapshot = metrics.snapshot();
    EXPECT_EQ(snapshot.sample_count, nk::FrameMetrics::window_capacity);
    EXPECT_EQ(snapshot.latest.frame_number, 34u);
    EXPECT_DOUBLE_EQ(snapshot.average_frame_ms, 20.5);
}

TEST(FrameMetrics, AveragesOnlyValidDelayedSamplesAndResets) {
    nk::FrameMetrics metrics;
    metrics.push({.frame_number = 1, .frame_ms = 5.0});
    metrics.push({
        .frame_number = 2,
        .frame_ms = 15.0,
        .gpu = {0, 4.0},
        .picking = {1, 4, 3.0},
    });
    auto snapshot = metrics.snapshot();
    EXPECT_EQ(snapshot.gpu_sample_count, 1u);
    EXPECT_EQ(snapshot.picking_sample_count, 1u);
    EXPECT_DOUBLE_EQ(snapshot.average_gpu_ms, 4.0);
    EXPECT_DOUBLE_EQ(snapshot.average_picking_frames, 3.0);

    metrics.reset();
    snapshot = metrics.snapshot();
    EXPECT_EQ(snapshot.sample_count, 0u);
    EXPECT_LT(snapshot.average_gpu_ms, 0.0);
}
