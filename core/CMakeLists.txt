find_package(glm CONFIG REQUIRED)
find_path(EARCUT_HPP_INCLUDE_DIRS "mapbox/earcut.hpp" REQUIRED)

add_library(aion_core STATIC
    src/Version.cpp
    src/commands/CommandStack.cpp
    src/geom/Mesh.cpp
    src/geom/MeshValidate.cpp
    src/geom/Offset.cpp
    src/geom/Operations.cpp
    src/geom/PlanarRegions.cpp
    src/geom/PushPull.cpp
    src/geom/Selection.cpp
    src/geom/Transformations.cpp
    src/geom/Triangulate.cpp
    src/math/Intersect.cpp
    src/math/Plane.cpp
    src/math/Transform.cpp
    src/text/FuzzyMatch.cpp
    src/view/Camera.cpp
    src/view/CameraMotion.cpp
    src/view/Picking.cpp
    src/inference/Inference.cpp
    src/units/Length.cpp
    src/tools/Tools.cpp
    src/tools/ToolHelpers.cpp
    src/tools/TransformTools.cpp
    src/tools/ShapeTools.cpp
)
add_library(aion::core ALIAS aion_core)

target_include_directories(aion_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

configure_file(src/VersionConfig.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/VersionConfig.h" @ONLY)
target_include_directories(aion_core PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")

target_link_libraries(aion_core PUBLIC glm::glm)
target_include_directories(aion_core PRIVATE ${EARCUT_HPP_INCLUDE_DIRS})

aion_configure_target(aion_core)
