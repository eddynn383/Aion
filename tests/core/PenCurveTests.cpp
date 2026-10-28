#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Bezier.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/io/AionFile.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/PenCurves.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <cmath>

using namespace aion;
using math::Vec3d;

namespace {

geom::BezierPoint corner(Vec3d p)
{
    return { p, p, p, false };
}

geom::BezierPoint smooth(Vec3d p, Vec3d out)
{
    return { p, 2.0 * p - out, out, true };
}

// Un S deschis pe sol: colț, punct neted cu tangenta orizontală, colț.
geom::BezierPath sCurve()
{
    return { { corner({ 0, 0, 0 }), smooth({ 1000, 1000, 0 }, { 1500, 1000, 0 }), corner({ 2000, 2000, 0 }) }, false };
}

// O formă închisă rotunjită în jurul lui `c`, cu raza r.
geom::BezierPath blob(Vec3d c, double r)
{
    const double k = 0.55228 * r;
    return { { smooth(c + Vec3d { r, 0, 0 }, c + Vec3d { r, k, 0 }), smooth(c + Vec3d { 0, r, 0 }, c + Vec3d { -k, r, 0 }),
                 smooth(c + Vec3d { -r, 0, 0 }, c + Vec3d { -r, -k, 0 }), smooth(c + Vec3d { 0, -r, 0 }, c + Vec3d { k, -r, 0 }) },
        true };
}

model::PenCurveId draw(commands::CommandStack& stack, const geom::BezierPath& path)
{
    model::PenCurveId id;
    stack.run("Pen", [&](model::Model& model) { id = model::addPenCurveGeometry(model, model.root(), path, { 0, 0, 1 }); });
    return id;
}

std::vector<geom::EdgeId> edgesOf(const geom::Mesh& mesh, std::uint32_t curve)
{
    std::vector<geom::EdgeId> out;
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        if (mesh.edges().values()[i].curve == curve) {
            out.push_back(mesh.edges().keys()[i]);
        }
    }
    return out;
}

} // namespace

TEST_CASE("Pen: o curbă deschisă pe sol dă muchii cu același curve id și rămâne editabilă", "[core][pen]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::PenCurveId id = draw(stack, sCurve());
    REQUIRE(m.penCurves().size() == 1);
    const model::PenCurve& c = m.penCurves().at(id);
    CHECK(c.definition == m.root());
    CHECK(c.curve != 0);
    const geom::Mesh& mesh = m.mesh(m.root());
    const std::vector<geom::EdgeId> edges = edgesOf(mesh, c.curve);
    CHECK(edges.size() == mesh.edges().size());
    CHECK(edges.size() > 4);
    CHECK(model::penCurveEditable(m, id));
    CHECK(model::penCurveForEdge(m, m.root(), edges.front()) == id);
    CHECK(model::penCurveForEdge(m, m.root(), edges.back()) == id);
}

TEST_CASE("Pen: o curbă închisă devine o față", "[core][pen]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::PenCurveId id = draw(stack, blob({ 0, 0, 0 }, 1000));
    const geom::Mesh& mesh = m.mesh(m.root());
    REQUIRE(mesh.faces().size() == 1);
    CHECK(std::abs(std::abs(mesh.faces().values()[0].plane.normal.z) - 1.0) < 1e-9);
    CHECK(model::penCurveEditable(m, id));
}

TEST_CASE("Pen: o curbă închisă pe o față-gazdă o împarte și rămâne editabilă", "[core][pen]")
{
    model::Model m;
    commands::CommandStack stack { m };
    stack.run("Rect", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, 0, 0 }, { 4000, 0, 0 }, { 4000, 3000, 0 }, { 0, 3000, 0 } });
    });
    const model::PenCurveId id = draw(stack, blob({ 2000, 1500, 0 }, 500));
    CHECK(m.mesh(m.root()).faces().size() == 2);
    CHECK(model::penCurveEditable(m, id));

    geom::BezierPath moved = m.penCurves().at(id).path;
    const Vec3d oldPoint = moved.points[0].point;
    moved.points[0].point += Vec3d { 200, 0, 0 };
    moved.points[0].in += Vec3d { 200, 0, 0 };
    moved.points[0].out += Vec3d { 200, 0, 0 };
    bool replaced = false;
    stack.run("Edit", [&](model::Model& model) { replaced = model::replacePenCurve(model, id, moved); });
    CHECK(replaced);
    const geom::Mesh& mesh = m.mesh(m.root());
    CHECK(mesh.faces().size() == 2);
    CHECK_FALSE(mesh.findVertexNear(oldPoint, tol::kLength));
    CHECK(mesh.findVertexNear(oldPoint + Vec3d { 200, 0, 0 }, tol::kLength));
    CHECK(m.penCurves().at(id).path == moved);
    CHECK(model::penCurveEditable(m, id));
}

TEST_CASE("Pen: după Push/Pull nu mai e editabilă", "[core][pen]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::PenCurveId id = draw(stack, blob({ 0, 0, 0 }, 1000));
    const geom::FaceId face = m.mesh(m.root()).faces().keys()[0];
    stack.run("Push", [&](geom::Mesh& mesh) { geom::pushPull(mesh, face, 500); });
    CHECK_FALSE(model::penCurveEditable(m, id));
    const std::size_t edges = m.mesh(m.root()).edges().size();
    bool replaced = true;
    stack.run("Edit", [&](model::Model& model) { replaced = model::replacePenCurve(model, id, blob({ 0, 0, 0 }, 800)); });
    CHECK_FALSE(replaced);
    CHECK(m.mesh(m.root()).edges().size() == edges);
}

