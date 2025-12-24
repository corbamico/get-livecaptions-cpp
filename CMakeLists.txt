cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(get-livecaptions)

include(FetchContent)
FetchContent_Declare(CPM GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake.git GIT_TAG v0.42.0)
FetchContent_MakeAvailable(CPM)
CPMAddPackage("gh:argosopentech/LibreTranslate-cpp#main")

find_package(asio CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)
find_package(argparse CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)

add_executable(get-livecaptions src/main.cpp src/pch.cpp ${LibreTranslate-cpp_SOURCE_DIR}/LibreTranslate.cpp)
target_precompile_headers(get-livecaptions PRIVATE src/pch.h)
target_include_directories(get-livecaptions PRIVATE ${LibreTranslate-cpp_SOURCE_DIR})
target_link_libraries(get-livecaptions PRIVATE WIL::WIL asio::asio argparse::argparse windowsapp CURL::libcurl)

#find_package(cppwinrt CONFIG REQUIRED)
#target_link_libraries(get-livecaptions PRIVATE WIL::WIL asio::asio Microsoft::CppWinRT RuntimeObject.lib windowsapp)
