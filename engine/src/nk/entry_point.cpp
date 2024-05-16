#include "nkpch.h"

#include "nk/entry_point.h"

#include "system/memory_system.h"

// REMOVE
#include "core/map.h"
#include "core/dyarr.h"
#include "memory/malloc_allocator.h"

#include "memory/memory_type.h"
NK_EXTEND_MEMORY_TYPE(Test)

struct Test {
    nk::u32 count;
    nk::Dyarr<nk::u32> values;

    // Test(const Test&) = delete;
    // Test& operator=(const Test&) = delete;

    // ~Test() {
    //     values.free();
    // }
    // void deconstruct() {
    //     values.free();
    // }
};
// REMOVE


namespace nk {
    i32 entry_point(i32 argc, char** argv) {
        NK_MEMORY_SYSTEM_INIT();

        {
            LoggingSystem::init();
            DebugLogIf(NK_MEMORY_SYSTEM_IS_INITIALIZED(), "MemorySystem initialized.");
            InfoLog("LoggingSystem initialized.");

            NK_MEMORY_SYSTEM_EXPANDED_MEMORY_TYPE(nk::MemoryType::extended_max(), nk::MemoryType::extended_to_cstr);

            // REMOVE
            nk::MallocAllocator allocator;
            allocator.allocator_init("Test Allocator", nk::MemoryType::Test);
            {

                auto testing = Test {
                    .count = 10,
                };
                testing.values.init(&allocator, 4);
                testing.values.insert(0, 0);
                testing.values.insert(1, 1);
                testing.values.insert(2, 2);

                nk::Map<nk::cstr, Test> values;
                values.init(&allocator);
                values.insert_move("test value", testing);

                auto& value = values.insert_use("test value 2");
                value.count = 6;
                value.values.init(&allocator, value.count);
                value.values.insert(5, 2);

                nk::Dyarr<Test> dyarr_test;
                dyarr_test.init(&allocator, 5);

                Test& test = dyarr_test.insert_use(0);
                test.values.init(&allocator, 5);

                Test& test2 = dyarr_test.insert_use(1);
                test2.values.init(&allocator, 5);

                NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT();

                if(auto test_value_2_opt = values.get("test value 2")) {
                    auto& test_value_2 = test_value_2_opt->get();
                    DebugLog("Count: {}", test_value_2.values.length());
                    for (const auto& v : test_value_2.values) {
                        DebugLog("{}", v);
                    }
                }

                // values.free();
            }
            // REMOVE
            NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT();

            InfoLog("LoggingSystem shutdown.");
            DebugLogIf(NK_MEMORY_SYSTEM_IS_INITIALIZED(), "MemorySystem shutdown.");
            LoggingSystem::shutdown();
        }

        NK_MEMORY_SYSTEM_LOG_DETAILED_REPORT();
        NK_MEMORY_SYSTEM_SHUTDOWN();

        return EXIT_SUCCESS;
    }
}
