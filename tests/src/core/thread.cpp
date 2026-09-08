#include <gtest/gtest.h>

#include "core/thread.h"

namespace {
    struct WorkerContext {
        nk::Mutex* mutex = nullptr;
        nk::ConditionVariable* condition = nullptr;
        bool ready = false;
        bool release = false;
        nk::u64 worker_id = 0;
    };

    nk::u32 wait_for_release(void* raw_context) noexcept {
        WorkerContext& context = *static_cast<WorkerContext*>(raw_context);
        auto lock = nk::LockGuard::acquire(*context.mutex);
        if (!lock)
            return 1;
        context.worker_id = nk::Thread::current_id();
        context.ready = true;
        if (!context.condition->notify_one())
            return 2;
        while (!context.release) {
            if (!context.condition->wait(*lock))
                return 3;
        }
        return 42;
    }

    nk::u32 immediate(void*) noexcept {
        return 7;
    }
}

TEST(Threading, StartsWaitsAndJoinsCooperatively) {
    nk::Mutex mutex;
    nk::ConditionVariable condition;
    ASSERT_TRUE(mutex.init());
    ASSERT_TRUE(condition.init());
    WorkerContext context{&mutex, &condition};
    nk::Thread worker;

    {
        auto lock = nk::LockGuard::acquire(mutex);
        ASSERT_TRUE(lock);
        ASSERT_TRUE(worker.start(wait_for_release, &context));
        EXPECT_TRUE(worker.joinable());
        EXPECT_NE(worker.id(), 0u);
        while (!context.ready)
            ASSERT_TRUE(condition.wait(*lock));
        EXPECT_NE(context.worker_id, 0u);
        EXPECT_NE(context.worker_id, nk::Thread::current_id());
        context.release = true;
        ASSERT_TRUE(condition.notify_all());
    }

    auto joined = worker.join();
    ASSERT_TRUE(joined);
    EXPECT_EQ(*joined, 42u);
    EXPECT_FALSE(worker.joinable());
    condition.shutdown();
    mutex.shutdown();
}

TEST(Threading, RejectsInvalidLifecycleTransitions) {
    nk::Mutex mutex;
    EXPECT_FALSE(mutex.lock());
    ASSERT_TRUE(mutex.init());
    auto duplicate_mutex_init = mutex.init();
    ASSERT_FALSE(duplicate_mutex_init);
    EXPECT_EQ(
        duplicate_mutex_init.error().code,
        nk::thread_error_code::invalid_state);

    nk::ConditionVariable condition;
    ASSERT_TRUE(condition.init());
    EXPECT_FALSE(condition.init());

    nk::Thread worker;
    EXPECT_FALSE(worker.start(nullptr, nullptr));
    ASSERT_TRUE(worker.start(immediate, nullptr));
    EXPECT_FALSE(worker.start(immediate, nullptr));
    auto joined = worker.join();
    ASSERT_TRUE(joined);
    EXPECT_EQ(*joined, 7u);
    EXPECT_FALSE(worker.join());
    EXPECT_GE(nk::Thread::logical_processor_count(), 1u);

    condition.shutdown();
    mutex.shutdown();
}
