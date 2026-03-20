cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(get-livecaptions)

include(FetchContent)
FetchContent_Declare(CPM GIT_REPOSITORY https://github.com/cpm-cmake/CPM.cmake.git GIT_TAG v0.42.0)
FetchContent_MakeAvailable(CPM)
CPMAddPackage("gh:argosopentech/LibreTranslate-cpp#main")

# Patch LibreTranslate.cpp to add a 10-second CURL operation timeout.
# Without this the synchronous curl_easy_perform call can stall the ASIO event
# loop for up to the system default (~300s) when LibreTranslate is unreachable.
set(_lt_src "${LibreTranslate-cpp_SOURCE_DIR}/LibreTranslate.cpp")
file(READ "${_lt_src}" _lt_content)
if(NOT _lt_content MATCHES "CURLOPT_TIMEOUT")
    string(REPLACE
        "curl_easy_setopt(curl, CURLOPT_POSTFIELDS"
        "curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);\n    curl_easy_setopt(curl, CURLOPT_POSTFIELDS"
        _lt_content "${_lt_content}")
    file(WRITE "${_lt_src}" "${_lt_content}")
    message(STATUS "LibreTranslate: patched with 10s CURL operation timeout")
endif()

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
