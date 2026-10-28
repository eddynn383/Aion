#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/Model.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/PenCurves.h"
#include "aion/core/tools/Tool.h"

#include <catch2/catch_test_macros.hpp>

#include <glm/geometric.hpp>

#include <cmath>

using namespace aion;
using math::Vec2d;
using math::Vec3d;

namespace {

// Un model cu camera de sus spre zona (0..2000, 0..2000) și unealta Pen.
struct PenScene {
    model::Model model;
    commands::CommandStack stack { model };
    geom::Selection selection;
    view::Camera camera;
    tools::ToolRequests requests;
    tools::ToolContext ctx { model.mesh(model.root()), stack, selection, camera };
    std::unique_ptr<tools::Tool> pen = tools::makeTool("pen");

    explicit PenScene(view::StandardView view = view::StandardView::Top, Vec3d target = { 1000, 1000, 0 })
    {
        camera.setViewportSize(1000, 800);
        camera.setPose(view::standardViewPose(view, { target, 0, 0, 6000 }));
        ctx.model = &model;
        ctx.context = model.root();
        ctx.requests = &requests;
        REQUIRE(pen);
    }

    Vec2d screen(Vec3d world) const
    {
        const auto s = camera.project(world);
        REQUIRE(s.has_value());
        return *s;
    }

    static tools::PointerEvent at(Vec2d p, int clicks = 1)
    {
        tools::PointerEvent e;
        e.screen = p;
        e.clickCount = clicks;
        return e;
    }

    void clickAt(Vec2d s)
    {
        pen->pointerMove(ctx, at(s));
        pen->pointerDown(ctx, at(s));
        pen->pointerUp(ctx, at(s));
    }

    void click(Vec3d world) { clickAt(screen(world)); }

    // Click și tragere: un punct neted, cu mânerul de ieșire la `handle`.
    void drag(Vec3d world, Vec3d handle)
    {
        const Vec2d a = screen(world);
        const Vec2d b = screen(handle);
        pen->pointerMove(ctx, at(a));
        pen->pointerDown(ctx, at(a));
        pen->pointerMove(ctx, at((a + b) * 0.5));
        pen->pointerMove(ctx, at(b));
        pen->pointerUp(ctx, at(b));
    }

    const model::PenCurve& onlyCurve() const
    {
        REQUIRE(model.penCurves().size() == 1);
        return model.penCurves().values()[0];
    }
};

} // namespace

TEST_CASE("Pen: click, click cu tragere, Enter → o curbă cu un punct neted", "[core][tools][pen]")
{
    PenScene s;
    s.click({ 0, 0, 0 });
    s.drag({ 1000, 800, 0 }, { 1500, 800, 0 });
    s.click({ 2000, 0, 0 });
    CHECK(s.model.penCurves().empty()); // încă desenează
    CHECK(s.pen->key(s.ctx, { tools::Key::Enter, {} }));
    const geom::BezierPath& p = s.onlyCurve().path;
    REQUIRE(p.points.size() == 3);
    CHECK_FALSE(p.closed);
    CHECK_FALSE(p.points[0].smooth);
    CHECK(tol::samePoint(p.points[0].in, p.points[0].point));
    CHECK(tol::samePoint(p.points[0].out, p.points[0].point));
    CHECK(p.points[1].smooth);
    CHECK(tol::samePoint(p.points[1].point, { 1000, 800, 0 }));
    CHECK(glm::length(p.points[1].out - Vec3d { 1500, 800, 0 }) < 1.0);
    CHECK(tol::samePoint(p.points[1].in, 2.0 * p.points[1].point - p.points[1].out));
    CHECK(s.model.mesh(s.model.root()).edges().size() > 4);
}

TEST_CASE("Pen: click pe primul punct închide curba și face o față", "[core][tools][pen]")
{
    PenScene s;
    s.click({ 0, 0, 0 });
    s.drag({ 1500, 200, 0 }, { 1800, 700, 0 });
    s.click({ 800, 1500, 0 });
    s.clickAt(s.screen({ 0, 0, 0 }) + Vec2d { 3, 2 });
    const model::PenCurve& c = s.onlyCurve();
    CHECK(c.path.closed);
    CHECK(c.path.points.size() == 3);
    CHECK(s.model.mesh(s.model.root()).faces().size() == 1);
}

