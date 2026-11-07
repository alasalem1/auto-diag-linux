cmake_minimum_required(VERSION 3.16)

project(autodiag-gui VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

qt_standard_project_setup()

qt_add_executable(autodiag-gui
    main.cpp
)

target_link_libraries(autodiag-gui
    PRIVATE Qt6::Widgets
)

install(TARGETS autodiag-gui
    RUNTIME DESTINATION bin
)
