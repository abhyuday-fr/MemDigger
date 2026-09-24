cmake_minimum_required(VERSION 3.20)
project(MemDigger CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# === ENGINE:BEGIN (auto-generated — do not edit between markers, changes will be overwritten) ===
add_executable(MemDigger src/main.cpp)
target_compile_options(MemDigger PRIVATE -O3)
set_property(TARGET MemDigger PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)
# === ENGINE:END ===


# Anything below this line is yours — engine will never touch it.
target_sources(MemDigger PRIVATE src/mem_bench.cpp)

find_package(Threads REQUIRED)
target_link_libraries(MemDigger PRIVATE Threads::Threads)
