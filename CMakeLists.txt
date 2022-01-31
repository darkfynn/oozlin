cmake_minimum_required(VERSION 3.12)

# Set the project name and version
project(oozlin VERSION 1.0)
find_package(Threads REQUIRED)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Build linoodle
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(linoodle)
set(BUILD_SHARED_LIBS ON)

# Build oozlin
add_executable(oozlin bitknit.cpp huff.cpp kraken.cpp kraken_bits leviathan.cpp lzna.cpp main.cpp stdafx.cpp utilities.cp)
