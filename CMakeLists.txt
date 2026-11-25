cmake_minimum_required(VERSION 3.16)
project(worker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

add_library(worker_lib
    src/calc.cpp
    src/rates.cpp
    src/json.cpp
    src/file_rate_source.cpp
)
target_include_directories(worker_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(worker_lib PUBLIC Threads::Threads)

add_executable(worker src/main.cpp)
target_link_libraries(worker PRIVATE worker_lib)

if(WIN32 AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests)
    add_custom_command(TARGET worker POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:worker> ${CMAKE_CURRENT_SOURCE_DIR}/tests/
    )
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/test.cpp)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    add_executable(worker_tests tests/test.cpp)
    target_link_libraries(worker_tests PRIVATE worker_lib GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(worker_tests)
endif()