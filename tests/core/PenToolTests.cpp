#include "aion/core/commands/CommandStack.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/Model.h"
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
