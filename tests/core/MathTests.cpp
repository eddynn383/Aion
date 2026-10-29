#include "aion/core/math/Intersect.h"
#include "aion/core/math/Plane.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/math/Transform.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

using namespace aion::math;
using aion::tol::equal;
using aion::tol::samePoint;

TEST_CASE("Planul din trei puncte și din poligon", "[core][math][plane]")
{
    const auto p = Plane::fromPoints({ 0, 0, 5 }, { 10, 0, 5 }, { 0, 10, 5 });
    REQUIRE(p.has_value());
    CHECK(samePoint(p->normal, { 0, 0, 1 }));
    CHECK(equal(p->d, 5.0));
    CHECK(equal(p->signedDistance({ 3, 4, 8 }), 3.0));
    CHECK(samePoint(p->project({ 3, 4, 8 }), { 3, 4, 5 }));

    CHECK_FALSE(Plane::fromPoints({ 0, 0, 0 }, { 1, 1, 1 }, { 2, 2, 2 }).has_value());

    // Poligon concav (în formă de L), în sens orar privit de sus: normala în jos.
    const std::vector<Vec3d> l { { 0, 0, 0 }, { 0, 20, 0 }, { 10, 20, 0 }, { 10, 10, 0 }, { 20, 10, 0 }, { 20, 0, 0 } };
    const auto lp = Plane::fromPolygon(l);
    REQUIRE(lp.has_value());
    CHECK(samePoint(lp->normal, { 0, 0, -1 }));
}

TEST_CASE("Planuri coincidente indiferent de sensul normalei", "[core][math][plane]")
{
    const Plane a = Plane::fromPointNormal({ 0, 0, 100 }, { 0, 0, 1 });
    CHECK(a.coincident(a.flipped()));
    CHECK(a.coincident(Plane::fromPointNormal({ 50, -20, 100.0002 }, { 0, 0, 1 })));
    CHECK_FALSE(a.coincident(Plane::fromPointNormal({ 0, 0, 101 }, { 0, 0, 1 })));
    CHECK_FALSE(a.coincident(Plane::fromPointNormal({ 0, 0, 100 }, glm::normalize(Vec3d { 0, 0.01, 1 }))));
}

TEST_CASE("Reperul 2D al unui plan păstrează orientarea", "[core][math][plane]")
{
    const Plane plane = Plane::fromPointNormal({ 1, 2, 3 }, glm::normalize(Vec3d { 1, 1, 1 }));
    const PlaneFrame frame(plane);
    const Vec3d p = plane.project({ 7, -3, 10 });
    CHECK(samePoint(frame.toWorld(frame.toLocal(p)), p));

    // Un triunghi trigonometric privit dinspre normală are arie pozitivă în 2D.
    const auto tri = Plane::fromPoints({ 0, 0, 0 }, { 1, 0, 0 }, { 0, 1, 0 });
    REQUIRE(tri.has_value());
    const PlaneFrame f(*tri);
    const std::array<Vec2d, 3> local { f.toLocal({ 0, 0, 0 }), f.toLocal({ 1, 0, 0 }), f.toLocal({ 0, 1, 0 }) };
    CHECK(signedArea(local) > 0.0);
}

TEST_CASE("Cele mai apropiate puncte între segmente", "[core][math][intersect]")
{
    // Se intersectează în (5, 5, 0).
    const auto x = closestPoints({ { 0, 0, 0 }, { 10, 10, 0 } }, { { 0, 10, 0 }, { 10, 0, 0 } });
    CHECK(equal(x.distance, 0.0));
    CHECK(equal(x.s, 0.5, 1e-12));
    CHECK(samePoint(x.onFirst, { 5, 5, 0 }));

    // Încrucișate în spațiu, la 3 mm una de alta.
    const auto skew = closestPoints({ { 0, 0, 0 }, { 10, 0, 0 } }, { { 4, -5, 3 }, { 4, 5, 3 } });
    CHECK(equal(skew.distance, 3.0));
    CHECK(samePoint(skew.onFirst, { 4, 0, 0 }));

    // Paralele: distanța corectă, fără împărțire la zero.
    const auto par = closestPoints({ { 0, 0, 0 }, { 10, 0, 0 } }, { { 2, 1, 0 }, { 8, 1, 0 } });
    CHECK(equal(par.distance, 1.0));

    // Capete limitate la segmente.
    const auto apart = closestPoints({ { 0, 0, 0 }, { 1, 0, 0 } }, { { 5, 0, 0 }, { 6, 0, 0 } });
    CHECK(equal(apart.distance, 4.0));
    CHECK(equal(apart.s, 1.0));
    CHECK(equal(apart.t, 0.0));

    // Segment degenerat (punct).
    const auto dot = closestPoints({ { 3, 3, 3 }, { 3, 3, 3 } }, { { 0, 0, 0 }, { 10, 0, 0 } });
    CHECK(equal(dot.distance, std::sqrt(18.0)));
}

TEST_CASE("Punct proiectat pe segment", "[core][math][intersect]")
{
    const Segment s { { 0, 0, 0 }, { 10, 0, 0 } };
    const auto mid = projectOnSegment({ 4, 2, 0 }, s);
    CHECK(equal(mid.t, 0.4, 1e-12));
    CHECK(equal(mid.distance, 2.0));
    const auto before = projectOnSegment({ -3, 4, 0 }, s);
    CHECK(equal(before.t, 0.0));
    CHECK(equal(before.distance, 5.0));
}

