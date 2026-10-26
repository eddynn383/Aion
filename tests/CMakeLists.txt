find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(aion_tests
    core/VersionTests.cpp
    core/ToleranceTests.cpp
    core/FuzzyMatchTests.cpp
    core/CameraTests.cpp
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
    core/IoTests.cpp
    core/QuantitiesTests.cpp
)

target_link_libraries(aion_tests PRIVATE aion::core Catch2::Catch2WithMain)

aion_configure_target(aion_tests)

# În WebAssembly (Node), testele au nevoie de sistemul de fișiere real: Catch2 scrie lista testelor
# într-un fișier temporar la descoperire.
if(EMSCRIPTEN)
    target_link_options(aion_tests PRIVATE -sNODERAWFS=1)
endif()

# Testele sunt descoperite la rularea ctest, nu la build, ca un test blocat să nu blocheze build-ul.
catch_discover_tests(aion_tests DISCOVERY_MODE PRE_TEST)
