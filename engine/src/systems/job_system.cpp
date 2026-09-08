#include "nkpch.h"

#include "systems/job_system.h"

#include "collections/arr.h"
#include "collections/ring.h"
#include "core/thread.h"

namespace nk {
    namespace {
        enum class SlotState : u8 {
            free,
            queued,
            running,
            completed,
        };

        struct JobRecord {
            alignas(std::max_align_t)
                u8 payload[JobSystem::inline_payload_bytes]{};
            result<void, job_error> (*execute)(void*) noexcept = nullptr;
            void (*complete)(
                void*,
                const result<void, job_error>&) noexcept = nullptr;
            void (*destroy)(void*) noexcept = nullptr;
            job_error error{};
            u32 generation = 0;
            SlotState state = SlotState::free;
            bool succeeded = false;
            bool cancel_requested = false;
        };

        struct WorkerContext {
            JobSystem* system = nullptr;
        };

        struct JobSystemData {
            Mutex mutex;
            ConditionVariable condition;
            cl::arr<JobRecord> records;
            cl::arr<Thread> workers;
            cl::arr<WorkerContext> worker_contexts;
            cl::ring<u32> high;
            cl::ring<u32> normal;
            cl::ring<u32> low;
            cl::ring<u32> completions;
            u32 worker_count = 0;
            u32 started_workers = 0;
            u32 active_count = 0;
            u32 high_streak = 0;
            u32 normal_streak = 0;
            bool accepting = false;
            bool stop_requested = false;
            bool initialized = false;
        };

        JobSystemData& data(void* storage) noexcept {
            return *static_cast<JobSystemData*>(storage);
        }

        bool queues_empty(const JobSystemData& state) noexcept {
            return state.high.empty() && state.normal.empty() &&
                   state.low.empty();
        }

        result<u32, job_error> dequeue(JobSystemData& state) noexcept {
            // Bound priority dominance so normal and low work cannot starve.
            cl::ring<u32>* selected = nullptr;
            if (!state.high.empty() &&
                (state.high_streak < 8 ||
                 (state.normal.empty() && state.low.empty()))) {
                selected = &state.high;
                ++state.high_streak;
            } else if (!state.normal.empty() &&
                       (state.normal_streak < 4 || state.low.empty())) {
                selected = &state.normal;
                state.high_streak = 0;
                ++state.normal_streak;
            } else if (!state.low.empty()) {
                selected = &state.low;
                state.high_streak = 0;
                state.normal_streak = 0;
            } else if (!state.high.empty()) {
                selected = &state.high;
                ++state.high_streak;
            } else if (!state.normal.empty()) {
                selected = &state.normal;
                ++state.normal_streak;
            }

            if (selected == nullptr)
                return err(job_error{job_error_code::invalid_state});
            auto index = selected->pop();
            if (!index)
                return err(job_error{job_error_code::invalid_state});
            return ok(*index);
        }

        cl::ring<u32>& queue_for(
            JobSystemData& state,
            const JobPriority priority) noexcept {
            switch (priority) {
                case JobPriority::high: return state.high;
                case JobPriority::normal: return state.normal;
                case JobPriority::low: return state.low;
            }
            return state.normal;
        }
    }

    JobSystem::~JobSystem() {
        shutdown();
    }

    result<JobSystem*, job_error> JobSystem::create(
        mem::Allocator& allocator,
        const JobSystemConfig& config) noexcept {
        JobSystem* system = allocator.construct_t(JobSystem);
        if (system == nullptr)
            return err(job_error{job_error_code::out_of_memory});

        auto initialized = system->init(allocator, config);
        if (!initialized) {
            const job_error error = initialized.error();
            allocator.deconstruct_t(JobSystem, system);
            return err(error);
        }
        return ok(system);
    }

    void JobSystem::destroy(
        mem::Allocator& allocator,
        JobSystem* system) noexcept {
        if (system != nullptr)
            (void)allocator.deconstruct_t(JobSystem, system);
    }