TEST_CASE("Pen: închiderea câștigă lângă alt punct prins", "[core][tools][pen]")
{
    PenScene s;
    // Un capăt de muchie la ~6 px de primul punct al curbei.
    const double perPixel = 1.0 / (s.screen({ 1, 0, 0 }).x - s.screen({ 0, 0, 0 }).x);
    const Vec3d nearby { 6.0 * perPixel, -600, 0 };
    s.click({ 0, 0, 0 });
    // Desenată după primul click (altfel și primul punct s-ar prinde de ea).
    s.stack.run("Line", [&](geom::Mesh& mesh) { geom::drawLine(mesh, { 6.0 * perPixel, 0, 0 }, nearby); });
    s.click({ 1500, 300, 0 });
    s.click({ 700, 1400, 0 });
    s.clickAt(s.screen({ 0, 0, 0 }) + Vec2d { 3, 0 });
    const model::PenCurve& c = s.onlyCurve();
    CHECK(c.path.closed);
    CHECK(tol::samePoint(c.path.points[0].point, { 0, 0, 0 }));
}

TEST_CASE("Pen: pe fața unui obiect, curba stă pe planul feței", "[core][tools][pen]")
{
    // Din față (privind spre +Y), un perete vertical la y = 1000.
    PenScene s(view::StandardView::Front, { 1000, 1000, 1000 });
    s.stack.run("Wall", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, 1000, 0 }, { 2000, 1000, 0 }, { 2000, 1000, 2000 }, { 0, 1000, 2000 } });
    });
    s.click({ 500, 1000, 500 });
    s.drag({ 1500, 1000, 600 }, { 1600, 1000, 1200 });
    s.click({ 1000, 1000, 1600 });
    CHECK(s.pen->key(s.ctx, { tools::Key::Enter, {} }));
    const model::PenCurve& c = s.onlyCurve();
    CHECK(std::abs(std::abs(c.normal.y) - 1.0) < 1e-9);
    for (const geom::BezierPoint& p : c.path.points) {
        CHECK(std::abs(p.point.y - 1000.0) < tol::kLength);
        CHECK(std::abs(p.out.y - 1000.0) < tol::kLength);
    }
}

TEST_CASE("Pen: Esc cu un singur punct anulează; cu două termină", "[core][tools][pen]")
{
    PenScene s;
    s.click({ 0, 0, 0 });
    CHECK(s.pen->key(s.ctx, { tools::Key::Escape, {} }));
    CHECK(s.model.penCurves().empty());
    CHECK(s.model.mesh(s.model.root()).edges().empty());

    s.click({ 0, 0, 0 });
    s.click({ 1500, 900, 0 });
    CHECK(s.pen->key(s.ctx, { tools::Key::Escape, {} }));
    CHECK(s.onlyCurve().path.points.size() == 2);
}

TEST_CASE("Pen: dublu click termină curba deschisă", "[core][tools][pen]")
{
    PenScene s;
    s.click({ 0, 0, 0 });
    s.click({ 1500, 900, 0 });
    const Vec2d last = s.screen({ 1500, 900, 0 });
    s.pen->pointerDown(s.ctx, PenScene::at(last, 2));
    s.pen->pointerUp(s.ctx, PenScene::at(last, 2));
    CHECK(s.onlyCurve().path.points.size() == 2);
}

TEST_CASE("Pen: în Animare cere o cale în loc de geometrie", "[core][tools][pen]")
{
    PenScene s;
    s.ctx.drawsPaths = true;
    s.click({ 0, 0, 0 });
    s.drag({ 1000, 800, 0 }, { 1500, 800, 0 });
    s.click({ 2000, 0, 0 });
    CHECK(s.pen->key(s.ctx, { tools::Key::Enter, {} }));
    CHECK(s.model.penCurves().empty());
    CHECK(s.model.mesh(s.model.root()).edges().empty());
    REQUIRE(s.requests.penPath);
    CHECK(s.requests.penPath->points.size() == 3);
    CHECK(tol::samePoint(s.requests.penNormal, { 0, 0, 1 }));
}

namespace {

// Editarea unei curbe: Document pune curba în context la fiecare apel; aici o actualizăm după fiecare cerere.
struct EditScene : PenScene {
    std::unique_ptr<tools::Tool> edit = tools::makeTool("penedit");

    EditScene()
    {
        REQUIRE(edit);
        geom::BezierPoint a { { 0, 0, 0 }, { 0, 0, 0 }, { 0, 0, 0 }, false };
        geom::BezierPoint b { { 1000, 800, 0 }, { 500, 800, 0 }, { 1500, 800, 0 }, true };
        geom::BezierPoint c { { 2000, 0, 0 }, { 2000, 0, 0 }, { 2000, 0, 0 }, false };
        ctx.editCurve = geom::BezierPath { { a, b, c }, false };
        ctx.editNormal = { 0, 0, 1 };
    }

    // Aplică cererea uneltei (ca Document) și o golește.
    geom::BezierPath applied()
    {
        REQUIRE(requests.curveEdit);
        ctx.editCurve = *requests.curveEdit;
        requests = {};
        return *ctx.editCurve;
    }

