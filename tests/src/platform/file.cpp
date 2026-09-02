#include <cstdio>
#include <cstring>

#include <gtest/gtest.h>

#include "platform/file.h"
#include "memory/malloc_allocator.h"

#if defined(NK_PLATFORM_LINUX)
namespace {
    class FailingAllocator final : public nk::mem::Allocator {
    public:
        void init() noexcept {}
        nk::cstr to_cstr() const noexcept override { return "FailingAllocator"; }

    protected:
        void* _do_allocate(nk::u64, nk::u64) noexcept override {
            return nullptr;
        }

        bool _do_free(void*, nk::u64) noexcept override {
            return false;
        }
    };

    class TemporaryFile {
    public:
        TemporaryFile(const nk::u8* data, const nk::u64 length) {
            const int descriptor = mkstemp(m_path);
            if (descriptor < 0)
                return;

            nk::u64 written = 0;
            while (written < length) {
                const auto result = ::write(
                    descriptor,
                    data + written,
                    static_cast<std::size_t>(length - written));
                if (result <= 0)
                    break;
                written += static_cast<nk::u64>(result);
            }
            ::close(descriptor);
            m_valid = written == length;
        }

        ~TemporaryFile() {
            if (m_path[0] != '\0')
                unlink(m_path);
        }

        TemporaryFile(const TemporaryFile&) = delete;
        TemporaryFile& operator=(const TemporaryFile&) = delete;

        nk::cstr path() const noexcept { return m_path; }
        bool valid() const noexcept { return m_valid; }

    private:
        char m_path[64] = "/tmp/nk-engine-file-XXXXXX";
        bool m_valid = false;
    };

    constexpr nk::u8 file_contents[]{'a', 'l', 'p', 'h', 'a', '\n', 'b', 'e', 't', 'a'};
}

TEST(File, ReportsOpenStateAndTypedOpenFailures) {
    TemporaryFile source{file_contents, sizeof(file_contents)};
    ASSERT_TRUE(source.valid());

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::File file{allocator};
    EXPECT_TRUE(nk::File::exists(source.path()));
    EXPECT_FALSE(nk::File::exists("/tmp/nk-engine-file-does-not-exist"));

    auto invalid_mode = file.open(source.path(), nk::FileMode::None, false);
    ASSERT_FALSE(invalid_mode);
    EXPECT_EQ(invalid_mode.error(), nk::file_error::invalid_mode);

    auto missing = file.open(
        "/tmp/nk-engine-file-does-not-exist",
        nk::FileMode::Read,
        true);
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error(), nk::file_error::not_found);

    ASSERT_TRUE(file.open(source.path(), nk::FileMode::Read, false));
    EXPECT_TRUE(file.is_open());
    auto repeated = file.open(source.path(), nk::FileMode::Read, false);
    ASSERT_FALSE(repeated);
    EXPECT_EQ(repeated.error(), nk::file_error::already_open);

    EXPECT_TRUE(file.close());
    EXPECT_FALSE(file.is_open());
    EXPECT_TRUE(file.close());
}

TEST(File, SeparatesLinesEndOfFileAndIoFailures) {
    TemporaryFile source{file_contents, sizeof(file_contents)};
    ASSERT_TRUE(source.valid());

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::File file{allocator};
    nk::str line{allocator};
    ASSERT_TRUE(file.open(source.path(), nk::FileMode::Read, false));

    auto first = file.read_line(line);
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, nk::read_line_outcome::line);
    EXPECT_EQ(line.view(), nk::strview{"alpha\n"});

    auto second = file.read_line(line);
    ASSERT_TRUE(second);
    EXPECT_EQ(*second, nk::read_line_outcome::line);
    EXPECT_EQ(line.view(), nk::strview{"beta"});

    auto end = file.read_line(line);
    ASSERT_TRUE(end);
    EXPECT_EQ(*end, nk::read_line_outcome::end_of_file);
    EXPECT_TRUE(file.close());

    auto closed_read = file.read_line(line);
    ASSERT_FALSE(closed_read);
    EXPECT_EQ(closed_read.error(), nk::file_error::not_open);
}

TEST(File, ReturnsPartialReadCountsWithoutTreatingEofAsFailure) {
    constexpr nk::u8 short_contents[]{'a', 'b', 'c'};
    TemporaryFile source{short_contents, sizeof(short_contents)};
    ASSERT_TRUE(source.valid());

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::File file{allocator};
    ASSERT_TRUE(file.open(source.path(), nk::FileMode::Read, true));

    nk::u8 output[8]{};
    auto partial = file.read(nk::cl::slice<nk::u8>{output});
    ASSERT_TRUE(partial);
    EXPECT_EQ(*partial, sizeof(short_contents));
    EXPECT_EQ(std::memcmp(output, short_contents, sizeof(short_contents)), 0);

    auto end = file.read(nk::cl::slice<nk::u8>{output});
    ASSERT_TRUE(end);
    EXPECT_EQ(*end, 0);
    EXPECT_TRUE(file.close());
}

