#include "system/logging_system.h"
#include "core/map.h"
#include "memory/malloc_allocator.h"

int main(void) {
    nk::LoggingSystem::init();

    DebugLog("{} test", 1);

    nk::MallocAllocator allocator;
    {
        nk::Map<nk::cstr, nk::u32> values;
        values.init_list_own(&allocator, {
            {
                .key = "Test",
                .value = 5,
            },
            {
                .key = "Test 2",
                .value = 6,
            },
            {
                .key = "Test 3",
                .value = 7,
            },
        });

        for (auto& key_value : values) {
            InfoLog("key: {} | value: {}", key_value.key, key_value.value);
        }

        values.insert("Test 2", 100);

        WarnLog("{}", values["Test 2"].has_value() ? values["Test 2"]->get() : 0);

        values.remove("Test 2");

        WarnLog("{}", values["Test 2"].has_value() ? values["Test 2"]->get() : 0);
    }

    nk::LoggingSystem::shutdown();
    return 0;
}
