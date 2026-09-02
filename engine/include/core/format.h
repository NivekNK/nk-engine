#pragma once

#include <charconv>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <tuple>
#include <type_traits>
#include <utility>

#include "core/os.h"
#include "core/str.h"
#include "core/strbuf.h"

namespace nk {
    enum class FormatStatus : u8 {
        Success,
        Truncated,
        InvalidFormat,
    };

    struct FormatResult {
        FormatStatus status = FormatStatus::Success;

        constexpr bool success() const noexcept {
            return status == FormatStatus::Success;
        }

        constexpr bool truncated() const noexcept {
            return status == FormatStatus::Truncated;
        }

        constexpr explicit operator bool() const noexcept {
            return success();
        }
    };

    namespace format_detail {
        constexpr bool is_alignment(const char value) noexcept {
            return value == '<' || value == '>' || value == '^';
        }

        constexpr bool is_digit(const char value) noexcept {
            return value >= '0' && value <= '9';
        }

        constexpr bool is_supported_type(const char value) noexcept {
            return value == 'd' || value == 'x' || value == 'X' ||
                   value == 'b' || value == 'o' || value == 'f' ||
                   value == 'F' || value == 'e' || value == 'E' ||
                   value == 'g' || value == 'G' || value == 's' ||
                   value == 'c' || value == 'p';
        }

        consteval bool validate_spec(
            const char* format,
            std::size_t& index,
            const std::size_t length) noexcept {
            if (index == length)
                return false;
            if (format[index] == '}')
                return true;
            if (format[index++] != ':')
                return false;

            if (index + 1 < length && is_alignment(format[index + 1]))
                index += 2;
            else if (index < length && is_alignment(format[index]))
                ++index;

            if (index < length && format[index] == '0')
                ++index;
            while (index < length && is_digit(format[index]))
                ++index;

            if (index < length && format[index] == '.') {
                ++index;
                if (index == length || !is_digit(format[index]))
                    return false;
                while (index < length && is_digit(format[index]))
                    ++index;
            }

            if (index < length && format[index] != '}') {
                if (!is_supported_type(format[index]))
                    return false;
                ++index;
            }
            return index < length && format[index] == '}';
        }

        consteval bool validate_format(
            const char* format,
            const std::size_t length,
            const std::size_t argument_count) noexcept {
            std::size_t placeholders = 0;
            for (std::size_t index = 0; index < length; ++index) {
                if (format[index] == '{') {
                    if (index + 1 < length && format[index + 1] == '{') {
                        ++index;
                        continue;
                    }
                    ++index;
                    if (!validate_spec(format, index, length))
                        return false;
                    ++placeholders;
                } else if (format[index] == '}') {
                    if (index + 1 >= length || format[index + 1] != '}')
                        return false;
                    ++index;
                }
            }
            return placeholders == argument_count;
        }

        struct Spec {
            char fill = ' ';
            char alignment = '\0';
            char type = '\0';
            u32 width = 0;
            i32 precision = -1;
            bool zero_padding = false;
        };

        inline bool parse_number(
            const strview text,
            u64& index,
            u32& output) noexcept {
            while (index < text.length() && is_digit(text[index])) {
                const u32 digit = static_cast<u32>(text[index] - '0');
                if (output > (numeric::u32_max - digit) / 10)
                    return false;
                output = output * 10 + digit;
                ++index;
            }
            return true;
        }

        inline bool parse_spec(const strview text, Spec& spec) noexcept {
            u64 index = 0;
            if (index + 1 < text.length() && is_alignment(text[index + 1])) {
                spec.fill = text[index];
                spec.alignment = text[index + 1];
                index += 2;
            } else if (index < text.length() && is_alignment(text[index])) {
                spec.alignment = text[index++];
            }

            if (index < text.length() && text[index] == '0') {
                spec.fill = '0';
                spec.alignment = '>';
                spec.zero_padding = true;
                ++index;
            }

            if (!parse_number(text, index, spec.width))
                return false;

            if (index < text.length() && text[index] == '.') {
                ++index;
                if (index == text.length() || !is_digit(text[index]))
                    return false;
                u32 precision = 0;
                if (!parse_number(text, index, precision) ||
                    precision > static_cast<u32>(numeric::i32_max)) {
                    return false;
                }
                spec.precision = static_cast<i32>(precision);
            }

            if (index < text.length())
                spec.type = text[index++];
            return index == text.length();
        }

        inline FormatResult invalid_format() noexcept {
#if defined(NK_DEV_MODE) && defined(NK_DEBUG) && NK_DEV_MODE <= NK_DEBUG
            constexpr nk::cstr message =
                "nk::format_to rejected an unsupported format/type combination.\n";
            os::write(message, std::char_traits<char>::length(message));
            os::flush();
            os::debug_break();
#endif
            return {FormatStatus::InvalidFormat};
        }

