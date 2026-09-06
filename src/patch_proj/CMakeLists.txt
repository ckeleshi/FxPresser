cmake_minimum_required(VERSION 4.2)

project(fxpresser-patch)

add_library(fxpresser-patch SHARED
    ../lib_proj/ffo.cpp
    ../lib_proj/dll.cpp
    ../lib_proj/dll.def
    ../lib_proj/hooking/byte_pattern.cpp
)

target_compile_definitions(fxpresser-patch PRIVATE FXPRESSER_PATCH_ONLY)

set_target_properties(fxpresser-patch PROPERTIES OUTPUT_NAME "opengl32")
