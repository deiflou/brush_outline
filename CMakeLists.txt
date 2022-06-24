cmake_minimum_required(VERSION 3.1.0)

project(brush_outline)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt5 REQUIRED COMPONENTS Widgets)

add_executable(
    brush_outline
    main.cpp
    window.cpp
    window.h
    res.qrc
)

target_link_libraries(brush_outline PRIVATE Qt5::Widgets)

set_target_properties(
    brush_outline
    PROPERTIES
    AUTOMOC ON
    AUTORCC ON
)