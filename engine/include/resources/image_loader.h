#pragma once

#include "collections/dyarr.h"
#include "core/result.h"
#include "core/strview.h"

namespace nk {
    namespace mem { class Allocator; }
    enum class file_error : u8;

    enum class image_error_code : u8 {
        file_failed,
        decode_failed,
        limits_exceeded,
        out_of_memory,
    };

    struct image_error {
        image_error_code code;
        file_error file;
        i32 native_code;
    };

    struct DecodedImage {
        u32 width = 0;
        u32 height = 0;
        u8 channel_count = 0;
        bool has_transparency = false;
        cl::dyarr<u8> pixels;
    };

    class ImageLoader final {
    public:
        [[nodiscard]] static result<DecodedImage, image_error> load_png(
            mem::Allocator& allocator,
            strview path,
            bool flip_vertical = true);

    private:
        ImageLoader() = delete;
    };
}
