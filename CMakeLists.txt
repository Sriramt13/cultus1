cmake_minimum_required(VERSION 3.14)
project(cultus_allocator CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /W3)
else()
    add_compile_options(-O3 -Wall -Wextra -pthread)
    link_libraries(-pthread)
endif()

file(GLOB CORE_SRC src/common.cpp src/arena.cpp src/thread_cache.cpp src/allocator.cpp)

add_library(cultus STATIC ${CORE_SRC})
target_include_directories(cultus PUBLIC include src)

add_executable(test_correctness tests/test_correctness.cpp)
target_link_libraries(test_correctness cultus)

add_executable(benchmark benchmarks/benchmark.cpp)
target_link_libraries(benchmark cultus)
