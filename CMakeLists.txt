cmake_minimum_required(VERSION 3.12)

# Set the project name and version
project(ooz-linux VERSION 1.0)
find_package(Threads REQUIRED)

# Specify the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Build linoodle
option(BUILD_SHARED_LIBS "" OFF)
add_subdirectory(linoodle)
set(BUILD_SHARED_LIBS ON)

# Build ooz-linux
add_executable(ooz-linux bitknit.cpp kraken.cpp lzna.cpp stdafx.cpp)
