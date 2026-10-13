cmake_minimum_required(VERSION 3.20)
project(AutoIP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(SOURCES
    main.cpp
    mainwindow.cpp
    pingworker.cpp
)

set(HEADERS
    mainwindow.h
    pingworker.h
)

set(RESOURCES
    autoip.qrc
)

qt_add_resources(RESOURCE_FILES ${RESOURCES})

add_executable(AutoIP WIN32
    ${SOURCES}
    ${HEADERS}
    ${RESOURCE_FILES}
    autoip.rc
)

target_link_libraries(AutoIP PRIVATE
    Qt6::Widgets
    iphlpapi
    ws2_32
)

set_target_properties(AutoIP PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "AutoIP"
    LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
)