        template <u64 Capacity>
        inline void append_repeated(
            strbuf<Capacity>& output,
            const char character,
            const u64 count) noexcept {
            for (u64 index = 0; index < count; ++index)
                output.append(character);
        }

        template <u64 Capacity>
        inline void append_padded(
            strbuf<Capacity>& output,
            const strview value,
            const Spec& spec,
            const bool numeric,
            const u64 prefix_length = 0) noexcept {
            if (spec.width <= value.length()) {
                output.append(value);
                return;
            }

            const u64 padding = spec.width - value.length();
            const char alignment = spec.alignment == '\0'
                ? (numeric ? '>' : '<')
                : spec.alignment;
            if (alignment == '<') {
                output.append(value);
                append_repeated(output, spec.fill, padding);
            } else if (alignment == '^') {
                const u64 left = padding / 2;
                append_repeated(output, spec.fill, left);
                output.append(value);
                append_repeated(output, spec.fill, padding - left);
            } else if (spec.zero_padding && prefix_length != 0) {
                output.append(value.substr(0, prefix_length));
                append_repeated(output, '0', padding);
                output.append(value.substr(prefix_length));
            } else {
                append_repeated(output, spec.fill, padding);
                output.append(value);
            }
        }

        inline bool string_spec(const Spec& spec) noexcept {
            return spec.type == '\0' || spec.type == 's';
        }

        template <typename Integer, bool IsEnum = std::is_enum_v<Integer>>
        struct integer_value_type {
            using type = Integer;
        };

        template <typename Integer>
        struct integer_value_type<Integer, true> {
            using type = std::underlying_type_t<Integer>;
        };

        template <u64 Capacity>
        bool format_value(
            strbuf<Capacity>& output,
            const strview value,
            const Spec& spec) noexcept {
            if (!string_spec(spec) || spec.precision >= 0)
                return false;
            append_padded(output, value, spec, false);
            return true;
        }

        template <u64 Capacity>
        bool format_value(
            strbuf<Capacity>& output,
            const str& value,
            const Spec& spec) noexcept {
            return format_value(output, value.view(), spec);
        }

        template <u64 OutputCapacity, u64 TextCapacity>
        bool format_value(
            strbuf<OutputCapacity>& output,
            const strbuf<TextCapacity>& value,
            const Spec& spec) noexcept {
            return format_value(output, value.view(), spec);
        }

        template <u64 Capacity>
        bool format_value(
            strbuf<Capacity>& output,
            const char* value,
            const Spec& spec) noexcept {
            return format_value(
                output,
                value == nullptr ? strview{"(null)"} : strview{value},
                spec);
        }

        template <u64 Capacity>
        bool format_value(
            strbuf<Capacity>& output,
            const char value,
            const Spec& spec) noexcept {
            if ((spec.type != '\0' && spec.type != 'c') || spec.precision >= 0)
                return false;
            append_padded(output, strview{&value, 1}, spec, false);
            return true;
        }

        template <u64 Capacity>
        bool format_value(
            strbuf<Capacity>& output,
            const bool value,
            const Spec& spec) noexcept {
            if (!string_spec(spec) || spec.precision >= 0)
                return false;
            append_padded(output, value ? strview{"true"} : strview{"false"}, spec, false);
            return true;
        }

        template <u64 Capacity, typename Integer>
            requires ((std::integral<Integer> &&
                       !std::same_as<std::remove_cv_t<Integer>, bool> &&
                       !std::same_as<std::remove_cv_t<Integer>, char>) ||
                      std::is_enum_v<Integer>)
        bool format_value(
            strbuf<Capacity>& output,
            const Integer value,
            const Spec& spec) noexcept {
            if (spec.precision >= 0)
                return false;

            i32 base = 10;
            if (spec.type == 'x' || spec.type == 'X')
                base = 16;
            else if (spec.type == 'b')
                base = 2;
            else if (spec.type == 'o')
                base = 8;
            else if (spec.type != '\0' && spec.type != 'd')
                return false;

            using Value = typename integer_value_type<Integer>::type;
            const Value converted = static_cast<Value>(value);
            char buffer[std::numeric_limits<Value>::digits + 3]{};
            const auto result = std::to_chars(
                buffer,
                buffer + sizeof(buffer),
                converted,
                base);
            if (result.ec != std::errc{})
                return false;

            if (spec.type == 'X') {
                for (char* current = buffer; current != result.ptr; ++current) {
                    if (*current >= 'a' && *current <= 'f')
                        *current = static_cast<char>(*current - 'a' + 'A');
                }
            }

            const strview text{buffer, static_cast<u64>(result.ptr - buffer)};
            const u64 prefix = text.length() != 0 && text[0] == '-' ? 1 : 0;
            append_padded(output, text, spec, true, prefix);
            return true;
        }

