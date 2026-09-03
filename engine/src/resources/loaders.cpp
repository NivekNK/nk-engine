#include "nkpch.h"

#include "resources/loaders.h"

#include <cerrno>

#include "collections/dyarr.h"
#include "core/format.h"
#include "core/str.h"
#include "memory/allocator.h"
#include "platform/file.h"
#include "resources/image_loader.h"
#include "resources/material.h"

namespace nk {
    namespace {
        bool prepare_resource(
            Resource& resource,
            const strview asset_base_path,
            const strview type_path,
            const strview name,
            const strview extension = {}) noexcept {
            if (name.empty() || !resource.name.assign(name))
                return false;

            if (type_path.empty()) {
                return static_cast<bool>(format_to(
                    resource.full_path,
                    "{}/{}{}",
                    asset_base_path,
                    name,
                    extension));
            }
            return static_cast<bool>(format_to(
                resource.full_path,
                "{}/{}/{}{}",
                asset_base_path,
                type_path,
                name,
                extension));
        }

        resource_error file_failure(const file_error error) noexcept {
            return {
                resource_error_code::file_failed,
                static_cast<i32>(error),
            };
        }

        strview trim(const strview text) noexcept {
            u64 begin = 0;
            u64 end = text.length();
            while (begin < end) {
                const char character = text[begin];
                if (character != ' ' && character != '\t' &&
                    character != '\r' && character != '\n') {
                    break;
                }
                ++begin;
            }
            while (end > begin) {
                const char character = text[end - 1];
                if (character != ' ' && character != '\t' &&
                    character != '\r' && character != '\n') {
                    break;
                }
                --end;
            }
            return text.substr(begin, end - begin);
        }

        bool parse_color(const strview text, glm::vec4& color) noexcept {
            strbuf<255> buffer{text};
            if (buffer.truncated())
                return false;

            const char* cursor = buffer.cstr();
            char* end = nullptr;
            glm::vec4 parsed{};
            for (u32 component = 0; component < 4; ++component) {
                errno = 0;
                parsed[component] = std::strtof(cursor, &end);
                if (end == cursor || errno == ERANGE)
                    return false;
                cursor = end;
            }
            while (*cursor == ' ' || *cursor == '\t' ||
                   *cursor == '\r' || *cursor == '\n') {
                ++cursor;
            }
            if (*cursor != '\0')
                return false;
            color = parsed;
            return true;
        }

        bool parse_f32(const strview text, f32& value) noexcept {
            strbuf<63> buffer{text};
            if (buffer.truncated())
                return false;

            errno = 0;
            char* end = nullptr;
            const f32 parsed = std::strtof(buffer.cstr(), &end);
            if (end == buffer.cstr() || errno == ERANGE ||
                !std::isfinite(parsed)) {
                return false;
            }
            while (*end == ' ' || *end == '\t' ||
                   *end == '\r' || *end == '\n') {
                ++end;
            }
            if (*end != '\0')
                return false;
            value = parsed;
            return true;
        }
    }

    result<void, resource_error> TextResourceLoader::load(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const strview name,
        Resource& out_resource) const {
        if (!prepare_resource(
                out_resource,
                asset_base_path,
                type_path(),
                name)) {
            return err(resource_error{resource_error_code::invalid_name, 0});
        }

        File file{allocator};
        auto opened = file.open(out_resource.full_path.view(), FileMode::Read, true);
        if (!opened)
            return err(file_failure(opened.error()));
        auto bytes = file.read_all_bytes();
        if (!bytes)
            return err(file_failure(bytes.error()));
        auto closed = file.close();
        if (!closed)
            return err(file_failure(closed.error()));

        str* text = allocator.construct_t(str, allocator);
        if (text == nullptr)
            return err(resource_error{resource_error_code::out_of_memory, 0});
        if (!text->assign(strview{
                reinterpret_cast<const char*>(bytes->data()),
                bytes->length()})) {
            allocator.deconstruct_t(str, text);
            return err(resource_error{resource_error_code::out_of_memory, 0});
        }

        out_resource.data_size = text->length();
        out_resource.data = text;
        return ok();
    }

    void TextResourceLoader::unload(
        mem::Allocator& allocator,
        Resource& resource) const noexcept {
        if (resource.data != nullptr)
            allocator.deconstruct_t(str, resource.as<str>());
        resource.data = nullptr;
        resource.data_size = 0;
    }

    result<void, resource_error> BinaryResourceLoader::load(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const strview name,
        Resource& out_resource) const {
        if (!prepare_resource(
                out_resource,
                asset_base_path,
                type_path(),
                name)) {
            return err(resource_error{resource_error_code::invalid_name, 0});
        }

        File file{allocator};
        auto opened = file.open(out_resource.full_path.view(), FileMode::Read, true);
        if (!opened)
            return err(file_failure(opened.error()));
        auto bytes = file.read_all_bytes();
        if (!bytes)
            return err(file_failure(bytes.error()));
        auto closed = file.close();
        if (!closed)
            return err(file_failure(closed.error()));

        const u64 byte_count = bytes->length();
        cl::dyarr<u8>* data = allocator.construct_t(
            cl::dyarr<u8>,
            std::move(*bytes));
        if (data == nullptr)
            return err(resource_error{resource_error_code::out_of_memory, 0});

        out_resource.data_size = byte_count;
        out_resource.data = data;
        return ok();
    }

