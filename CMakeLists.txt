cmake_minimum_required(VERSION 3.21)

set(CMAKE_AUTOMOC ON)

project(fxpresser)

find_package(Qt6 COMPONENTS Core Gui Widgets REQUIRED)

add_executable(fxpresser WIN32 fxmainwindow.cpp main.cpp icon.rc)

# 通过链接器嵌入清单，请求管理员权限(UAC)，避免链接器默认 asInvoker 清单覆盖
target_link_options(fxpresser PRIVATE
    "/MANIFEST:EMBED"
    "/MANIFESTUAC:NO"
    "/MANIFESTINPUT:${CMAKE_CURRENT_SOURCE_DIR}/fxpresser.manifest"
)

target_link_libraries(fxpresser Qt6::Core Qt6::Gui Qt6::Widgets)
