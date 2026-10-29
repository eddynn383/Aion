#include "aion/core/geom/Bezier.h"
#include "aion/core/math/Tolerance.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace aion;
using math::Vec3d;

namespace {

geom::BezierPoint corner(Vec3d p)
{
    return { p, p, p, false };
}

geom::BezierPath straight()
{
    return { { corner({ 0, 0, 0 }), corner({ 1000, 0, 0 }) }, false };
}

// Un sfert de cerc cu raza 1000, aproximat cu un singur segment cubic (k = 0.55228 · r).
geom::BezierPath quarter()
{
    constexpr double k = 552.28;
    geom::BezierPoint a { { 1000, 0, 0 }, { 1000, 0, 0 }, { 1000, k, 0 }, false };
    geom::BezierPoint b { { 0, 1000, 0 }, { k, 1000, 0 }, { 0, 1000, 0 }, false };
    return { { a, b }, false };
}

bool finiteUnit(const Vec3d& v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) && std::abs(glm::length(v) - 1.0) < 1e-9;
}

} // namespace

TEST_CASE("Bézier: un segment drept fără mânere", "[core][bezier]")
{
    const geom::BezierPath p = straight();
    CHECK(p.segmentCount() == 1);
    CHECK(geom::sampleBezier(p, 1.0).size() == 2);
    const geom::ArcLength arc(p);
    CHECK(std::abs(arc.length() - 1000.0) < 1e-6);
    CHECK(tol::samePoint(arc.at(500).point, { 500, 0, 0 }));
    CHECK(tol::samePoint(arc.at(500).tangent, { 1, 0, 0 }));
}

TEST_CASE("Bézier: un sfert de cerc aproximat are lungimea corectă", "[core][bezier]")
{
    const geom::BezierPath p = quarter();
    const double expected = glm::pi<double>() * 1000.0 / 2.0;
    CHECK(std::abs(geom::ArcLength(p).length() - expected) < expected * 0.005);
    const std::vector<Vec3d> samples = geom::sampleBezier(p, 1.0);
    CHECK(samples.size() > 4);
    for (const Vec3d& s : samples) {
        CHECK(std::abs(glm::length(s) - 1000.0) <= 1.2);
    }
    // Fără puncte duplicate.
    for (std::size_t i = 1; i < samples.size(); ++i) {
        CHECK(glm::length(samples[i] - samples[i - 1]) > tol::kLength);
    }
}

TEST_CASE("Bézier: viteza după lungime e uniformă", "[core][bezier]")
{
    const geom::ArcLength arc(quarter());
    const double step = arc.length() / 10.0;
    for (int i = 0; i < 10; ++i) {
        const double d = glm::length(arc.at(step * (i + 1)).point - arc.at(step * i).point);
        CHECK(std::abs(d - step) < step * 0.01);
    }
}

TEST_CASE("Bézier: prelungirea capetelor și calea închisă", "[core][bezier]")
{
    const geom::ArcLength arc(straight());
    CHECK(tol::samePoint(arc.at(-200).point, { -200, 0, 0 }));
    CHECK(tol::samePoint(arc.at(1300).point, { 1300, 0, 0 }));
    CHECK(tol::samePoint(arc.at(1300).tangent, { 1, 0, 0 }));

    const geom::BezierPath square { { corner({ 0, 0, 0 }), corner({ 1000, 0, 0 }), corner({ 1000, 1000, 0 }),
                                        corner({ 0, 1000, 0 }) },
        true };
    CHECK(square.segmentCount() == 4);
    const geom::ArcLength loop(square);
    CHECK(std::abs(loop.length() - 4000.0) < 1e-6);
    CHECK(tol::samePoint(loop.at(loop.length() + 100).point, loop.at(100).point));
    CHECK(tol::samePoint(loop.at(-100).point, { 0, 100, 0 }));
    // Pe o cale închisă, primul punct nu se repetă la final.
    CHECK(geom::sampleBezier(square, 1.0).size() == 4);
}

TEST_CASE("Bézier: inserarea nu schimbă forma", "[core][bezier]")
{
    const geom::BezierPath p = quarter();
    const geom::BezierPath q = geom::insertPoint(p, 0, 0.3);
    REQUIRE(q.points.size() == 3);
    CHECK(q.points[1].smooth);
    CHECK(tol::samePoint(q.points[1].point, geom::bezierPoint(p, 0, 0.3)));
    // Fiecare punct al curbei vechi e pe cea nouă: t < 0.3 pe primul segment, restul pe al doilea.
    for (int i = 0; i <= 20; ++i) {
        const double t = i / 20.0;
        const Vec3d expected = geom::bezierPoint(p, 0, t);
        const Vec3d actual = t <= 0.3 ? geom::bezierPoint(q, 0, t / 0.3) : geom::bezierPoint(q, 1, (t - 0.3) / 0.7);
        CHECK(glm::length(actual - expected) < tol::kLength);
    }
    const geom::BezierPath back = geom::removePoint(q, 1);
    CHECK(back.points.size() == 2);
    CHECK(tol::samePoint(back.points[0].point, p.points[0].point));
    CHECK(tol::samePoint(back.points[1].point, p.points[1].point));
}

