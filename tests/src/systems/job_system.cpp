#include <gtest/gtest.h>

#include "core/thread.h"
#include "memory/malloc_allocator.h"
#include "systems/job_system.h"

namespace {
    struct ArithmeticPayload {
        int input = 0;
        int output = 0;
        nk::u64 worker_thread = 0;
        nk::u64* completion_thread = nullptr;
        nk::u64* published_worker_thread = nullptr;
        int* published_output = nullptr;
    };

    nk::result<void, nk::job_error> multiply(
        ArithmeticPayload& payload) noexcept {
        payload.worker_thread = nk::Thread::current_id();
        payload.output = payload.input * 3;
        return nk::ok();
    }

    void publish_arithmetic(
        ArithmeticPayload& payload,
        const nk::result<void, nk::job_error>& outcome) noexcept {
        if (!outcome)
            return;
        *payload.completion_thread = nk::Thread::current_id();
        if (payload.published_worker_thread != nullptr)
            *payload.published_worker_thread = payload.worker_thread;
        *payload.published_output = payload.output;
    }

    struct LifetimePayload {
        static inline int destroyed = 0;
        nk::u8 input[7]{};
        nk::u8 output[73]{};

        LifetimePayload() noexcept = default;
        LifetimePayload(const LifetimePayload&) = delete;
        LifetimePayload& operator=(const LifetimePayload&) = delete;
        LifetimePayload(LifetimePayload&&) noexcept = default;
        ~LifetimePayload() noexcept { ++destroyed; }
    };

    nk::result<void, nk::job_error> fill_output(
        LifetimePayload& payload) noexcept {
        for (nk::u32 index = 0; index < 73; ++index)
            payload.output[index] = static_cast<nk::u8>(index);
        return nk::ok();
    }

    struct CancelPayload {
        int* executions = nullptr;
        bool* cancelled = nullptr;
    };

    nk::result<void, nk::job_error> count_execution(
        CancelPayload& payload) noexcept {
        ++*payload.executions;
        return nk::ok();
    }

    void observe_cancel(
        CancelPayload& payload,
        const nk::result<void, nk::job_error>& outcome) noexcept {
        *payload.cancelled =
            !outcome && outcome.error().code == nk::job_error_code::cancelled;
    }

    struct Gate {
        nk::Mutex mutex;
        nk::ConditionVariable condition;
        bool started = false;
        bool release = false;
    };

    struct GatePayload {
        Gate* gate = nullptr;
    };

    nk::result<void, nk::job_error> wait_at_gate(
        GatePayload& payload) noexcept {
        auto lock = nk::LockGuard::acquire(payload.gate->mutex);
        if (!lock)
            return nk::err(nk::job_error{nk::job_error_code::thread_failure});
        payload.gate->started = true;
        if (!payload.gate->condition.notify_all())
            return nk::err(nk::job_error{nk::job_error_code::thread_failure});
        while (!payload.gate->release) {
            if (!payload.gate->condition.wait(*lock))
                return nk::err(nk::job_error{nk::job_error_code::thread_failure});
        }
        return nk::ok();
    }

    struct OrderedPayload {
        int value = 0;
        int* order = nullptr;
        nk::u32* count = nullptr;
    };

    nk::result<void, nk::job_error> record_order(
        OrderedPayload& payload) noexcept {
        payload.order[(*payload.count)++] = payload.value;
        return nk::ok();
    }

    struct ProducerJobPayload {
        nk::u32* completed = nullptr;
    };

    nk::result<void, nk::job_error> producer_work(
        ProducerJobPayload&) noexcept {
        return nk::ok();
    }

    void producer_completion(
        ProducerJobPayload& payload,
        const nk::result<void, nk::job_error>& outcome) noexcept {
        if (outcome)
            ++*payload.completed;
    }

    struct ProducerContext {
        nk::JobSystem* jobs = nullptr;
        nk::u32* completed = nullptr;
        nk::u32 submission_count = 0;
        bool failed = false;
    };

    nk::u32 submit_from_thread(void* raw_context) noexcept {
        auto& context = *static_cast<ProducerContext*>(raw_context);
        for (nk::u32 index = 0; index < context.submission_count; ++index) {
            auto submitted = context.jobs->submit(
                nk::JobPriority::normal,
                ProducerJobPayload{context.completed},
                producer_work,
                producer_completion);
            if (!submitted) {
                context.failed = true;
                return 1;
            }
        }
        return 0;
    }
}