    result<void, job_error> JobSystem::init(
        mem::Allocator& allocator,
        const JobSystemConfig& config) noexcept {
        if (m_data != nullptr || config.max_jobs == 0)
            return err(job_error{job_error_code::invalid_config});

        JobSystemData* state = allocator.construct_t(JobSystemData);
        if (state == nullptr)
            return err(job_error{job_error_code::out_of_memory});
        m_allocator = &allocator;
        m_data = state;

        if (!state->mutex.init() || !state->condition.init()) {
            shutdown();
            return err(job_error{job_error_code::thread_failure});
        }

        if (!state->records.arr_init(&allocator, config.max_jobs) ||
            !state->high.ring_init(&allocator, config.max_jobs) ||
            !state->normal.ring_init(&allocator, config.max_jobs) ||
            !state->low.ring_init(&allocator, config.max_jobs) ||
            !state->completions.ring_init(&allocator, config.max_jobs)) {
            shutdown();
            return err(job_error{job_error_code::out_of_memory});
        }

        if (config.worker_count > 0) {
            if (!state->workers.arr_init(&allocator, config.worker_count) ||
                !state->worker_contexts.arr_init(
                    &allocator,
                    config.worker_count)) {
                shutdown();
                return err(job_error{job_error_code::out_of_memory});
            }
        }

        state->worker_count = config.worker_count;
        state->accepting = true;
        state->initialized = true;
        for (u32 index = 0; index < config.worker_count; ++index) {
            state->worker_contexts[index].system = this;
            auto started = state->workers[index].start(
                &JobSystem::worker_entry,
                &state->worker_contexts[index]);
            if (!started) {
                state->accepting = false;
                state->stop_requested = true;
                (void)state->condition.notify_all();
                for (u32 joined = 0;
                     joined < state->started_workers;
                     ++joined) {
                    (void)state->workers[joined].join();
                }
                state->started_workers = 0;
                const i32 native_code = started.error().native_code;
                shutdown();
                return err(job_error{
                    job_error_code::thread_failure,
                    native_code,
                });
            }
            ++state->started_workers;
        }
        return ok();
    }

    result<JobHandle, job_error> JobSystem::submit_erased(
        const JobPriority priority,
        const ErasedSubmission& submission) noexcept {
        if (m_data == nullptr)
            return err(job_error{job_error_code::invalid_state});
        JobSystemData& state = data(m_data);
        u32 selected_index = numeric::invalid_id;
        JobHandle handle{};
        {
            auto lock = LockGuard::acquire(state.mutex);
            if (!lock)
                return err(job_error{
                    job_error_code::thread_failure,
                    lock.error().native_code,
                });
            if (!state.accepting)
                return err(job_error{job_error_code::invalid_state});
            if (state.active_count == state.records.length())
                return err(job_error{job_error_code::queue_full});

            for (u32 index = 0; index < state.records.length(); ++index) {
                if (state.records[index].state == SlotState::free) {
                    selected_index = index;
                    break;
                }
            }
            if (selected_index == numeric::invalid_id)
                return err(job_error{job_error_code::queue_full});

            JobRecord& record = state.records[selected_index];
            submission.construct(record.payload, submission.source);
            record.execute = submission.execute;
            record.complete = submission.complete;
            record.destroy = submission.destroy;
            record.error = {};
            record.succeeded = false;
            record.cancel_requested = false;
            record.state = SlotState::queued;
            ++record.generation;
            if (record.generation == 0)
                ++record.generation;

            auto queued = queue_for(state, priority).push_copy(selected_index);
            if (!queued) {
                record.destroy(record.payload);
                record.execute = nullptr;
                record.complete = nullptr;
                record.destroy = nullptr;
                record.state = SlotState::free;
                return err(job_error{job_error_code::queue_full});
            }

            ++state.active_count;
            handle = {selected_index, record.generation};
            (void)state.condition.notify_one();
        }

        if (state.worker_count == 0)
            execute_job(selected_index);
        return ok(handle);
    }

    u32 JobSystem::worker_entry(void* context) noexcept {
        auto& worker = *static_cast<WorkerContext*>(context);
        return worker.system->worker_loop();
    }