TEST_CASE("Bézier: tangentă finită pe curbe degenerate", "[core][bezier]")
{
    // Două puncte identice: nicio direcție → (1, 0, 0).
    const geom::BezierPath same { { corner({ 5, 5, 0 }), corner({ 5, 5, 0 }) }, false };
    CHECK(finiteUnit(geom::bezierTangent(same, 0, 0.0)));
    CHECK(finiteUnit(geom::bezierTangent(same, 0, 0.5)));
    CHECK(finiteUnit(geom::ArcLength(same).at(0.0).tangent));
    CHECK(finiteUnit(geom::ArcLength(same).at(10.0).tangent));

    // Mânere de lungime 0 la capete: derivata e 0 la t = 0 și 1; tangenta cade pe coardă.
    const geom::BezierPath flat = straight();
    CHECK(tol::samePoint(geom::bezierTangent(flat, 0, 0.0), { 1, 0, 0 }));
    CHECK(tol::samePoint(geom::bezierTangent(flat, 0, 1.0), { 1, 0, 0 }));

    // Un punct neted cu mânerul pe el (încă netras).
    geom::BezierPoint smooth = corner({ 1000, 0, 0 });
    smooth.smooth = true;
    const geom::BezierPath p { { corner({ 0, 0, 0 }), smooth, corner({ 1000, 1000, 0 }) }, false };
    const geom::ArcLength arc(p);
    for (int i = 0; i <= 10; ++i) {
        CHECK(finiteUnit(arc.at(arc.length() * i / 10.0).tangent));
    }
}

TEST_CASE("Bézier: transformarea mută punctele și mânerele", "[core][bezier]")
{
    const geom::BezierPath p = geom::transformed(quarter(), math::Transform::translation({ 0, 0, 500 }));
    CHECK(tol::samePoint(p.points[0].point, { 1000, 0, 500 }));
    CHECK(tol::samePoint(p.points[0].out, { 1000, 552.28, 500 }));
    CHECK(tol::samePoint(p.points[1].in, { 552.28, 1000, 500 }));
}

TEST_CASE("Bézier: prelungirea capetelor sare peste segmentele de lungime 0", "[core][bezier]")
{
    // Două click-uri pe același punct, la început și la sfârșit: direcția prelungirii vine din segmentul real.
    const geom::BezierPath p { { corner({ 0, 0, 0 }), corner({ 0, 0, 0 }), corner({ 0, 1000, 0 }), corner({ 0, 1000, 0 }) },
        false };
    const geom::ArcLength arc(p);
    CHECK(tol::samePoint(arc.at(-100).point, { 0, -100, 0 }));
    CHECK(tol::samePoint(arc.at(-100).tangent, { 0, 1, 0 }));
    CHECK(tol::samePoint(arc.at(arc.length() + 100).point, { 0, 1100, 0 }));
    CHECK(tol::samePoint(arc.at(arc.length() + 100).tangent, { 0, 1, 0 }));
}

TEST_CASE("Bézier: la un colț, tangenta cu tranziție se întoarce treptat", "[core][bezier]")
{
    const geom::BezierPath l { { corner({ 0, 0, 0 }), corner({ 5000, 0, 0 }), corner({ 5000, 5000, 0 }) }, false };
    const geom::ArcLength arc(l);
    const double blend = 1000.0;
    // Departe de colț: tangenta exactă.
    CHECK(tol::samePoint(arc.at(2000, blend).tangent, { 1, 0, 0 }));
    CHECK(tol::samePoint(arc.at(8000, blend).tangent, { 0, 1, 0 }));
    // Pe lângă colț: fără salt (cel mult câteva grade la fiecare 10 mm), de la 0° la 90°.
    double previous = 0.0;
    for (double d = 4400.0; d <= 5600.0; d += 10.0) {
        const math::Vec3d t = arc.at(d, blend).tangent;
        CHECK(std::abs(glm::length(t) - 1.0) < 1e-9);
        const double angle = std::atan2(t.y, t.x) * 180.0 / glm::pi<double>();
        CHECK(angle >= previous - 1e-9);
        CHECK(angle - previous < 3.0);
        previous = angle;
    }
    CHECK(std::abs(previous - 90.0) < 1e-6);
    // Fără tranziție: saltul de la colț rămâne (comportamentul de până acum).
    CHECK(tol::samePoint(arc.at(5001).tangent, { 0, 1, 0 }));
}
