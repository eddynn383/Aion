find_package(glm CONFIG REQUIRED)

add_library(forma_core STATIC
    src/Version.cpp
    src/geom/Mesh.cpp
    src/geom/MeshValidate.cpp
    src/math/Intersect.cpp
    src/math/Plane.cpp
    src/math/Transform.cpp
    src/text/FuzzyMatch.cpp
    src/view/Camera.cpp
    src/view/CameraMotion.cpp
)
add_library(forma::core ALIAS forma_core)

target_include_directories(forma_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

configure_file(src/VersionConfig.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/VersionConfig.h" @ONLY)
target_include_directories(forma_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(forma_core PUBLIC glm::glm)

forma_configure_target(forma_core)