TEST_CASE("Raza intersectează planul și triunghiul", "[core][math][intersect]")
{
    const Ray down { { 1, 1, 10 }, { 0, 0, -1 } };
    const auto t = intersectRayPlane(down, Plane::fromPointNormal({ 0, 0, 0 }, { 0, 0, 1 }));
    REQUIRE(t.has_value());
    CHECK(equal(*t, 10.0));
    CHECK_FALSE(intersectRayPlane({ { 0, 0, 10 }, { 1, 0, 0 } }, Plane {}).has_value());
    CHECK_FALSE(intersectRayPlane({ { 0, 0, 10 }, { 0, 0, 1 } }, Plane {}).has_value());

    const auto hit = intersectRayTriangle(down, { 0, 0, 0 }, { 5, 0, 0 }, { 0, 5, 0 });
    REQUIRE(hit.has_value());
    CHECK(equal(*hit, 10.0));
    // Și din spate (triunghiul în sens opus).
    CHECK(intersectRayTriangle(down, { 0, 0, 0 }, { 0, 5, 0 }, { 5, 0, 0 }).has_value());
    CHECK_FALSE(intersectRayTriangle({ { 4, 4, 10 }, { 0, 0, -1 } }, { 0, 0, 0 }, { 5, 0, 0 }, { 0, 5, 0 }).has_value());
}

TEST_CASE("O rază exact pe diagonala unui pătrat nu trece printre cele două triunghiuri", "[core][math][intersect]")
{
    // Raza din aplicație (vederea iso) spre (−1000, −500, 500): exact pe diagonala A–C a feței x = −1000 a unei cutii.
    const Ray ray { { -3325.5095234230421, -3325.5095234230412, 4325.5095234230412 },
        { 0.43927591820844347, 0.5337231595084595, -0.722617642108492 } };
    const Vec3d a { -1000, -1000, 0 };
    const Vec3d b { -1000, -1000, 2000 };
    const Vec3d c { -1000, 1000, 2000 };
    const Vec3d d { -1000, 1000, 0 };
    const auto first = intersectRayTriangle(ray, c, d, a);
    const auto second = intersectRayTriangle(ray, c, a, b);
    CHECK((first.has_value() || second.has_value()));
    // Dar nu „lipește” o rază care trece clar pe lângă.
    CHECK_FALSE(intersectRayTriangle({ { -2000, 1001, 500 }, { 1, 0, 0 } }, c, d, a).has_value());
}

TEST_CASE("Punct în poligon, inclusiv pe graniță și în poligoane concave", "[core][math][polygon]")
{
    const std::vector<Vec2d> l { { 0, 0 }, { 20, 0 }, { 20, 10 }, { 10, 10 }, { 10, 20 }, { 0, 20 } };
    CHECK(signedArea(l) > 0.0);
    CHECK(equal(signedArea(l), 300.0));
    CHECK(locatePoint({ 5, 5 }, l, 1e-3) == PointLocation::Inside);
    CHECK(locatePoint({ 15, 15 }, l, 1e-3) == PointLocation::Outside);
    CHECK(locatePoint({ 20, 5 }, l, 1e-3) == PointLocation::OnBoundary);
    CHECK(locatePoint({ 10, 15 }, l, 1e-3) == PointLocation::OnBoundary);
    CHECK(locatePoint({ 0, 0 }, l, 1e-3) == PointLocation::OnBoundary);
    // Raza orizontală trece exact printr-un vârf (cazul clasic care strică testul naiv).
    CHECK(locatePoint({ 5, 10 }, l, 1e-3) == PointLocation::Inside);
    CHECK(locatePoint({ -5, 10 }, l, 1e-3) == PointLocation::Outside);
}

TEST_CASE("Transformările se compun și se inversează", "[core][math][transform]")
{
    const Transform move = Transform::translation({ 100, 0, 0 });
    const Transform turn = Transform::rotation({ 0, 0, 0 }, { 0, 0, 1 }, glm::half_pi<double>());
    const Transform both = move * turn; // întâi rotație, apoi mutare

    CHECK(samePoint(both.applyToPoint({ 10, 0, 0 }), { 100, 10, 0 }));
    CHECK(samePoint(both.applyToVector({ 10, 0, 0 }), { 0, 10, 0 }));
    CHECK(samePoint(both.inverse().applyToPoint({ 100, 10, 0 }), { 10, 0, 0 }));
    CHECK((both * both.inverse()).isIdentity());
    CHECK(Transform {}.isIdentity());

    // Rotație în jurul unui pivot.
    const Transform pivoted = Transform::rotation({ 10, 10, 0 }, { 0, 0, 1 }, glm::pi<double>());
    CHECK(samePoint(pivoted.applyToPoint({ 20, 10, 0 }), { 0, 10, 0 }));

    // Normala unei fețe înclinate rămâne perpendiculară după o scalare neuniformă.
    const Transform squash = Transform::scaling({ 0, 0, 0 }, { 1, 1, 0.5 });
    const Vec3d n = glm::normalize(Vec3d { 0, 1, 1 }); // fața prin (0,1,0)-(0,0,1)
    const Vec3d edge = squash.applyToVector({ 0, -1, 1 });
    CHECK(aion::tol::isZero(glm::dot(squash.applyToNormal(n), edge), 1e-9));
    CHECK(equal(glm::length(squash.applyToNormal(n)), 1.0, 1e-12));
}
