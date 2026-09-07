cmake_minimum_required(VERSION 3.16)
project(skiplist CXX)
set(CMAKE_CXX_STANDARD 20)
find_package(Threads REQUIRED)

if(MSVC)
    add_compile_options(/O2 /EHsc)
else()
    add_compile_options(-O3 -pthread)
endif()

add_executable(test test.cpp)
target_link_libraries(test Threads::Threads)

add_executable(bench bench.cpp)
target_link_libraries(bench Threads::Threads)
