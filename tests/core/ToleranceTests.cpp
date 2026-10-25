#include "forma/core/math/Tolerance.h"

#include <catch2/catch_test_macros.hpp>

using forma::tol::Vec3d;

TEST_CASE("Valorile din toleranță sunt considerate egale", "[core][math][tolerance]")
{
    CHECK(forma::tol::equal(10.0, 10.0005));
    CHECK_FALSE(forma::tol::equal(10.0, 10.01));
    CHECK(forma::tol::isZero(-0.0009));
    CHECK_FALSE(forma::tol::isZero(0.002));
}

TEST_CASE("Puncte coincidente în toleranță", "[core][math][tolerance]")
{
    const Vec3d a { 1000.0, 2000.0, 0.0 };

    CHECK(forma::tol::samePoint(a, a + Vec3d { 0.0005, 0.0, 0.0 }));
    CHECK_FALSE(forma::tol::samePoint(a, a + Vec3d { 0.0, 0.01, 0.0 }));
}

TEST_CASE("Paralelism și perpendicularitate", "[core][math][tolerance]")
{
    const Vec3d x { 1.0, 0.0, 0.0 };
    const Vec3d y { 0.0, 1.0, 0.0 };

    CHECK(forma::tol::parallel(x, -x));
    CHECK_FALSE(forma::tol::parallel(x, y));
    CHECK(forma::tol::perpendicular(x, y));
    CHECK_FALSE(forma::tol::perpendicular(x, glm::normalize(x + y)));
}
