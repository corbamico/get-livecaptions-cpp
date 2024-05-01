cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(get-livecaptions)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_executable(get-livecaptions src/main.cpp src/pch.cpp)
target_precompile_headers(get-livecaptions PRIVATE src/pch.h)

