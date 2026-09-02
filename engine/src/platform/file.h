#pragma once

#include "collections/dyarr.h"
#include "collections/slice.h"
#include "core/result.h"
#include "core/str.h"

namespace nk {
    namespace FileMode {
        using Value = u8;

        enum : Value {
            None,
            Read,
            Write,
        };
    }

    enum class file_error : u8 {
        none,
        invalid_path,
        invalid_mode,
        already_open,
        not_open,
        operation_not_permitted,
        not_found,
        open_failed,
        seek_failed,
        read_failed,
        write_failed,
        close_failed,
        out_of_memory,
    };

    enum class read_line_outcome : u8 {
        line,
        end_of_file,
    };

    class File {
    public:
        explicit File(mem::Allocator& allocator)
            : m_file{nullptr},
              m_open{false},
              m_binary{false},
              m_mode{FileMode::None},
              m_allocator{&allocator},
              m_path{allocator} {}

        ~File();

        File(const File&) = delete;
        File& operator=(const File&) = delete;
        File(File&&) = delete;
        File& operator=(File&&) = delete;

        static bool exists(cstr path);

        [[nodiscard]] result<void, file_error> open(
            strview path,
            FileMode::Value mode,
            bool binary);
        [[nodiscard]] result<void, file_error> close() noexcept;

        [[nodiscard]] result<read_line_outcome, file_error> read_line(
            str& out_line);
        [[nodiscard]] result<void, file_error> write_line(strview line);

        [[nodiscard]] result<u64, file_error> read(cl::slice<u8> output);
        [[nodiscard]] result<cl::dyarr<u8>, file_error> read_all_bytes();

        [[nodiscard]] result<u64, file_error> write(
            cl::slice<const u8> input);

        bool is_open() const noexcept { return m_open; }

    private:
        FILE* m_file;
        bool m_open;
        bool m_binary;
        FileMode::Value m_mode;
        mem::Allocator* m_allocator;
        str m_path;
    };
}