    void press(Vec2d a, tools::Modifiers mods = {})
    {
        tools::PointerEvent e = at(a);
        e.modifiers = mods;
        edit->pointerMove(ctx, e);
        edit->pointerDown(ctx, e);
    }

    void release(Vec2d a, tools::Modifiers mods = {})
    {
        tools::PointerEvent e = at(a);
        e.modifiers = mods;
        edit->pointerUp(ctx, e);
    }

    void tap(Vec3d world, tools::Modifiers mods = {})
    {
        press(screen(world), mods);
        release(screen(world), mods);
    }

    void dragFrom(Vec3d from, Vec3d to, tools::Modifiers mods = {})
    {
        const Vec2d a = screen(from);
        const Vec2d b = screen(to);
        press(a, mods);
        tools::PointerEvent e = at((a + b) * 0.5);
        e.modifiers = mods;
        edit->pointerMove(ctx, e);
        e.screen = b;
        edit->pointerMove(ctx, e);
        release(b, mods);
    }
};

} // namespace

TEST_CASE("Pen edit: tragerea unui punct îl mută cu mânerele lui", "[core][tools][pen]")
{
    EditScene s;
    s.dragFrom({ 1000, 800, 0 }, { 1200, 900, 0 });
    const geom::BezierPath p = s.applied();
    CHECK(glm::length(p.points[1].point - Vec3d { 1200, 900, 0 }) < 1.0);
    CHECK(glm::length(p.points[1].out - Vec3d { 1700, 900, 0 }) < 1.0);
    CHECK(glm::length(p.points[1].in - Vec3d { 700, 900, 0 }) < 1.0);
    CHECK(tol::samePoint(p.points[0].point, { 0, 0, 0 }));
}

TEST_CASE("Pen edit: tragerea unui mâner neted îl oglindește pe celălalt", "[core][tools][pen]")
{
    EditScene s;
    s.tap({ 1000, 800, 0 }); // selectează punctul: mânerele lui se pot trage
    CHECK_FALSE(s.requests.curveEdit);
    s.dragFrom({ 1500, 800, 0 }, { 1500, 1100, 0 });
    const geom::BezierPath p = s.applied();
    CHECK(glm::length(p.points[1].out - Vec3d { 1500, 1100, 0 }) < 1.0);
    CHECK(glm::length(p.points[1].in - Vec3d { 500, 500, 0 }) < 1.0);
    CHECK(p.points[1].smooth);
}

TEST_CASE("Pen edit: Option pe un mâner rupe perechea", "[core][tools][pen]")
{
    EditScene s;
    s.tap({ 1000, 800, 0 });
    s.dragFrom({ 1500, 800, 0 }, { 1500, 1100, 0 }, { false, false, true });
    const geom::BezierPath p = s.applied();
    CHECK(glm::length(p.points[1].out - Vec3d { 1500, 1100, 0 }) < 1.0);
    CHECK(tol::samePoint(p.points[1].in, { 500, 800, 0 }));
    CHECK_FALSE(p.points[1].smooth);
}

TEST_CASE("Pen edit: Option + click comută între colț și neted", "[core][tools][pen]")
{
    EditScene s;
    s.tap({ 1000, 800, 0 }, { false, false, true });
    geom::BezierPath p = s.applied();
    CHECK_FALSE(p.points[1].smooth);
    CHECK(tol::samePoint(p.points[1].in, p.points[1].point));
    CHECK(tol::samePoint(p.points[1].out, p.points[1].point));
    s.tap({ 1000, 800, 0 }, { false, false, true });
    p = s.applied();
    CHECK(p.points[1].smooth);
    // Mânerele pe direcția vecinilor (de la primul spre ultimul), la o treime din distanța până la ei.
    const Vec3d dir = glm::normalize(Vec3d { 2000, 0, 0 } - Vec3d { 0, 0, 0 });
    CHECK(glm::length(p.points[1].out - (p.points[1].point + dir * (glm::length(Vec3d { 1000, -800, 0 }) / 3.0))) < 1e-6);
    CHECK(glm::length(p.points[1].in - (p.points[1].point - dir * (glm::length(Vec3d { 1000, 800, 0 }) / 3.0))) < 1e-6);
}

TEST_CASE("Pen edit: click pe curbă adaugă un punct fără să schimbe forma", "[core][tools][pen]")
{
    EditScene s;
    const geom::BezierPath before = *s.ctx.editCurve;
    const Vec3d onCurve = geom::bezierPoint(before, 0, 0.5);
    s.tap(onCurve);
    const geom::BezierPath p = s.applied();
    REQUIRE(p.points.size() == 4);
    CHECK(glm::length(p.points[1].point - onCurve) < 30.0); // unde e cursorul, pe curbă
    const geom::ArcLength a(before);
    const geom::ArcLength b(p);
    CHECK(std::abs(a.length() - b.length()) < 1.0);
    for (int i = 0; i <= 10; ++i) {
        CHECK(glm::length(a.at(a.length() * i / 10.0).point - b.at(b.length() * i / 10.0).point) < 1.0);
    }
}

