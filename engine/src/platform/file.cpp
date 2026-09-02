#include "nkpch.h"

#include <cerrno>

#include "platform/file.h"
#include "memory/allocator.h"

namespace nk {
    File::~File() {
        if (m_open)
            (void)close();
    }

    bool File::exists(cstr path) {
        if (path == nullptr)
            return false;
        struct stat buffer;
        return stat(path, &buffer) == 0;
    }

    result<void, file_error> File::open(
        const strview path,
        const FileMode::Value mode,
        const bool binary) {
        if (m_open)
            return err(file_error::already_open);
        if (path.empty())
            return err(file_error::invalid_path);

        cstr mode_str;
        if ((mode & FileMode::Read) != 0 && (mode & FileMode::Write) != 0) {
            mode_str = binary ? "rb+" : "w+";
        } else if ((mode & FileMode::Read) != 0 && (mode & FileMode::Write) == 0) {
            mode_str = binary ? "rb" : "r";
        } else if ((mode & FileMode::Read) == 0 && (mode & FileMode::Write) != 0) {
            mode_str = binary ? "wb+" : "w";
        } else {
            return err(file_error::invalid_mode);
        }

        if (!m_path.assign(path))
            return err(file_error::out_of_memory);

        errno = 0;
        FILE* file = fopen(m_path.cstr(), mode_str);
        if (file == nullptr) {
            const file_error error = errno == ENOENT
                ? file_error::not_found
                : file_error::open_failed;
            m_path.clear();
            return err(error);
        }

        m_file = file;
        m_open = true;
        m_binary = binary;
        m_mode = mode;
        return ok();
    }

    result<void, file_error> File::close() noexcept {
        if (!m_open)
            return ok();

        const i32 close_result = fclose(m_file);
        m_file = nullptr;
        m_open = false;
        m_binary = false;
        m_mode = FileMode::None;
        m_path.clear();
        if (close_result != 0)
            return err(file_error::close_failed);
        return ok();
    }

    result<read_line_outcome, file_error> File::read_line(str& out_line) {
        if (!m_open)
            return err(file_error::not_open);
        if ((m_mode & FileMode::Read) == 0)
            return err(file_error::operation_not_permitted);

        constexpr u64 buffer_size = 32000;
        char buffer[buffer_size];
        if (fgets(buffer, buffer_size, m_file) == nullptr) {
            if (feof(m_file) != 0)
                return ok(read_line_outcome::end_of_file);
            return err(file_error::read_failed);
        }

        if (!out_line.assign(buffer))
            return err(file_error::out_of_memory);
        return ok(read_line_outcome::line);
    }

    result<void, file_error> File::write_line(const strview line) {
        if (!m_open)
            return err(file_error::not_open);
        if ((m_mode & FileMode::Write) == 0)
            return err(file_error::operation_not_permitted);

        const bool wrote_line =
            fwrite(line.data(), 1, line.length(), m_file) == line.length();
        const i32 result = wrote_line ? fputc('\n', m_file) : EOF;

        if (result == EOF || fflush(m_file) != 0)
            return err(file_error::write_failed);
        return ok();
    }

    result<u64, file_error> File::read(const cl::slice<u8> output) {
        if (!m_open)
            return err(file_error::not_open);
        if ((m_mode & FileMode::Read) == 0)
            return err(file_error::operation_not_permitted);
        if (output.empty())
            return ok(u64{0});

        const u64 bytes_read = fread(
            output.data(),
            1,
            output.length(),
            m_file);
        if (bytes_read != output.length() && ferror(m_file) != 0)
            return err(file_error::read_failed);

        return ok(bytes_read);
    }

    result<cl::dyarr<u8>, file_error> File::read_all_bytes() {
        if (!m_open)
            return err(file_error::not_open);
        if ((m_mode & FileMode::Read) == 0)
            return err(file_error::operation_not_permitted);

        if (fseek(m_file, 0, SEEK_END) != 0)
            return err(file_error::seek_failed);
        const long file_size = ftell(m_file);
        if (file_size < 0 || fseek(m_file, 0, SEEK_SET) != 0)
            return err(file_error::seek_failed);

        const u64 size = static_cast<u64>(file_size);
        cl::dyarr<u8> data;
        if (size == 0) {
            if (!data.dyarr_init(m_allocator, 0))
                return err(file_error::out_of_memory);
            return ok(std::move(data));
        }
        if (!data.dyarr_init_len(m_allocator, size, size))
            return err(file_error::out_of_memory);

        auto bytes_read = read(cl::slice<u8>{data});
        if (!bytes_read)
            return err(bytes_read.error());
        if (*bytes_read != size)
            return err(file_error::read_failed);

        return ok(std::move(data));
    }

    result<u64, file_error> File::write(const cl::slice<const u8> input) {
        if (!m_open)
            return err(file_error::not_open);
        if ((m_mode & FileMode::Write) == 0)
            return err(file_error::operation_not_permitted);
        if (input.empty())
            return ok(u64{0});

        const u64 bytes_written = fwrite(
            input.data(),
            1,
            input.length(),
            m_file);
        if (bytes_written != input.length() || fflush(m_file) != 0)
            return err(file_error::write_failed);

        return ok(bytes_written);
    }
}
