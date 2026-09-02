#include "nkpch.h"

#include "resources/image_loader.h"

#include <algorithm>
#include <cstddef>
#include <limits>

#include <spng.h>

#include "memory/allocator.h"
#include "platform/file.h"

namespace nk {
    namespace {
        constexpr u32 max_image_dimension = 16'384;
        constexpr u64 max_cached_chunk_bytes = MiB(64);

        struct alignas(std::max_align_t) DecoderAllocationHeader {
            mem::Allocator* allocator;
            u64 total_size;
            u64 payload_size;
        };

        static_assert(
            sizeof(DecoderAllocationHeader) % alignof(DecoderAllocationHeader) == 0);

        thread_local mem::Allocator* active_decoder_allocator = nullptr;

        class DecoderAllocatorScope {
        public:
            explicit DecoderAllocatorScope(mem::Allocator& allocator) noexcept
                : m_previous{active_decoder_allocator} {
                active_decoder_allocator = &allocator;
            }

            ~DecoderAllocatorScope() {
                active_decoder_allocator = m_previous;
            }

            DecoderAllocatorScope(const DecoderAllocatorScope&) = delete;
            DecoderAllocatorScope& operator=(const DecoderAllocatorScope&) = delete;

        private:
            mem::Allocator* m_previous;
        };

        void* allocate_decoder_memory(
            mem::Allocator* allocator,
            const std::size_t payload_size,
            const bool zeroed) noexcept {
            if (allocator == nullptr || payload_size == 0 ||
                payload_size > std::numeric_limits<u64>::max() -
                    sizeof(DecoderAllocationHeader)) {
                return nullptr;
            }

            const u64 total_size =
                static_cast<u64>(payload_size) + sizeof(DecoderAllocationHeader);
            auto* header = static_cast<DecoderAllocationHeader*>(
                allocator->allocate_raw(total_size, alignof(DecoderAllocationHeader)));
            if (header == nullptr)
                return nullptr;

            header->allocator = allocator;
            header->total_size = total_size;
            header->payload_size = static_cast<u64>(payload_size);
            void* payload = header + 1;
            if (zeroed)
                std::memset(payload, 0, payload_size);
            return payload;
        }

        void* decoder_malloc(const std::size_t size) noexcept {
            return allocate_decoder_memory(active_decoder_allocator, size, false);
        }

        void* decoder_calloc(
            const std::size_t count,
            const std::size_t size) noexcept {
            if (count == 0 || size == 0 ||
                count > std::numeric_limits<std::size_t>::max() / size) {
                return nullptr;
            }
            return allocate_decoder_memory(
                active_decoder_allocator,
                count * size,
                true);
        }

        void decoder_free(void* data) noexcept {
            if (data == nullptr)
                return;
            auto* header = static_cast<DecoderAllocationHeader*>(data) - 1;
            header->allocator->free_raw(header, header->total_size);
        }

        void* decoder_realloc(void* data, const std::size_t size) noexcept {
            if (data == nullptr)
                return decoder_malloc(size);
            if (size == 0) {
                decoder_free(data);
                return nullptr;
            }

            auto* old_header = static_cast<DecoderAllocationHeader*>(data) - 1;
            void* replacement = allocate_decoder_memory(
                old_header->allocator,
                size,
                false);
            if (replacement == nullptr)
                return nullptr;

            std::memcpy(
                replacement,
                data,
                std::min(old_header->payload_size, static_cast<u64>(size)));
            decoder_free(data);
            return replacement;
        }

        class DecoderContext {
        public:
            explicit DecoderContext(spng_ctx* context) noexcept
                : m_context{context} {}

            ~DecoderContext() {
                if (m_context != nullptr)
                    spng_ctx_free(m_context);
            }

            DecoderContext(const DecoderContext&) = delete;
            DecoderContext& operator=(const DecoderContext&) = delete;

            spng_ctx* get() const noexcept { return m_context; }

        private:
            spng_ctx* m_context;
        };

