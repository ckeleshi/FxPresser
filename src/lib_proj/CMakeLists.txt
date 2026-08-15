cmake_minimum_required(VERSION 3.21)

project(fxpresser-plugin)

find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

add_library(fxpresser-plugin SHARED
    ffo.cpp
    dll.cpp
    dll.def
    helper/helper.cpp
    hooking/byte_pattern.cpp
)

target_link_libraries(fxpresser-plugin PRIVATE imgui::imgui)
target_link_libraries(fxpresser-plugin PRIVATE winmm opengl32)
target_link_libraries(fxpresser-plugin PRIVATE nlohmann_json::nlohmann_json)

set_target_properties(fxpresser-plugin PROPERTIES OUTPUT_NAME "opengl32")
