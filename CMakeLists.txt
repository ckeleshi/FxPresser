cmake_minimum_required(VERSION 3.7.0)

set(CMAKE_AUTOMOC ON)

project(fxpresser)

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(fxpresser WIN32 fxmainwindow.cpp main.cpp icon.rc)

target_link_libraries(fxpresser Qt5::Core Qt5::Gui Qt5::Widgets)
