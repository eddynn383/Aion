find_package(Catch2 3 CONFIG REQUIRED)
include(Catch)

add_executable(forma_tests
    core/VersionTests.cpp
    core/ToleranceTests.cpp
)

target_link_libraries(forma_tests PRIVATE forma::core Catch2::Catch2WithMain)

forma_configure_target(forma_tests)

catch_discover_tests(forma_tests)
