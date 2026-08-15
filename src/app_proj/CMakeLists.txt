cmake_minimum_required(VERSION 3.21)

set(CMAKE_AUTOMOC ON)

project(fxpresser-app)

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(fxpresser-app WIN32 fxmainwindow.cpp main.cpp fxpresser.rc)

target_link_libraries(fxpresser-app Qt5::Core Qt5::Gui Qt5::Widgets)

set_target_properties(fxpresser-app PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\"" )
set_target_properties(fxpresser-app PROPERTIES OUTPUT_NAME "fxpresser")