    void BinaryResourceLoader::unload(
        mem::Allocator& allocator,
        Resource& resource) const noexcept {
        if (resource.data != nullptr) {
            allocator.deconstruct_t(
                cl::dyarr<u8>,
                resource.as<cl::dyarr<u8>>());
        }
        resource.data = nullptr;
        resource.data_size = 0;
    }

    result<void, resource_error> ImageResourceLoader::load(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const strview name,
        Resource& out_resource) const {
        if (!prepare_resource(
                out_resource,
                asset_base_path,
                type_path(),
                name,
                ".png")) {
            return err(resource_error{resource_error_code::invalid_name, 0});
        }

        auto decoded = ImageLoader::load_png(
            allocator,
            out_resource.full_path.view());
        if (!decoded) {
            const image_error error = decoded.error();
            if (error.code == image_error_code::out_of_memory) {
                return err(resource_error{
                    resource_error_code::out_of_memory,
                    error.native_code,
                });
            }
            if (error.code == image_error_code::file_failed) {
                return err(resource_error{
                    resource_error_code::file_failed,
                    static_cast<i32>(error.file),
                });
            }
            return err(resource_error{
                resource_error_code::decode_failed,
                error.native_code,
            });
        }

        DecodedImage* image = allocator.construct_t(
            DecodedImage,
            std::move(*decoded));
        if (image == nullptr)
            return err(resource_error{resource_error_code::out_of_memory, 0});

        out_resource.data_size = image->pixels.length();
        out_resource.data = image;
        return ok();
    }

    void ImageResourceLoader::unload(
        mem::Allocator& allocator,
        Resource& resource) const noexcept {
        if (resource.data != nullptr)
            allocator.deconstruct_t(DecodedImage, resource.as<DecodedImage>());
        resource.data = nullptr;
        resource.data_size = 0;
    }

    result<void, resource_error> MaterialResourceLoader::load(
        mem::Allocator& allocator,
        const strview asset_base_path,
        const strview name,
        Resource& out_resource) const {
        if (!prepare_resource(
                out_resource,
                asset_base_path,
                type_path(),
                name,
                ".kmt")) {
            return err(resource_error{resource_error_code::invalid_name, 0});
        }

        File file{allocator};
        auto opened = file.open(out_resource.full_path.view(), FileMode::Read, false);
        if (!opened)
            return err(file_failure(opened.error()));

        MaterialConfig parsed{};
        parsed.name.assign(name);
        str line{allocator};
        while (true) {
            auto read = file.read_line(line);
            if (!read)
                return err(file_failure(read.error()));
            if (*read == read_line_outcome::end_of_file)
                break;

            const strview content = trim(line.view());
            if (content.empty() || content[0] == '#')
                continue;
            const u64 separator = content.find('=');
            if (separator == strview::npos)
                continue;

            const strview key = trim(content.substr(0, separator));
            const strview value = trim(content.substr(separator + 1));
            if (key == strview{"name", 4}) {
                if (!parsed.name.assign(value)) {
                    return err(resource_error{
                        resource_error_code::invalid_data,
                        0,
                    });
                }
            } else if (key == strview{"shader", 6}) {
                if (!parsed.shader_name.assign(value)) {
                    return err(resource_error{
                        resource_error_code::invalid_data,
                        0,
                    });
                }
            } else if (key == strview{"type", 4}) {
                if (value == strview{"world", 5}) {
                    parsed.type = MaterialType::world;
                } else if (value == strview{"ui", 2}) {
                    parsed.type = MaterialType::ui;
                } else {
                    return err(resource_error{
                        resource_error_code::invalid_data,
                        0,
                    });
                }
            } else if (key == strview{"diffuse_map_name", 16}) {
                if (!parsed.diffuse_map_name.assign(value)) {
                    return err(resource_error{
                        resource_error_code::invalid_data,
                        0,
                    });
                }
            } else if (key == strview{"specular_map_name", 17}) {
                if (!parsed.specular_map_name.assign(value)) {
                    return err(resource_error{
                        resource_error_code::invalid_data,
                        0,
                    });
                }
            } else if (key == strview{"normal_map_name", 15}) {
                if (!parsed.normal_map_name.assign(value)) {
                    return err(resource_error{
                        resource_error_code::invalid_data,
                        0,
                    });
                }
            } else if (key == strview{"shininess", 9}) {
                if (!parse_f32(value, parsed.shininess) ||
                    parsed.shininess <= 0.0f) {
                    return err(resource_error{
                        resource_error_code::invalid_data,
                        0,
                    });
                }
            } else if (key == strview{"diffuse_colour", 14} &&
                       !parse_color(value, parsed.diffuse_color)) {
                return err(resource_error{
                    resource_error_code::invalid_data,
                    0,
                });
            }
        }

        auto closed = file.close();
        if (!closed)
            return err(file_failure(closed.error()));

        if (parsed.shader_name.empty()) {
            parsed.shader_name.assign(
                parsed.type == MaterialType::world
                    ? strview{"Builtin.MaterialShader", 22}
                    : strview{"Builtin.UIShader", 16});
        }

        MaterialConfig* config = allocator.construct_t(MaterialConfig, parsed);
        if (config == nullptr)
            return err(resource_error{resource_error_code::out_of_memory, 0});
        out_resource.data_size = sizeof(MaterialConfig);
        out_resource.data = config;
        return ok();
    }

    void MaterialResourceLoader::unload(
        mem::Allocator& allocator,
        Resource& resource) const noexcept {
        if (resource.data != nullptr) {
            allocator.deconstruct_t(
                MaterialConfig,
                resource.as<MaterialConfig>());
        }
        resource.data = nullptr;
        resource.data_size = 0;
    }
}
