#include "aion/core/model/Texturing.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

using namespace aion;
using Catch::Approx;
using math::Vec2d;
using math::Vec3d;

TEST_CASE("Texturi: UV la mărimea reală, pe pardoseli și pereți", "[core][textures]")
{
    const geom::TextureTransform none;
    const Vec2d size { 500, 250 }; // o cărămidă: 500 × 250 mm pe repetiție

    // Pardoseală: U = X, V = Y.
    const Vec2d floor = model::textureCoordinates({ 0, 0, 1 }, { 1000, 250, 0 }, none, size);
    CHECK(floor.x == Approx(2.0));
    CHECK(floor.y == Approx(1.0));

    // Perete spre −Y: U orizontal (pe X), V în sus (Z), deci textura stă „în picioare”.
    const auto [u, v] = model::textureAxes({ 0, -1, 0 });
    CHECK(std::abs(u.z) < 1e-12);
    CHECK(v.z == Approx(1.0));
    const Vec2d wall = model::textureCoordinates({ 0, -1, 0 }, { 0, 0, 500 }, none, size);
    CHECK(std::abs(wall.y) == Approx(2.0));

    // Deplasarea, rotația și scara feței.
    geom::TextureTransform t;
    t.offset = { 500, 0 };
    CHECK(model::textureCoordinates({ 0, 0, 1 }, { 500, 0, 0 }, t, size).x == Approx(0.0).margin(1e-12));
    t = {};
    t.scale = { 2, 2 };
    CHECK(model::textureCoordinates({ 0, 0, 1 }, { 1000, 0, 0 }, t, size).x == Approx(1.0));
    t = {};
    t.rotation = 90;
    const Vec2d rotated = model::textureCoordinates({ 0, 0, 1 }, { 500, 0, 0 }, t, size);
    CHECK(rotated.x == Approx(0.0).margin(1e-9));
    CHECK(std::abs(rotated.y) == Approx(2.0));
}
