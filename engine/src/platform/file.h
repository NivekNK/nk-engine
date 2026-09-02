#pragma once

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

        static bool exists(cstr path);

        bool open(strview path, FileMode::Value mode, bool binary);
        void close();

        bool read_line(str* out_line);
        bool write_line(strview line);

        bool read(u64 data_size, void* out_data, u64* out_bytes_read);
        bool read_all_bytes(u8** out_data, u64* out_bytes_read);

        bool write(u64 data_size, const void* data, u64* out_bytes_written);

    private:
        FILE* m_file;
        bool m_open;
        bool m_binary;
        FileMode::Value m_mode;
        mem::Allocator* m_allocator;
        str m_path;
    };
}
