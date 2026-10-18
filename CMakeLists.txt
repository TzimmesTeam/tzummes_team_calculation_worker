cmake_minimum_required(VERSION 3.16)

project(worker)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_library(worker_lib
        src/algorithms.cpp
)

target_include_directories(worker_lib PUBLIC
        src
)

add_executable(worker
        main.cpp
)

target_link_libraries(worker
        worker_lib
)

include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(worker_tests
        tests/test.cpp
)

target_link_libraries(worker_tests
        worker_lib
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(worker_tests)