TEST(JobSystem, RunsSynchronouslyWithDeferredMainCompletion) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 0, .max_jobs = 4});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    const nk::u64 main_thread = nk::Thread::current_id();
    nk::u64 completion_thread = 0;
    int published = 0;

    auto submitted = jobs->submit(
        nk::JobPriority::normal,
        ArithmeticPayload{7, 0, 0, &completion_thread, nullptr, &published},
        multiply,
        publish_arithmetic);
    ASSERT_TRUE(submitted);
    EXPECT_EQ(jobs->worker_count(), 0u);
    EXPECT_EQ(jobs->pending_count(), 1u);
    auto state = jobs->status(*submitted);
    ASSERT_TRUE(state);
    EXPECT_EQ(*state, nk::JobStatus::completed);
    EXPECT_EQ(published, 0);

    auto updated = jobs->update();
    ASSERT_TRUE(updated);
    EXPECT_EQ(*updated, 1u);
    EXPECT_EQ(published, 21);
    EXPECT_EQ(completion_thread, main_thread);
    EXPECT_FALSE(jobs->status(*submitted));
    nk::JobSystem::destroy(allocator, jobs);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(JobSystem, RunsWorkOnWorkersAndCompletionOnCallingThread) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 2, .max_jobs = 16});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    const nk::u64 main_thread = nk::Thread::current_id();
    nk::u64 completion_thread = 0;
    nk::u64 worker_thread = 0;
    int published = 0;

    ASSERT_TRUE(jobs->submit(
        nk::JobPriority::high,
        ArithmeticPayload{
            9,
            0,
            0,
            &completion_thread,
            &worker_thread,
            &published},
        multiply,
        publish_arithmetic));
    ASSERT_TRUE(jobs->drain());
    EXPECT_EQ(published, 27);
    EXPECT_EQ(completion_thread, main_thread);
    EXPECT_NE(worker_thread, main_thread);
    nk::JobSystem::destroy(allocator, jobs);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(JobSystem, RejectsOverflowWithoutMutatingActiveJobs) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 0, .max_jobs = 2});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    nk::u64 completion_thread = 0;
    int published = 0;

    for (int input : {1, 2}) {
        ASSERT_TRUE(jobs->submit(
            nk::JobPriority::normal,
            ArithmeticPayload{
                input,
                0,
                0,
                &completion_thread,
                nullptr,
                &published},
            multiply,
            publish_arithmetic));
    }
    auto rejected = jobs->submit(
        nk::JobPriority::normal,
        ArithmeticPayload{3, 0, 0, &completion_thread, nullptr, &published},
        multiply,
        publish_arithmetic);
    ASSERT_FALSE(rejected);
    EXPECT_EQ(rejected.error().code, nk::job_error_code::queue_full);
    EXPECT_EQ(jobs->pending_count(), 2u);
    ASSERT_TRUE(jobs->drain());
    nk::JobSystem::destroy(allocator, jobs);
}

TEST(JobSystem, CancelsQueuedSynchronousWorkBeforePublishing) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 0, .max_jobs = 2});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    int executions = 0;
    bool cancelled = false;

    // Zero-worker submissions have already run when submit returns; cancellation
    // must reject a completed handle instead of forging a cancelled outcome.
    auto submitted = jobs->submit(
        nk::JobPriority::low,
        CancelPayload{&executions, &cancelled},
        count_execution,
        observe_cancel);
    ASSERT_TRUE(submitted);
    EXPECT_FALSE(jobs->cancel(*submitted));
    EXPECT_EQ(executions, 1);
    ASSERT_TRUE(jobs->drain());
    EXPECT_FALSE(cancelled);
    nk::JobSystem::destroy(allocator, jobs);
}

TEST(JobSystem, CancelsQueuedWorkWithoutExecutingItsPayload) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 1, .max_jobs = 8});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    Gate gate;
    ASSERT_TRUE(gate.mutex.init());
    ASSERT_TRUE(gate.condition.init());

    ASSERT_TRUE(jobs->submit(
        nk::JobPriority::high,
        GatePayload{&gate},
        wait_at_gate));
    {
        auto lock = nk::LockGuard::acquire(gate.mutex);
        ASSERT_TRUE(lock);
        while (!gate.started)
            ASSERT_TRUE(gate.condition.wait(*lock));
    }

    int executions = 0;
    bool cancelled = false;
    auto target = jobs->submit(
        nk::JobPriority::normal,
        CancelPayload{&executions, &cancelled},
        count_execution,
        observe_cancel);
    ASSERT_TRUE(target);
    ASSERT_TRUE(jobs->cancel(*target));
    {
        auto lock = nk::LockGuard::acquire(gate.mutex);
        ASSERT_TRUE(lock);
        gate.release = true;
        ASSERT_TRUE(gate.condition.notify_all());
    }

    ASSERT_TRUE(jobs->drain());
    EXPECT_EQ(executions, 0);
    EXPECT_TRUE(cancelled);
    nk::JobSystem::destroy(allocator, jobs);
    gate.condition.shutdown();
    gate.mutex.shutdown();
}