    u32 JobSystem::worker_loop() noexcept {
        JobSystemData& state = data(m_data);
        for (;;) {
            u32 index = numeric::invalid_id;
            bool cancel_without_execution = false;
            {
                auto lock = LockGuard::acquire(state.mutex);
                if (!lock)
                    return 1;
                while (queues_empty(state) && !state.stop_requested) {
                    if (!state.condition.wait(*lock))
                        return 2;
                }
                if (queues_empty(state) && state.stop_requested)
                    return 0;

                auto dequeued = dequeue(state);
                if (!dequeued)
                    return 3;
                index = *dequeued;
                JobRecord& record = state.records[index];
                cancel_without_execution = record.cancel_requested ||
                                           state.stop_requested;
                record.state = SlotState::running;
            }

            if (cancel_without_execution) {
                auto lock = LockGuard::acquire(state.mutex);
                if (!lock)
                    return 4;
                JobRecord& record = state.records[index];
                record.succeeded = false;
                record.error = {job_error_code::cancelled};
                record.state = SlotState::completed;
                if (!state.completions.push_copy(index))
                    return 5;
                (void)state.condition.notify_all();
                continue;
            }

            execute_job(index);
        }
    }

    void JobSystem::execute_job(const u32 index) noexcept {
        JobSystemData& state = data(m_data);
        JobRecord& record = state.records[index];
        auto outcome = record.execute(record.payload);

        auto lock = LockGuard::acquire(state.mutex);
        if (!lock)
            return;
        if (record.cancel_requested || state.stop_requested) {
            record.succeeded = false;
            record.error = {job_error_code::cancelled};
        } else if (outcome) {
            record.succeeded = true;
            record.error = {};
        } else {
            record.succeeded = false;
            record.error = outcome.error();
        }
        record.state = SlotState::completed;
        (void)state.completions.push_copy(index);
        (void)state.condition.notify_all();
    }

    result<u32, job_error> JobSystem::update(
        const u32 max_completions) noexcept {
        if (m_data == nullptr)
            return err(job_error{job_error_code::invalid_state});
        JobSystemData& state = data(m_data);
        u32 completed_count = 0;
        while (completed_count < max_completions) {
            u32 index = numeric::invalid_id;
            JobRecord* record = nullptr;
            {
                auto lock = LockGuard::acquire(state.mutex);
                if (!lock) {
                    return err(job_error{
                        job_error_code::thread_failure,
                        lock.error().native_code,
                    });
                }
                auto completion = state.completions.pop();
                if (!completion)
                    return ok(completed_count);
                index = *completion;
                record = &state.records[index];
            }

            const result<void, job_error> outcome = record->succeeded
                ? result<void, job_error>{ok()}
                : result<void, job_error>{err(record->error)};
            record->complete(record->payload, outcome);

            {
                auto lock = LockGuard::acquire(state.mutex);
                if (!lock) {
                    return err(job_error{
                        job_error_code::thread_failure,
                        lock.error().native_code,
                    });
                }
                record->destroy(record->payload);
                record->execute = nullptr;
                record->complete = nullptr;
                record->destroy = nullptr;
                record->state = SlotState::free;
                record->cancel_requested = false;
                --state.active_count;
                (void)state.condition.notify_all();
            }
            ++completed_count;
        }
        return ok(completed_count);
    }

    result<void, job_error> JobSystem::drain() noexcept {
        if (m_data == nullptr)
            return err(job_error{job_error_code::invalid_state});
        JobSystemData& state = data(m_data);
        for (;;) {
            auto updated = update();
            if (!updated)
                return err(updated.error());

            auto lock = LockGuard::acquire(state.mutex);
            if (!lock) {
                return err(job_error{
                    job_error_code::thread_failure,
                    lock.error().native_code,
                });
            }
            if (state.active_count == 0)
                return ok();
            if (state.completions.empty()) {
                auto waited = state.condition.wait(*lock);
                if (!waited) {
                    return err(job_error{
                        job_error_code::thread_failure,
                        waited.error().native_code,
                    });
                }
            }
        }
    }

