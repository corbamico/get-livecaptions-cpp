cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

project(get-livecaptions)
add_executable(get-livecaptions src/main.cpp src/pch.cpp)
target_precompile_headers(get-livecaptions PRIVATE src/pch.h)

find_package(asio CONFIG REQUIRED)
find_package(wil CONFIG REQUIRED)
target_link_libraries(get-livecaptions PRIVATE WIL::WIL asio::asio)

#if you use lastest cppwinrt from vcpkg, uncomment following
#find_package(cppwinrt CONFIG REQUIRED)
#target_link_libraries(get-livecaptions PRIVATE WIL::WIL asio::asio Microsoft::CppWinRT RuntimeObject.lib)
