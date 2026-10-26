cmake_minimum_required(VERSION 3.25)

project(Aion
    VERSION 0.1.0
    DESCRIPTION "Modelator 3D modern"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AionOptions)

# Core-ul nu depinde de Qt: se poate compila singur (teste, WebAssembly).
add_subdirectory(core)

if(AION_BUILD_APP)
    find_package(Qt6 6.9 REQUIRED COMPONENTS Gui GuiPrivate Quick QuickControls2 ShaderTools Svg)
    qt_standard_project_setup(REQUIRES 6.9)
    add_subdirectory(render)
    add_subdirectory(app)
endif()

if(AION_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