        image_error decoder_error(const i32 native_code) noexcept {
            image_error_code code = image_error_code::decode_failed;
            if (native_code == SPNG_EMEM)
                code = image_error_code::out_of_memory;
            else if (native_code == SPNG_EOVERFLOW ||
                     native_code == SPNG_EBUFSIZ ||
                     native_code == SPNG_EUSER_WIDTH ||
                     native_code == SPNG_EUSER_HEIGHT) {
                code = image_error_code::limits_exceeded;
            }
            return {
                .code = code,
                .file = file_error::none,
                .native_code = native_code,
            };
        }

        void flip_rows(DecodedImage& image) noexcept {
            const u64 row_size =
                static_cast<u64>(image.width) * image.channel_count;
            for (u32 row = 0; row < image.height / 2; ++row) {
                u8* top = image.pixels.data() + row * row_size;
                u8* bottom =
                    image.pixels.data() + (image.height - row - 1) * row_size;
                for (u64 byte = 0; byte < row_size; ++byte)
                    std::swap(top[byte], bottom[byte]);
            }
        }
    }

    result<DecodedImage, image_error> ImageLoader::load_png(
        mem::Allocator& allocator,
        const strview path,
        const bool flip_vertical) {
        File file{allocator};
        auto opened = file.open(path, FileMode::Read, true);
        if (!opened) {
            return err(image_error{
                .code = image_error_code::file_failed,
                .file = opened.error(),
                .native_code = 0,
            });
        }

        auto encoded = file.read_all_bytes();
        if (!encoded) {
            return err(image_error{
                .code = image_error_code::file_failed,
                .file = encoded.error(),
                .native_code = 0,
            });
        }
        auto closed = file.close();
        if (!closed) {
            return err(image_error{
                .code = image_error_code::file_failed,
                .file = closed.error(),
                .native_code = 0,
            });
        }

        DecoderAllocatorScope allocator_scope{allocator};
        spng_alloc decoder_alloc{
            .malloc_fn = decoder_malloc,
            .realloc_fn = decoder_realloc,
            .calloc_fn = decoder_calloc,
            .free_fn = decoder_free,
        };
        DecoderContext context{spng_ctx_new2(&decoder_alloc, 0)};
        if (context.get() == nullptr) {
            return err(image_error{
                .code = image_error_code::out_of_memory,
                .file = file_error::none,
                .native_code = SPNG_EMEM,
            });
        }

        i32 decoder_result = spng_set_image_limits(
            context.get(), max_image_dimension, max_image_dimension);
        if (decoder_result != 0)
            return err(decoder_error(decoder_result));
        decoder_result = spng_set_chunk_limits(
            context.get(), max_cached_chunk_bytes, max_cached_chunk_bytes);
        if (decoder_result != 0)
            return err(decoder_error(decoder_result));
        decoder_result = spng_set_png_buffer(
            context.get(), encoded->data(), encoded->length());
        if (decoder_result != 0)
            return err(decoder_error(decoder_result));

        spng_ihdr header{};
        decoder_result = spng_get_ihdr(context.get(), &header);
        if (decoder_result != 0)
            return err(decoder_error(decoder_result));

        std::size_t decoded_size = 0;
        decoder_result = spng_decoded_image_size(
            context.get(), SPNG_FMT_RGBA8, &decoded_size);
        if (decoder_result != 0)
            return err(decoder_error(decoder_result));

        DecodedImage image{};
        image.width = header.width;
        image.height = header.height;
        image.channel_count = 4;
        if (!image.pixels.dyarr_init_len(
                &allocator,
                static_cast<u64>(decoded_size),
                static_cast<u64>(decoded_size))) {
            return err(image_error{
                .code = image_error_code::out_of_memory,
                .file = file_error::none,
                .native_code = SPNG_EMEM,
            });
        }

        decoder_result = spng_decode_image(
            context.get(),
            image.pixels.data(),
            decoded_size,
            SPNG_FMT_RGBA8,
            SPNG_DECODE_TRNS);
        if (decoder_result != 0)
            return err(decoder_error(decoder_result));

        if (flip_vertical)
            flip_rows(image);

        for (u64 alpha = 3; alpha < image.pixels.length(); alpha += 4) {
            if (image.pixels[alpha] < 255) {
                image.has_transparency = true;
                break;
            }
        }

        return ok(std::move(image));
    }
}
