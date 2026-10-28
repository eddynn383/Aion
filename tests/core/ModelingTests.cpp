#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/model/Modeling.h"
#include "aion/core/model/Operations.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <numbers>

using namespace aion;
using Catch::Approx;
using math::Transform;
using math::Vec3d;
using model::Model;

namespace {

// O cutie ca obiect (instanță), cu colțul minim în `corner`.
model::InstanceId boxObject(Model& m, Vec3d corner, Vec3d size)
{
    model::Definition def { "Cutie", true };
    def.shape = "box";
    const model::DefinitionId d = m.addDefinition(def);
    geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, size, 24);
    const auto [lo, hi] = model::bounds(m, d);
    model::Instance inst;
    inst.parent = m.root();
    inst.definition = d;
    inst.transform = Transform::translation(corner - lo);
    inst.name = "Cutie";
    return m.addInstance(inst);
}

geom::FaceId faceAt(const geom::Mesh& mesh, Vec3d p)
{
    for (geom::FaceId f : mesh.faces().keys()) {
        if (geom::faceContainsPoint(mesh, f, p)) {
            return f;
        }
    }
    return {};
}

} // namespace

TEST_CASE("Mirror: un obiect oglindit își păstrează cutia, iar geometria liberă rămâne cu fețele spre exterior", "[core][modeling]")
{
    Model m;
    const model::InstanceId box = boxObject(m, { 1000, 0, 0 }, { 400, 200, 300 });
    geom::Selection sel;
    sel.instances.insert(box);
    const auto [lo, hi] = model::selectionBounds(m, m.root(), sel);
    CHECK(glm::length(lo - Vec3d { 1000, 0, 0 }) < 1e-6);

    // Față de propriul centru: cutia rămâne pe loc, transformarea are determinant negativ.
    model::mirrorSelection(m, m.root(), sel, 0, (lo + hi) * 0.5);
    const auto [lo2, hi2] = model::selectionBounds(m, m.root(), sel);
    CHECK(glm::length(lo2 - lo) < 1e-6);
    CHECK(glm::length(hi2 - hi) < 1e-6);
    CHECK(m.instance(box).transform.matrix()[0].x < 0.0);

    // Față de planul X = 0: obiectul trece pe partea cealaltă.
    model::mirrorSelection(m, m.root(), sel, 0, { 0, 0, 0 });
    const auto [lo3, hi3] = model::selectionBounds(m, m.root(), sel);
    CHECK(lo3.x == Approx(-1400.0));
    CHECK(hi3.x == Approx(-1000.0));

    // Geometrie liberă: o cutie în rădăcină, oglindită pe Z; fețele au tot normalele spre exterior.
    geom::Mesh& root = m.mesh(m.root());
    geom::drawPolygon(root, { { 0, 0, 0 }, { 500, 0, 0 }, { 500, 500, 0 }, { 0, 500, 0 } });
    geom::pushPull(root, faceAt(root, { 250, 250, 0 }), -500);
    geom::Selection loose;
    for (geom::FaceId f : root.faces().keys()) {
        loose.faces.insert(f);
    }
    for (geom::EdgeId e : root.edges().keys()) {
        loose.edges.insert(e);
    }
    model::mirrorSelection(m, m.root(), loose, 2, { 0, 0, 0 });
    const geom::FaceId bottom = faceAt(root, { 250, 250, -500 });
    const geom::FaceId top = faceAt(root, { 250, 250, 0 });
    REQUIRE(bottom);
    REQUIRE(top);
    CHECK(root.face(bottom).plane.normal.z == Approx(-1.0));
    CHECK(root.face(top).plane.normal.z == Approx(1.0));
    CHECK(root.validate().ok());
    CHECK(m.validate().ok());
}

TEST_CASE("Array liniar și polar: copii la distanțe și unghiuri egale", "[core][modeling]")
{
    Model m;
    const model::InstanceId box = boxObject(m, { 1000, -100, 0 }, { 200, 200, 200 });
    geom::Selection sel;
    sel.instances.insert(box);

    const geom::Selection linear = model::linearArray(m, m.root(), sel, { 0, 500, 0 }, 3);
    REQUIRE(linear.instances.size() == 3);
    double maxY = -1e9;
    for (model::InstanceId id : linear.instances) {
        // Copia unui grup e independentă (ca în SketchUp); a unei componente ar fi legată.
        CHECK(m.definition(m.instance(id).definition).shape == "box");
        maxY = std::max(maxY, model::placementOf(m, id).position.y);
    }
    CHECK(maxY == Approx(0.0 + 1500.0)); // poziția e a pivotului: centrul cutiei (y = 0)

    // Polar: 4 copii la 90° în jurul axei Z prin origine, rotite.
    const geom::Selection polar = model::polarArray(m, m.root(), sel, { 0, 0, 0 }, { 0, 0, 1 }, std::numbers::pi / 2, 3, true);
    REQUIRE(polar.instances.size() == 3);
    geom::Selection all = polar;
    all.instances.insert(box);
    const auto [lo, hi] = model::selectionBounds(m, m.root(), all);
    CHECK(lo.x == Approx(-1200.0));
    CHECK(hi.x == Approx(1200.0));
    CHECK(lo.y == Approx(-1200.0));
    CHECK(hi.y == Approx(1200.0));

    // Fără rotire: copiile au aceeași orientare ca originalul.
    const geom::Selection moved = model::polarArray(m, m.root(), sel, { 0, 0, 0 }, { 0, 0, 1 }, std::numbers::pi, 1, false);
    REQUIRE(moved.instances.size() == 1);
    CHECK(model::placementOf(m, *moved.instances.begin()).rotation.z == Approx(0.0).margin(1e-9));
    CHECK(m.validate().ok());
}

TEST_CASE("Soften / Smooth: muchiile sub unghiul ales devin soft", "[core][modeling]")
{
    Model m;
    geom::Mesh& mesh = m.mesh(m.root());
    geom::buildPrimitive(mesh, geom::Primitive::Cylinder, { 1000, 1000, 1000 }, 24);
    std::set<geom::EdgeId> edges(mesh.edges().keys().begin(), mesh.edges().keys().end());

    // 24 de laturi → 15° între fețele laterale; capacele fac 90° cu ele.
    const int sides = model::softenEdges(mesh, edges, 20.0, false);
    CHECK(sides == 24);
    const int none = model::softenEdges(mesh, edges, 10.0, false);
    CHECK(none == 0);
    const int all = model::softenEdges(mesh, edges, 95.0, false);
    CHECK(all == static_cast<int>(edges.size()));
    CHECK(mesh.validate().ok());
}