        template <u64 Capacity, std::floating_point Float>
        bool format_value(
            strbuf<Capacity>& output,
            const Float value,
            const Spec& spec) noexcept {
            std::chars_format style = std::chars_format::general;
            if (spec.type == 'f' || spec.type == 'F')
                style = std::chars_format::fixed;
            else if (spec.type == 'e' || spec.type == 'E')
                style = std::chars_format::scientific;
            else if (spec.type != '\0' && spec.type != 'g' && spec.type != 'G')
                return false;

            char buffer[128]{};
            const auto result = spec.precision >= 0
                ? std::to_chars(
                      buffer,
                      buffer + sizeof(buffer),
                      value,
                      style,
                      spec.precision)
                : std::to_chars(buffer, buffer + sizeof(buffer), value, style);
            if (result.ec != std::errc{})
                return false;

            if (spec.type == 'E' || spec.type == 'F' || spec.type == 'G') {
                for (char* current = buffer; current != result.ptr; ++current) {
                    if (*current >= 'a' && *current <= 'z')
                        *current = static_cast<char>(*current - 'a' + 'A');
                }
            }

            const strview text{buffer, static_cast<u64>(result.ptr - buffer)};
            const u64 prefix = text.length() != 0 && text[0] == '-' ? 1 : 0;
            append_padded(output, text, spec, true, prefix);
            return true;
        }

        template <u64 Capacity, typename Pointer>
            requires (std::is_pointer_v<Pointer> &&
                      !std::same_as<std::remove_cv_t<
                          std::remove_pointer_t<Pointer>>, char>)
        bool format_value(
            strbuf<Capacity>& output,
            const Pointer value,
            const Spec& spec) noexcept {
            if ((spec.type != '\0' && spec.type != 'p') || spec.precision >= 0)
                return false;

            char digits[sizeof(std::uintptr_t) * 2]{};
            const auto result = std::to_chars(
                digits,
                digits + sizeof(digits),
                reinterpret_cast<std::uintptr_t>(value),
                16);
            if (result.ec != std::errc{})
                return false;

            strbuf<2 + sizeof(std::uintptr_t) * 2> text;
            text.append("0x");
            text.append(strview{digits, static_cast<u64>(result.ptr - digits)});
            append_padded(output, text.view(), spec, true, 2);
            return true;
        }

        template <std::size_t Index = 0, u64 Capacity, typename Tuple>
        bool format_argument(
            strbuf<Capacity>& output,
            const u64 requested,
            Tuple& arguments,
            const Spec& spec) noexcept {
            if constexpr (Index == std::tuple_size_v<Tuple>) {
                return false;
            } else {
                if (requested == Index)
                    return format_value(output, std::get<Index>(arguments), spec);
                return format_argument<Index + 1>(
                    output,
                    requested,
                    arguments,
                    spec);
            }
        }
    }

    template <typename... Args>
    class format_string {
    public:
        template <std::size_t Length>
        consteval format_string(const char (&format)[Length]) noexcept
            : m_data{format},
              m_length{Length == 0 ? 0 : Length - 1} {
            if (!format_detail::validate_format(
                    format,
                    m_length,
                    sizeof...(Args))) {
                std::abort();
            }
        }

        constexpr strview view() const noexcept {
            return {m_data, static_cast<u64>(m_length)};
        }

    private:
        const char* m_data;
        std::size_t m_length;
    };

    template <u64 Capacity, typename... Args>
    FormatResult format_to(
        strbuf<Capacity>& output,
        format_string<std::type_identity_t<Args>...> format,
        Args&&... args) noexcept {
        output.clear();
        const strview pattern = format.view();
        auto arguments = std::forward_as_tuple(std::forward<Args>(args)...);
        u64 argument_index = 0;

        for (u64 index = 0; index < pattern.length(); ++index) {
            if (pattern[index] == '{') {
                if (index + 1 < pattern.length() && pattern[index + 1] == '{') {
                    output.append('{');
                    ++index;
                    continue;
                }

                const u64 begin = ++index;
                while (index < pattern.length() && pattern[index] != '}')
                    ++index;

                strview encoded_spec = pattern.substr(begin, index - begin);
                if (!encoded_spec.empty())
                    encoded_spec = encoded_spec.substr(1);
                format_detail::Spec spec;
                if (!format_detail::parse_spec(encoded_spec, spec) ||
                    !format_detail::format_argument(
                        output,
                        argument_index++,
                        arguments,
                        spec)) {
                    return format_detail::invalid_format();
                }
            } else if (pattern[index] == '}') {
                output.append('}');
                ++index;
            } else {
                output.append(pattern[index]);
            }
        }

        return {
            output.truncated()
                ? FormatStatus::Truncated
                : FormatStatus::Success,
        };
    }
}
