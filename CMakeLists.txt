cmake_minimum_required(VERSION 3.20)
project(LockFreeOrderBook VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_TLS_VERIFY OFF)

# Compiler flags for low latency
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -O3 -march=native -pthread")
elseif(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /O2 /Oi /GL /std:c++20")
endif()

# Include directories
include_directories(include)

# Add the main executable
add_executable(lfe_engine src/main.cpp)
target_include_directories(lfe_engine PRIVATE include)

# We can enable testing and benchmarks later if they exist
enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)
