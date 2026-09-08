#pragma once

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "core/defines.h"
#include "core/result.h"
#include "memory/allocator.h"

namespace nk {
    enum class JobPriority : u8 {
        low,
        normal,
        high,
    };

    enum class JobStatus : u8 {
        queued,
        running,
        completed,
        cancel_requested,
    };

    enum class job_error_code : u8 {
        invalid_config,
        invalid_state,
        invalid_handle,
        out_of_memory,
        queue_full,
        payload_too_large,
        payload_alignment_unsupported,
        thread_failure,
        cancelled,
        execution_failed,
    };

    struct job_error {
        job_error_code code = job_error_code::execution_failed;
        i32 native_code = 0;
    };

    struct JobHandle {
        u32 index = numeric::invalid_id;
        u32 generation = 0;

        [[nodiscard]] bool valid() const noexcept {
            return index != numeric::invalid_id && generation != 0;
        }

        bool operator==(const JobHandle&) const noexcept = default;
    };

    struct JobSystemConfig {
        u32 worker_count = 0;
        u32 max_jobs = 256;
    };

    template <typename Payload>
    using JobWork = result<void, job_error> (*)(Payload&) noexcept;

    template <typename Payload>
    using JobCompletion = void (*)(
        Payload&,
        const result<void, job_error>&) noexcept;

    class JobSystem final {
    public:
        // Large enough for owned resource requests (bounded paths plus decoded
        // result owner) while retaining one allocation for the entire pool.
        static constexpr u64 inline_payload_bytes = 2048;
        static constexpr u64 inline_payload_alignment = alignof(std::max_align_t);

        JobSystem(const JobSystem&) = delete;
        JobSystem& operator=(const JobSystem&) = delete;
        JobSystem(JobSystem&&) = delete;
        JobSystem& operator=(JobSystem&&) = delete;

        ~JobSystem();

        JobSystem() noexcept = default;

        [[nodiscard]] static result<JobSystem*, job_error> create(
            mem::Allocator& allocator,
            const JobSystemConfig& config) noexcept;
        static void destroy(
            mem::Allocator& allocator,
            JobSystem* system) noexcept;

        template <typename Payload>
        [[nodiscard]] result<JobHandle, job_error> submit(
            const JobPriority priority,
            Payload&& payload,
            JobWork<std::remove_cvref_t<Payload>> work,
            JobCompletion<std::remove_cvref_t<Payload>> completion = nullptr) {
            using Value = std::remove_cvref_t<Payload>;
            struct TaskModel {
                Value payload;
                JobWork<Value> work;
                JobCompletion<Value> completion;
            };

            if constexpr (sizeof(TaskModel) > inline_payload_bytes) {
                return err(job_error{job_error_code::payload_too_large});
            } else if constexpr (alignof(TaskModel) > inline_payload_alignment) {
                return err(job_error{
                    job_error_code::payload_alignment_unsupported});
            } else if constexpr (
                !std::is_constructible_v<Value, Payload&&> ||
                !std::is_move_constructible_v<Value> ||
                !std::is_destructible_v<Value>) {
                return err(job_error{job_error_code::invalid_config});
            } else {
                if (work == nullptr)
                    return err(job_error{job_error_code::invalid_config});

                TaskModel source{
                    std::forward<Payload>(payload),
                    work,
                    completion,
                };
                ErasedSubmission submission{
                    .source = &source,
                    .construct = +[](void* destination, void* raw_source) noexcept {
                        auto* typed_source = static_cast<TaskModel*>(raw_source);
                        std::construct_at(
                            static_cast<TaskModel*>(destination),
                            std::move(*typed_source));
                    },
                    .execute = +[](void* storage) noexcept {
                        auto& task = *static_cast<TaskModel*>(storage);
                        return task.work(task.payload);
                    },
                    .complete = +[](
                        void* storage,
                        const result<void, job_error>& outcome) noexcept {
                        auto& task = *static_cast<TaskModel*>(storage);
                        if (task.completion != nullptr)
                            task.completion(task.payload, outcome);
                    },
                    .destroy = +[](void* storage) noexcept {
                        std::destroy_at(static_cast<TaskModel*>(storage));
                    },
                };
                return submit_erased(priority, submission);
            }
        }

        [[nodiscard]] result<u32, job_error> update(
            u32 max_completions = numeric::u32_max) noexcept;
        [[nodiscard]] result<void, job_error> drain() noexcept;
        [[nodiscard]] result<void, job_error> cancel(JobHandle handle) noexcept;
        [[nodiscard]] result<JobStatus, job_error> status(
            JobHandle handle) noexcept;
        [[nodiscard]] u32 worker_count() const noexcept;
        [[nodiscard]] u32 pending_count() noexcept;
        [[nodiscard]] bool accepting() const noexcept;
        void shutdown() noexcept;

    private:
        using ErasedExecute = result<void, job_error> (*)(void*) noexcept;
        using ErasedComplete = void (*)(
            void*,
            const result<void, job_error>&) noexcept;
        using ErasedDestroy = void (*)(void*) noexcept;

        struct ErasedSubmission {
            void* source;
            void (*construct)(void*, void*) noexcept;
            ErasedExecute execute;
            ErasedComplete complete;
            ErasedDestroy destroy;
        };

        [[nodiscard]] result<void, job_error> init(
            mem::Allocator& allocator,
            const JobSystemConfig& config) noexcept;
        [[nodiscard]] result<JobHandle, job_error> submit_erased(
            JobPriority priority,
            const ErasedSubmission& submission) noexcept;
        static u32 worker_entry(void* context) noexcept;
        [[nodiscard]] u32 worker_loop() noexcept;
        void execute_job(u32 index) noexcept;

        mem::Allocator* m_allocator = nullptr;
        void* m_data = nullptr;
    };
}
