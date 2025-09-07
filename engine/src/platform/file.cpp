#include "nkpch.h"
#include "systems/logging_system.h"

#include "platform/file.h"

namespace nk {
    File::~File() {
        if (m_open) close();
    };

    bool File::exists(cstr path) {
        struct stat buffer;
        return stat(path, &buffer) == 0;
    }

    bool File::open(cstr path, FileMode::Value mode, bool binary) {
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

        FILE* file = fopen(path, mode_str);
        if (file == nullptr) {
            ErrorLog("Failed to open file: {}", path);
            return false;
        }

        m_file = file;
        m_open = true;
        m_binary = binary;
        m_mode = mode;
        m_path = path;
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
        if (!m_open) return false;
        
        constexpr u64 buffer_size = 32000;
        char buffer[buffer_size];
        if (fgets(buffer, buffer_size, m_file) == nullptr)
            return false;

        *out_line = buffer;
        return true;
    }

    bool File::write_line(cstr line) {
        if (!m_open) return false;
        i32 result = fputs(line, m_file);
        if (result != EOF) {
            result = fputc('\n', m_file);
        }

        // Make sure to flush the stream so it is written to the file immediately
        // This prevents data loss in the event of a crash
        fflush(m_file);
        return result != EOF;
    }

    bool File::read(u64 data_size, void* out_data, u64* out_bytes_read) {
        if (!m_open)
            return false;

        *out_bytes_read = fread(out_data, 1, data_size, m_file);
        if (*out_bytes_read != data_size)
            return false;

        return true;
    }

    bool File::read_all_bytes(u8** out_data, u64* out_bytes_read) {
        if (!m_open)
            return false;

        fseek(m_file, 0, SEEK_END);
        u64 size = ftell(m_file);
        rewind(m_file);

        *out_data = native_allocate_lot(u8, size);
        *out_bytes_read = fread(*out_data, 1, size, m_file);
        if (*out_bytes_read != size)
            return false;

        return true;
    }

    bool File::write(u64 data_size, const void* data, u64* out_bytes_written) {
        if (!m_open || data == nullptr)
            return false;

        *out_bytes_written = fwrite(data, 1, data_size, m_file);
        if (*out_bytes_written != data_size)
            return false;

        fflush(m_file);
        return true;
    }
}