TEST_CASE("Pen: un vârf mutat o face needitabilă, o copie nu", "[core][pen]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::PenCurveId id = draw(stack, sCurve());
    const std::uint32_t curve = m.penCurves().at(id).curve;
    const std::vector<Vec3d> samples = geom::sampleBezier(sCurve(), geom::defaultTolerance(sCurve()));

    SECTION("copia nu strică originalul")
    {
        std::vector<Vec3d> copy = samples;
        for (Vec3d& p : copy) {
            p += Vec3d { 0, 5000, 0 };
        }
        stack.run("Copy", [&](geom::Mesh& mesh) { geom::drawCurve(mesh, copy, false, true, curve); });
        CHECK(model::penCurveEditable(m, id));
    }
    SECTION("un vârf mutat")
    {
        const geom::VertexId v = m.mesh(m.root()).findVertexNear(samples[2], tol::kLength);
        REQUIRE(v);
        stack.run("Move", [&](geom::Mesh& mesh) { mesh.setPosition(v, samples[2] + Vec3d { 0, 0, 10 }); });
        CHECK_FALSE(model::penCurveEditable(m, id));
    }
    SECTION("mutată într-un grup")
    {
        geom::Selection all;
        for (geom::EdgeId e : edgesOf(m.mesh(m.root()), curve)) {
            all.edges.insert(e);
        }
        stack.run("Group", [&](model::Model& model) { model::makeGroup(model, model.root(), all, false); });
        CHECK_FALSE(model::penCurveEditable(m, id));
    }
}

TEST_CASE("Pen: undo / redo", "[core][pen]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::PenCurveId id = draw(stack, sCurve());
    stack.undo();
    CHECK(m.penCurves().empty());
    CHECK(m.mesh(m.root()).edges().empty());
    stack.redo();
    REQUIRE(m.penCurves().contains(id));
    CHECK(model::penCurveEditable(m, id));
}

TEST_CASE("Fișier: curbele Pen se salvează și se încarcă (v4)", "[core][pen][io]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::PenCurveId id = draw(stack, sCurve());
    const io::LoadResult loaded = io::fromJson(io::toJson(m));
    REQUIRE(loaded.model);
    REQUIRE(loaded.model->penCurves().contains(id));
    CHECK(loaded.model->penCurves().at(id) == m.penCurves().at(id));
    CHECK(model::penCurveEditable(*loaded.model, id));
    CHECK(io::kFormatVersion == 4);

    // Un fișier v3 (fără cheia penCurves) se încarcă fără curbe.
    std::string v3 = io::toJson(model::Model {});
    const std::string current = "\"formatVersion\": 4";
    const std::size_t at = v3.find(current);
    REQUIRE(at != std::string::npos);
    v3.replace(at, current.size(), "\"formatVersion\": 3");
    const std::size_t key = v3.find("\"penCurves\"");
    REQUIRE(key != std::string::npos);
    const io::LoadResult old = io::fromJson(v3);
    REQUIRE(old.model);
    CHECK(old.model->penCurves().empty());
}

TEST_CASE("Pen: o curbă nouă după gruparea alteia rămâne editabilă (id-ul curbei nu se refolosește)", "[core][pen]")
{
    model::Model m;
    commands::CommandStack stack { m };
    const model::PenCurveId first = draw(stack, blob({ 0, 0, 0 }, 1000));
    const auto groupAll = [&] {
        geom::Selection all;
        const geom::Mesh& mesh = m.mesh(m.root());
        all.faces.insert(mesh.faces().keys().begin(), mesh.faces().keys().end());
        all.edges.insert(mesh.edges().keys().begin(), mesh.edges().keys().end());
        stack.run("Group", [&](model::Model& model) { model::makeGroup(model, model.root(), all, false); });
    };
    groupAll(); // muchiile primei curbe pleacă din rădăcină: entitatea ei rămâne, needitabilă
    REQUIRE(m.mesh(m.root()).edges().empty());
    const model::PenCurveId second = draw(stack, sCurve());
    CHECK(m.penCurves().at(second).curve != m.penCurves().at(first).curve);
    CHECK(model::penCurveEditable(m, second));
    const geom::EdgeId edge = m.mesh(m.root()).edges().keys()[0];
    CHECK(model::penCurveForEdge(m, m.root(), edge) == second);

    // Un arc obișnuit desenat după ce și a doua curbă a plecat nu e luat drept curbă Pen.
    groupAll();
    stack.run("Arc", [&](geom::Mesh& mesh) {
        geom::drawCurve(mesh, { { 0, 0, 0 }, { 500, 300, 0 }, { 1000, 400, 0 }, { 1500, 300, 0 }, { 2000, 0, 0 } }, false, true);
    });
    const geom::EdgeId arcEdge = m.mesh(m.root()).edges().keys()[0];
    CHECK_FALSE(model::penCurveForEdge(m, m.root(), arcEdge));
}
