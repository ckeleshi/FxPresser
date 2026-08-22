cmake_minimum_required(VERSION 3.21)

project(fxpresser)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_subdirectory(src/app_proj)
add_subdirectory(src/lib_proj)
