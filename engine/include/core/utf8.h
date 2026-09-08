#pragma once

#include "core/result.h"
#include "core/strview.h"

namespace nk::utf8 {
    enum class error : u8 { end_of_text, invalid_sequence };
    struct Codepoint { u32 value; u8 bytes; };

    [[nodiscard]] inline result<Codepoint, error> decode(
        strview text, u64 offset) noexcept {
        if (offset >= text.length())
            return err(error::end_of_text);
        const auto* bytes = reinterpret_cast<const u8*>(text.data()) + offset;
        const u8 first = bytes[0];
        if (first < 0x80)
            return ok(Codepoint{first, 1});
        u8 length;
        u32 value;
        u32 minimum;
        if (first >= 0xc2 && first <= 0xdf) {
            length = 2; value = first & 0x1f; minimum = 0x80;
        } else if (first >= 0xe0 && first <= 0xef) {
            length = 3; value = first & 0x0f; minimum = 0x800;
        } else if (first >= 0xf0 && first <= 0xf4) {
            length = 4; value = first & 7; minimum = 0x10000;
        } else {
            return err(error::invalid_sequence);
        }
        if (length > text.length() - offset)
            return err(error::invalid_sequence);
        for (u8 i = 1; i < length; ++i) {
            if ((bytes[i] & 0xc0) != 0x80)
                return err(error::invalid_sequence);
            value = (value << 6) | (bytes[i] & 0x3f);
        }
        if (value < minimum || value > 0x10ffff ||
            (value >= 0xd800 && value <= 0xdfff))
            return err(error::invalid_sequence);
        return ok(Codepoint{value, length});
    }

    [[nodiscard]] inline bool valid(strview text) noexcept {
        for (u64 offset = 0; offset < text.length();) {
            auto codepoint = decode(text, offset);
            if (!codepoint)
                return false;
            offset += codepoint->bytes;
        }
        return true;
    }

    // Writes a scalar value, not a keycode; output does not require a NUL byte.
    [[nodiscard]] inline u8 encode(u32 value, char (&output)[4]) noexcept {
        if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            return 0;
        if (value < 0x80) { output[0] = static_cast<char>(value); return 1; }
        const u8 count = value < 0x800 ? 2 : value < 0x10000 ? 3 : 4;
        for (u8 i = count - 1; i > 0; --i) {
            output[i] = static_cast<char>(0x80 | (value & 0x3f));
            value >>= 6;
        }
        output[0] = static_cast<char>((count == 2 ? 0xc0 : count == 3 ? 0xe0 : 0xf0) | value);
        return count;
    }
}