TEST(JobSystem, ServicesPrioritiesInOrderOnceAWorkerBecomesAvailable) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 1, .max_jobs = 8});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    Gate gate;
    ASSERT_TRUE(gate.mutex.init());
    ASSERT_TRUE(gate.condition.init());
    ASSERT_TRUE(jobs->submit(
        nk::JobPriority::high,
        GatePayload{&gate},
        wait_at_gate));
    {
        auto lock = nk::LockGuard::acquire(gate.mutex);
        ASSERT_TRUE(lock);
        while (!gate.started)
            ASSERT_TRUE(gate.condition.wait(*lock));
    }

    int order[3]{};
    nk::u32 count = 0;
    ASSERT_TRUE(jobs->submit(
        nk::JobPriority::low,
        OrderedPayload{1, order, &count},
        record_order));
    ASSERT_TRUE(jobs->submit(
        nk::JobPriority::normal,
        OrderedPayload{2, order, &count},
        record_order));
    ASSERT_TRUE(jobs->submit(
        nk::JobPriority::high,
        OrderedPayload{3, order, &count},
        record_order));
    {
        auto lock = nk::LockGuard::acquire(gate.mutex);
        ASSERT_TRUE(lock);
        gate.release = true;
        ASSERT_TRUE(gate.condition.notify_all());
    }

    ASSERT_TRUE(jobs->drain());
    EXPECT_EQ(count, 3u);
    EXPECT_EQ(order[0], 3);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 1);
    nk::JobSystem::destroy(allocator, jobs);
    gate.condition.shutdown();
    gate.mutex.shutdown();
}

TEST(JobSystem, AcceptsMultipleConcurrentProducers) {
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 4, .max_jobs = 320});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    nk::u32 completed = 0;
    ProducerContext contexts[4]{};
    nk::Thread producers[4];
    for (nk::u32 index = 0; index < 4; ++index) {
        contexts[index] = {jobs, &completed, 64, false};
        ASSERT_TRUE(producers[index].start(
            submit_from_thread,
            &contexts[index]));
    }
    for (nk::u32 index = 0; index < 4; ++index) {
        auto joined = producers[index].join();
        ASSERT_TRUE(joined);
        EXPECT_EQ(*joined, 0u);
        EXPECT_FALSE(contexts[index].failed);
    }

    ASSERT_TRUE(jobs->drain());
    EXPECT_EQ(completed, 256u);
    nk::JobSystem::destroy(allocator, jobs);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(JobSystem, DestroysTypedPayloadsIndependentlyOfInputAndOutputSizes) {
    LifetimePayload::destroyed = 0;
    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 1, .max_jobs = 4});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    ASSERT_TRUE(jobs->submit(
        nk::JobPriority::normal,
        LifetimePayload{},
        fill_output));
    ASSERT_TRUE(jobs->drain());
    EXPECT_GE(LifetimePayload::destroyed, 2);
    nk::JobSystem::destroy(allocator, jobs);
    EXPECT_EQ(allocator.get_active_allocation_count(), 0u);
}

TEST(JobSystem, RejectsOversizedPayloadsAtTheApiBoundary) {
    struct OversizedPayload {
        nk::u8 bytes[nk::JobSystem::inline_payload_bytes + 1]{};
    };
    auto work = +[](OversizedPayload&) noexcept
        -> nk::result<void, nk::job_error> { return nk::ok(); };

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    auto created = nk::JobSystem::create(
        allocator,
        {.worker_count = 0, .max_jobs = 1});
    ASSERT_TRUE(created);
    nk::JobSystem* jobs = *created;
    auto submitted = jobs->submit(
        nk::JobPriority::normal,
        OversizedPayload{},
        work);
    ASSERT_FALSE(submitted);
    EXPECT_EQ(
        submitted.error().code,
        nk::job_error_code::payload_too_large);
    nk::JobSystem::destroy(allocator, jobs);
}
