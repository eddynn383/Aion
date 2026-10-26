find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(forma_tests
    core/VersionTests.cpp
    core/ToleranceTests.cpp
    core/FuzzyMatchTests.cpp
    core/CameraTests.cpp
    core/MathTests.cpp
)

target_link_libraries(forma_tests PRIVATE forma::core Catch2::Catch2WithMain)

forma_configure_target(forma_tests)

# Testele sunt descoperite la rularea ctest, nu la build, ca un test blocat să nu blocheze build-ul.
catch_discover_tests(forma_tests DISCOVERY_MODE PRE_TEST)
