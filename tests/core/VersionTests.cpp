#include "aion/core/Version.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Versiunea din core corespunde cu cea din string", "[core][version]")
{
    const auto v = aion::version();
    const std::string expected
        = std::to_string(v.major) + "." + std::to_string(v.minor) + "." + std::to_string(v.patch);

    REQUIRE(aion::versionString() == expected);
}