TEST_CASE("Pen edit: Delete șterge punctul selectat", "[core][tools][pen]")
{
    EditScene s;
    s.tap({ 1000, 800, 0 });
    CHECK(s.edit->key(s.ctx, { tools::Key::Delete, {} }));
    const geom::BezierPath p = s.applied();
    REQUIRE(p.points.size() == 2);
    CHECK(tol::samePoint(p.points[1].point, { 2000, 0, 0 }));
    // Sub două puncte nu se mai șterge nimic.
    s.tap({ 2000, 0, 0 });
    s.edit->key(s.ctx, { tools::Key::Delete, {} });
    CHECK_FALSE(s.requests.curveEdit);
}

TEST_CASE("Pen edit: Esc și click în afară termină editarea", "[core][tools][pen]")
{
    EditScene s;
    CHECK(s.edit->key(s.ctx, { tools::Key::Escape, {} }));
    CHECK(s.requests.curveEditDone);
    s.requests = {};
    s.tap({ 1000, -1500, 0 });
    CHECK(s.requests.curveEditDone);
    CHECK_FALSE(s.requests.curveEdit);
}

TEST_CASE("Select: dublu click pe o curbă Pen editabilă cere editarea ei", "[core][tools][pen]")
{
    PenScene s;
    s.click({ 0, 0, 0 });
    s.drag({ 1000, 800, 0 }, { 1500, 800, 0 });
    s.click({ 2000, 0, 0 });
    REQUIRE(s.pen->key(s.ctx, { tools::Key::Enter, {} }));
    const model::PenCurveId id = s.model.penCurves().keys()[0];
    auto select = tools::makeTool("select");
    const geom::BezierPath& path = s.model.penCurves().at(id).path;
    const Vec2d onCurve = s.screen(geom::bezierPoint(path, 1, 0.5));

    SECTION("pe o curbă editabilă")
    {
        select->pointerDown(s.ctx, PenScene::at(onCurve, 1));
        select->pointerUp(s.ctx, PenScene::at(onCurve, 1));
        CHECK_FALSE(s.requests.editPenCurve);
        select->pointerDown(s.ctx, PenScene::at(onCurve, 2));
        select->pointerUp(s.ctx, PenScene::at(onCurve, 2));
        CHECK(s.requests.editPenCurve == id);
    }
    SECTION("pe una needitabilă (un vârf mutat), comportamentul de până acum")
    {
        const geom::Mesh& mesh = s.model.mesh(s.model.root());
        const geom::VertexId v = mesh.vertices().keys()[1];
        s.stack.run("Move", [&](geom::Mesh& m) { m.setPosition(v, mesh.position(v) + Vec3d { 0, 0, 50 }); });
        REQUIRE_FALSE(model::penCurveEditable(s.model, id));
        select->pointerDown(s.ctx, PenScene::at(onCurve, 2));
        select->pointerUp(s.ctx, PenScene::at(onCurve, 2));
        CHECK_FALSE(s.requests.editPenCurve);
    }
}

TEST_CASE("Pen: pe fața unui grup, curba stă pe planul feței", "[core][tools][pen]")
{
    PenScene s(view::StandardView::Front, { 1000, 1000, 1000 });
    s.stack.run("Wall", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, 1000, 0 }, { 2000, 1000, 0 }, { 2000, 1000, 2000 }, { 0, 1000, 2000 } });
    });
    geom::Selection all;
    const geom::Mesh& mesh = s.model.mesh(s.model.root());
    all.faces.insert(mesh.faces().keys().begin(), mesh.faces().keys().end());
    s.stack.run("Group", [&](model::Model& model) { model::makeGroup(model, model.root(), all, false); });
    REQUIRE(s.model.mesh(s.model.root()).faces().empty());
    s.click({ 500, 1000, 500 });
    s.click({ 1500, 1000, 600 });
    s.click({ 1000, 1000, 1600 });
    CHECK(s.pen->key(s.ctx, { tools::Key::Enter, {} }));
    const model::PenCurve& c = s.onlyCurve();
    CHECK(std::abs(std::abs(c.normal.y) - 1.0) < 1e-9);
    for (const geom::BezierPoint& p : c.path.points) {
        CHECK(std::abs(p.point.y - 1000.0) < 1e-6);
    }
}
