#include <gtest/gtest.h>
#include <benchmark/benchmark.h>

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        WarnLog("Check arguments on tests, separate for GoogleTest and Benchmark");
    }

    return RUN_ALL_TESTS();
}
