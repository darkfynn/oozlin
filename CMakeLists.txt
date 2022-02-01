cmake_minimum_required(VERSION 3.12)

# Set the project name and version
project(oozlin VERSION 1.0 LANGUAGES CXX)
find_package(Threads REQUIRED)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Build linoodle
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(linoodle)
set(BUILD_SHARED_LIBS ON)

# Build oozlin
add_executable(oozlin main.cpp bitknit.cpp huff.cpp kraken.cpp kraken_bits.cpp mermaid.cpp leviathan.cpp lzna.cpp stdafx.cpp utilities.cpp)
target_link_libraries(oozlin -ldl)
