#include "nkpch.h"
#include "systems/logging_system.h"

#include "platform/file.h"
#include "memory/allocator.h"

namespace nk {
    File::~File() {
        if (m_open) close();
    };

    bool File::exists(cstr path) {
        if (path == nullptr)
            return false;
        struct stat buffer;
        return stat(path, &buffer) == 0;
    }

    bool File::open(const strview path, const FileMode::Value mode, const bool binary) {
        if (m_open || path.empty())
            return false;

        cstr mode_str;
        if ((mode & FileMode::Read) != 0 && (mode & FileMode::Write) != 0) {
            mode_str = binary ? "rb+" : "w+";
        } else if ((mode & FileMode::Read) != 0 && (mode & FileMode::Write) == 0) {
            mode_str = binary ? "rb" : "r";
        } else if ((mode & FileMode::Read) == 0 && (mode & FileMode::Write) != 0) {
            mode_str = binary ? "wb+" : "w";
        } else {
            ErrorLog("Invalid mode passed while trying to open file: {}", path);
            return false;
        }

        if (!m_path.assign(path)) {
            ErrorLog("Unable to store file path: {}", path);
            return false;
        }

        FILE* file = fopen(m_path.cstr(), mode_str);
        if (file == nullptr) {
            ErrorLog("Failed to open file: {}", path);
            m_path.clear();
            return false;
        }

        m_file = file;
        m_open = true;
        m_binary = binary;
        m_mode = mode;
        return true;
    }

    void File::close() {
        if (!m_open) return;
        fclose(m_file);
        m_file = nullptr;
        m_open = false;
        m_binary = false;
        m_mode = FileMode::None;
        m_path.clear();
    }

    bool File::read_line(str* out_line) {
        if (!m_open || out_line == nullptr)
            return false;
        
        constexpr u64 buffer_size = 32000;
        char buffer[buffer_size];
        if (fgets(buffer, buffer_size, m_file) == nullptr)
            return false;

        return out_line->assign(buffer);
    }

    bool File::write_line(const strview line) {
        if (!m_open) return false;
        const bool wrote_line =
            fwrite(line.data(), 1, line.length(), m_file) == line.length();
        const i32 result = wrote_line ? fputc('\n', m_file) : EOF;

        // Make sure to flush the stream so it is written to the file immediately
        // This prevents data loss in the event of a crash
        fflush(m_file);
        return result != EOF;
    }

    bool File::read(u64 data_size, void* out_data, u64* out_bytes_read) {
        if (!m_open || data_size == 0 || out_data == nullptr || out_bytes_read == nullptr)
            return false;

        *out_bytes_read = fread(out_data, 1, data_size, m_file);
        if (*out_bytes_read != data_size)
            return false;

        return true;
    }

    bool File::read_all_bytes(u8** out_data, u64* out_bytes_read) {
        if (!m_open || out_data == nullptr || out_bytes_read == nullptr ||
            m_allocator == nullptr) {
            return false;
        }

        *out_data = nullptr;
        *out_bytes_read = 0;

        if (fseek(m_file, 0, SEEK_END) != 0)
            return false;
        const long file_size = ftell(m_file);
        rewind(m_file);

        if (file_size <= 0)
            return false;
        const u64 size = static_cast<u64>(file_size);

        *out_data = m_allocator->allocate_lot_t(u8, size);
        if (*out_data == nullptr)
            return false;
        *out_bytes_read = fread(*out_data, 1, size, m_file);
        if (*out_bytes_read != size) {
            m_allocator->free_lot_t(u8, *out_data, size);
            *out_data = nullptr;
            *out_bytes_read = 0;
            return false;
        }

        return true;
    }

    bool File::write(u64 data_size, const void* data, u64* out_bytes_written) {
        if (!m_open || data_size == 0 || data == nullptr || out_bytes_written == nullptr)
            return false;

        *out_bytes_written = fwrite(data, 1, data_size, m_file);
        if (*out_bytes_written != data_size)
            return false;

        fflush(m_file);
        return true;
    }
}
