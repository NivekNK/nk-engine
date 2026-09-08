#include <gtest/gtest.h>

#include "core/app.h"
#include "memory/linear_allocator.h"

namespace {
    enum class Call : nk::u8 {
        initialize,
        update,
        resize,
        build,
        complete,
        shutdown,
    };

    class ProbeApp final : public nk::App {
    public:
        explicit ProbeApp(const bool fail_initialize = false)
            : App({
                  .name = "Probe",
                  .start_pos_x = 0,
                  .start_pos_y = 0,
                  .start_width = 1,
                  .start_height = 1,
              }),
              m_fail_initialize{fail_initialize} {}

        nk::result<void, nk::app_error> start(nk::AppServices& services) {
            return initialize(services);
        }
        nk::result<void, nk::app_error> tick(const nk::FrameContext& frame) {
            return update(frame);
        }
        nk::result<void, nk::app_error> build(
            const nk::FrameContext& frame,
            nk::FrameBuilder& builder) {
            return build_frame(frame, builder);
        }
        void resize(const nk::u32 width, const nk::u32 height) {
            on_resized(width, height);
        }
        void complete(const nk::frame_outcome outcome) {
            frame_complete(outcome);
        }
        void stop() { shutdown(); }

        Call calls[8]{};
        nk::u32 call_count = 0;
        void* frame_allocation = nullptr;

    private:
        void record(const Call call) { calls[call_count++] = call; }

        nk::result<void, nk::app_error> initialize(
            nk::AppServices&) override {
            record(Call::initialize);
            if (m_fail_initialize) {
                return nk::err(nk::app_error{
                    nk::app_error_code::initialization_failed,
                    17,
                });
            }
            return nk::ok();
        }
        nk::result<void, nk::app_error> update(
            const nk::FrameContext&) override {
            record(Call::update);
            return nk::ok();
        }
        nk::result<void, nk::app_error> build_frame(
            const nk::FrameContext&,
            nk::FrameBuilder& builder) override {
            record(Call::build);
            frame_allocation = builder.scratch()._allocate_raw(32, 16);
            if (frame_allocation == nullptr) {
                return nk::err(nk::app_error{
                    nk::app_error_code::frame_build_failed});
            }
            return nk::ok();
        }
        void on_resized(nk::u32, nk::u32) override {
            record(Call::resize);
        }
        void frame_complete(nk::frame_outcome) noexcept override {
            record(Call::complete);
        }
        void shutdown() noexcept override { record(Call::shutdown); }

        bool m_fail_initialize = false;
    };
}

TEST(ApplicationLifecycle, PreservesTheTypedFrameCallOrder) {
    alignas(64) nk::u8 storage[256]{};
    nk::mem::LinearAllocator scratch{
        nk::mem::untracked,
        sizeof(storage),
        storage};
    nk::AppServices services;
    nk::FrameContext context{.delta_time = 0.25};
    ProbeApp app;

    ASSERT_TRUE(app.start(services));
    ASSERT_TRUE(app.tick(context));
    app.resize(800, 600);
    nk::FrameBuilder builder{scratch, context.delta_time};
    ASSERT_TRUE(app.build(context, builder));
    app.complete(nk::frame_outcome::rendered);
    app.stop();
    ASSERT_TRUE(scratch.reset(nk::mem::LinearResetMode::RetainContents));

    constexpr Call expected[]{
        Call::initialize,
        Call::update,
        Call::resize,
        Call::build,
        Call::complete,
        Call::shutdown,
    };
    ASSERT_EQ(app.call_count, 6u);
    for (nk::u32 index = 0; index < app.call_count; ++index)
        EXPECT_EQ(app.calls[index], expected[index]);
    EXPECT_DOUBLE_EQ(builder.packet.delta_time, context.delta_time);
}

TEST(ApplicationLifecycle, AllowsRollbackImmediatelyAfterInitializationFailure) {
    nk::AppServices services;
    ProbeApp app{true};

    auto initialized = app.start(services);
    ASSERT_FALSE(initialized);
    EXPECT_EQ(initialized.error().native_code, 17);
    app.stop();

    ASSERT_EQ(app.call_count, 2u);
    EXPECT_EQ(app.calls[0], Call::initialize);
    EXPECT_EQ(app.calls[1], Call::shutdown);
}

TEST(ApplicationLifecycle, ReusesFrameScratchOnlyAfterAnExplicitReset) {
    alignas(64) nk::u8 storage[128]{};
    nk::mem::LinearAllocator scratch{
        nk::mem::untracked,
        sizeof(storage),
        storage};
    nk::FrameContext context{};
    ProbeApp app;

    nk::FrameBuilder first{scratch, 0.0};
    ASSERT_TRUE(app.build(context, first));
    void* first_allocation = app.frame_allocation;
    EXPECT_EQ(scratch.get_active_allocation_count(), 1u);

    ASSERT_TRUE(scratch.reset(nk::mem::LinearResetMode::RetainContents));
    nk::FrameBuilder second{scratch, 0.0};
    ASSERT_TRUE(app.build(context, second));
    EXPECT_EQ(app.frame_allocation, first_allocation);
    EXPECT_EQ(scratch.get_active_allocation_count(), 1u);
    EXPECT_TRUE(scratch.reset(nk::mem::LinearResetMode::RetainContents));
}
