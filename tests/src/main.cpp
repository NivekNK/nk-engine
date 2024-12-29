#include <gtest/gtest.h>
#include <benchmark/benchmark.h>
#include <iostream>

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
        std::cout << "Check arguments on tests, separate for GoogleTest and Benchmark" << std::endl;
    }

    return RUN_ALL_TESTS();
}
