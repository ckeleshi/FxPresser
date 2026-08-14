cmake_minimum_required(VERSION 3.21)

set(CMAKE_AUTOMOC ON)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(fxpresser)

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(fxpresser WIN32 fxmainwindow.cpp main.cpp fxpresser.rc)

target_link_libraries(fxpresser Qt5::Core Qt5::Gui Qt5::Widgets)

set_target_properties(fxpresser PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"" )
