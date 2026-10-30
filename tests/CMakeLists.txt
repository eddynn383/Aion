find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(aion_tests
    core/VersionTests.cpp
    core/ToleranceTests.cpp
    core/FuzzyMatchTests.cpp
    core/CameraTests.cpp
    core/CameraLensTests.cpp
    core/MathTests.cpp
    core/SlotMapTests.cpp
    core/MeshTests.cpp
    core/CommandStackTests.cpp
    core/GeometryTests.cpp
    core/GeometryFuzzTests.cpp
    core/ToolTests.cpp
    core/LengthTests.cpp
    core/InferenceTests.cpp
    core/PushPullTests.cpp
    core/TransformTests.cpp
    core/OffsetTests.cpp
    core/ModelTests.cpp
    core/ModelingTests.cpp
    core/SnapSceneTests.cpp
    core/ShapeTests.cpp
    core/BezierTests.cpp
    core/PenCurveTests.cpp
    core/PenToolTests.cpp
    core/MotionPathTests.cpp
    core/BevelTests.cpp
    core/SubdivideTests.cpp
    core/SweepTests.cpp
    core/PickIndexTests.cpp
    core/ModelScaleTests.cpp
    core/CopyingTests.cpp
    core/AnimationPlayerTests.cpp
    core/TexturingTests.cpp
    core/ParametricTests.cpp
    core/FurnitureTests.cpp
    core/CarcassTests.cpp
    core/IoTests.cpp
    core/RecoveryTests.cpp
    core/QuantitiesTests.cpp
    core/InteractionTests.cpp
    core/PrimitiveTests.cpp
    core/AnimationTests.cpp
    core/RuleTests.cpp
)

target_link_libraries(aion_tests PRIVATE aion::core Catch2::Catch2WithMain)

aion_configure_target(aion_tests)

# În WebAssembly (Node), testele au nevoie de sistemul de fișiere real: Catch2 scrie lista testelor
# într-un fișier temporar la descoperire.
if(EMSCRIPTEN)
    target_link_options(aion_tests PRIVATE -sNODERAWFS=1)
endif()

if(WIN32)
    # Pe Windows, numele testelor (cu diacritice) ajung la Catch2 prin codepage-ul consolei și nu se mai potrivesc
    # ca filtre; rulăm toate testele într-un singur proces.
    add_test(NAME aion_tests COMMAND aion_tests)
else()
    # Testele sunt descoperite la rularea ctest, nu la build, ca un test blocat să nu blocheze build-ul.
    catch_discover_tests(aion_tests DISCOVERY_MODE PRE_TEST)
endif()