    result<void, job_error> JobSystem::cancel(const JobHandle handle) noexcept {
        if (m_data == nullptr || !handle.valid())
            return err(job_error{job_error_code::invalid_handle});
        JobSystemData& state = data(m_data);
        auto lock = LockGuard::acquire(state.mutex);
        if (!lock) {
            return err(job_error{
                job_error_code::thread_failure,
                lock.error().native_code,
            });
        }
        if (handle.index >= state.records.length())
            return err(job_error{job_error_code::invalid_handle});
        JobRecord& record = state.records[handle.index];
        if (record.generation != handle.generation ||
            record.state == SlotState::free ||
            record.state == SlotState::completed) {
            return err(job_error{job_error_code::invalid_handle});
        }
        record.cancel_requested = true;
        (void)state.condition.notify_all();
        return ok();
    }

    result<JobStatus, job_error> JobSystem::status(
        const JobHandle handle) noexcept {
        if (m_data == nullptr || !handle.valid())
            return err(job_error{job_error_code::invalid_handle});
        JobSystemData& state = data(m_data);
        auto lock = LockGuard::acquire(state.mutex);
        if (!lock) {
            return err(job_error{
                job_error_code::thread_failure,
                lock.error().native_code,
            });
        }
        if (handle.index >= state.records.length())
            return err(job_error{job_error_code::invalid_handle});
        const JobRecord& record = state.records[handle.index];
        if (record.generation != handle.generation ||
            record.state == SlotState::free) {
            return err(job_error{job_error_code::invalid_handle});
        }
        if (record.cancel_requested)
            return ok(JobStatus::cancel_requested);
        switch (record.state) {
            case SlotState::queued: return ok(JobStatus::queued);
            case SlotState::running: return ok(JobStatus::running);
            case SlotState::completed: return ok(JobStatus::completed);
            case SlotState::free: break;
        }
        return err(job_error{job_error_code::invalid_handle});
    }

    u32 JobSystem::worker_count() const noexcept {
        return m_data == nullptr ? 0 : data(m_data).worker_count;
    }

    u32 JobSystem::pending_count() noexcept {
        if (m_data == nullptr)
            return 0;
        JobSystemData& state = data(m_data);
        auto lock = LockGuard::acquire(state.mutex);
        return lock ? state.active_count : 0;
    }

    bool JobSystem::accepting() const noexcept {
        if (m_data == nullptr)
            return false;
        JobSystemData& state = data(m_data);
        auto lock = LockGuard::acquire(state.mutex);
        return lock && state.accepting;
    }

    void JobSystem::shutdown() noexcept {
        if (m_data == nullptr)
            return;
        JobSystemData& state = data(m_data);
        if (state.initialized) {
            {
                auto lock = LockGuard::acquire(state.mutex);
                if (lock) {
                    state.accepting = false;
                    state.stop_requested = true;
                    for (JobRecord& record : state.records) {
                        if (record.state == SlotState::queued ||
                            record.state == SlotState::running) {
                            record.cancel_requested = true;
                        }
                    }
                    (void)state.condition.notify_all();
                }
            }

            for (u32 index = 0; index < state.started_workers; ++index)
                (void)state.workers[index].join();
            state.started_workers = 0;
            (void)update();

            // Keep shutdown total even if a backend error prevented publication.
            for (JobRecord& record : state.records) {
                if (record.state == SlotState::free)
                    continue;
                record.destroy(record.payload);
                record.execute = nullptr;
                record.complete = nullptr;
                record.destroy = nullptr;
                record.state = SlotState::free;
            }
            state.active_count = 0;
            state.initialized = false;
        }

        (void)state.completions.ring_shutdown();
        (void)state.low.ring_shutdown();
        (void)state.normal.ring_shutdown();
        (void)state.high.ring_shutdown();
        (void)state.worker_contexts.arr_shutdown();
        (void)state.workers.arr_shutdown();
        (void)state.records.arr_shutdown();
        state.condition.shutdown();
        state.mutex.shutdown();

        mem::Allocator* allocator = m_allocator;
        m_data = nullptr;
        m_allocator = nullptr;
        (void)allocator->deconstruct_t(JobSystemData, &state);
    }
}