TEST(File, TransfersReadAllOwnershipThroughDyarr) {
    TemporaryFile source{file_contents, sizeof(file_contents)};
    ASSERT_TRUE(source.valid());

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    {
        nk::File file{allocator};
        ASSERT_TRUE(file.open(source.path(), nk::FileMode::Read, true));
        const nk::u64 path_allocations = allocator.get_active_allocation_count();

        {
            auto contents = file.read_all_bytes();
            ASSERT_TRUE(contents);
            ASSERT_EQ(contents->length(), sizeof(file_contents));
            EXPECT_EQ(
                std::memcmp(contents->data(), file_contents, sizeof(file_contents)),
                0);
            EXPECT_EQ(
                allocator.get_active_allocation_count(),
                path_allocations + 1);
        }
        EXPECT_EQ(allocator.get_active_allocation_count(), path_allocations);
        EXPECT_TRUE(file.close());
        EXPECT_EQ(allocator.get_active_allocation_count(), path_allocations);
    }
    EXPECT_EQ(allocator.get_active_allocation_count(), 0);
}

TEST(File, PreservesOutOfMemoryAndSeekFailures) {
    FailingAllocator failing_allocator;
    ASSERT_NE(
        failing_allocator._allocator_init_untracked<FailingAllocator>(),
        nullptr);
    nk::File allocation_failure{failing_allocator};
    ASSERT_TRUE(allocation_failure.open("/etc/hosts", nk::FileMode::Read, true));
    auto no_memory = allocation_failure.read_all_bytes();
    ASSERT_FALSE(no_memory);
    EXPECT_EQ(no_memory.error(), nk::file_error::out_of_memory);
    EXPECT_TRUE(allocation_failure.close());

    int descriptors[2]{};
    ASSERT_EQ(pipe(descriptors), 0);
    constexpr char pipe_data[] = "not seekable";
    ASSERT_EQ(
        ::write(descriptors[1], pipe_data, sizeof(pipe_data)),
        static_cast<ssize_t>(sizeof(pipe_data)));
    ::close(descriptors[1]);

    char descriptor_path[64]{};
    ASSERT_GT(
        std::snprintf(
            descriptor_path,
            sizeof(descriptor_path),
            "/proc/self/fd/%d",
            descriptors[0]),
        0);

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::File seek_failure{allocator};
    ASSERT_TRUE(seek_failure.open(descriptor_path, nk::FileMode::Read, true));
    auto not_seekable = seek_failure.read_all_bytes();
    ASSERT_FALSE(not_seekable);
    EXPECT_EQ(not_seekable.error(), nk::file_error::seek_failed);
    EXPECT_TRUE(seek_failure.close());
    ::close(descriptors[0]);
}

TEST(File, WritesSlicesLinesAndReportsDeviceFailures) {
    TemporaryFile destination{nullptr, 0};
    ASSERT_TRUE(destination.valid());

    nk::mem::MallocAllocator allocator{nk::mem::untracked};
    nk::File file{allocator};
    ASSERT_TRUE(file.open(destination.path(), nk::FileMode::Write, true));

    constexpr nk::u8 prefix[]{'x', 'y'};
    auto bytes_written = file.write(nk::cl::slice<const nk::u8>{prefix});
    ASSERT_TRUE(bytes_written);
    EXPECT_EQ(*bytes_written, sizeof(prefix));
    EXPECT_TRUE(file.write_line("line"));
    EXPECT_TRUE(file.close());

    nk::File read_only{allocator};
    ASSERT_TRUE(read_only.open(destination.path(), nk::FileMode::Read, true));
    auto forbidden = read_only.write(nk::cl::slice<const nk::u8>{prefix});
    ASSERT_FALSE(forbidden);
    EXPECT_EQ(forbidden.error(), nk::file_error::operation_not_permitted);
    EXPECT_TRUE(read_only.close());

    nk::File full_device{allocator};
    ASSERT_TRUE(full_device.open("/dev/full", nk::FileMode::Write, true));
    auto device_failure = full_device.write(nk::cl::slice<const nk::u8>{prefix});
    ASSERT_FALSE(device_failure);
    EXPECT_EQ(device_failure.error(), nk::file_error::write_failed);
    auto device_close = full_device.close();
    if (!device_close) {
        EXPECT_EQ(device_close.error(), nk::file_error::close_failed);
    }
}
#endif
