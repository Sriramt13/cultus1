cmake_minimum_required(VERSION 3.16)
project(cultus_skiplist CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/O2 /W3 /EHsc)
else()
    add_compile_options(-O3 -Wall -Wextra -Wpedantic)
endif()

add_executable(test_correctness tests/test_correctness.cpp)
target_include_directories(test_correctness PRIVATE include)
target_link_libraries(test_correctness PRIVATE Threads::Threads)

add_executable(benchmark benchmarks/benchmark.cpp)
target_include_directories(benchmark PRIVATE include)
target_link_libraries(benchmark PRIVATE Threads::Threads)
