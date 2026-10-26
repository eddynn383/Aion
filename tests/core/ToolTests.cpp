#include "aion/core/geom/Operations.h"
#include "aion/core/tools/Tool.h"
#include "aion/core/view/Picking.h"

#include <catch2/catch_test_macros.hpp>

using namespace aion;
using geom::Mesh;
using math::Vec2d;
using math::Vec3d;

namespace {

// O scenă cu camera privind de sus spre zona (0..2000, 0..2000) de pe sol.
struct Scene {
    Mesh mesh;
    commands::CommandStack stack { mesh };
    geom::Selection selection;
    view::Camera camera;
    tools::ToolContext ctx { mesh, stack, selection, camera };

    Scene()
    {
        stack.setValidation(true, geom::ValidationLevel::Full);
        stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
        camera.setViewportSize(1000, 800);
        camera.setPose(view::standardViewPose(view::StandardView::Top, { { 1000, 1000, 0 }, 0, 0, 6000 }));
    }

    Vec2d screen(Vec3d world) const
    {
        const auto s = camera.project(world);
        REQUIRE(s.has_value());
        return *s;
    }

    void click(tools::Tool& tool, Vec3d world, tools::Modifiers mods = {}, int clicks = 1)
    {
        const tools::PointerEvent e { screen(world), mods, clicks };
        tool.pointerMove(ctx, e);
        tool.pointerDown(ctx, e);
        tool.pointerUp(ctx, e);
    }

    static tools::PointerEvent at(Vec2d p)
    {
        tools::PointerEvent e;
        e.screen = p;
        return e;
    }

    void drag(tools::Tool& tool, Vec2d from, Vec2d to)
    {
        tool.pointerDown(ctx, at(from));
        tool.pointerMove(ctx, at((from + to) * 0.5));
        tool.pointerMove(ctx, at(to));
        tool.pointerUp(ctx, at(to));
    }
};

} // namespace

TEST_CASE("Unelte: dreptunghi, linie, selecție, ștergere, undo (scenariul din Faza 3)", "[core][tools]")
{
    Scene s;
    auto rect = tools::makeTool("rectangle");
    auto line = tools::makeTool("line");
    auto select = tools::makeTool("select");

    // Dreptunghi de 2000 × 1000 pe sol.
    s.click(*rect, { 0, 0, 0 });
    s.click(*rect, { 2000, 1000, 0 });
    REQUIRE(s.mesh.faces().size() == 1);

    // Linie verticală care îl împarte în două.
    s.click(*line, { 1000, 0, 0 });
    s.click(*line, { 1000, 1000, 0 });
    line->key(s.ctx, { tools::Key::Escape, {} });
    REQUIRE(s.mesh.faces().size() == 2);

    // Selectăm fața din stânga și o ștergem.
    s.click(*select, { 500, 500, 0 });
    REQUIRE(s.selection.faces.size() == 1);
    REQUIRE(s.selection.edges.empty());
    REQUIRE(tools::deleteSelection(s.ctx));
    CHECK(s.mesh.faces().size() == 1);
    CHECK(s.selection.empty());

    // Undo readuce fața.
    REQUIRE(s.stack.undo());
    CHECK(s.mesh.faces().size() == 2);
    CHECK(s.stack.undoName() == "Line");
}

TEST_CASE("Unelte: linia continuă din ultimul punct și se oprește la închiderea unei fețe", "[core][tools]")
{
    Scene s;
    auto line = tools::makeTool("line");
    s.click(*line, { 0, 0, 0 });
    s.click(*line, { 1000, 0, 0 });
    s.click(*line, { 1000, 1000, 0 });
    s.click(*line, { 0, 1000, 0 });
    CHECK(s.mesh.faces().empty());
    CHECK(line->overlay().lines.size() == 1); // linia „elastică” spre cursor
    s.click(*line, { 0, 0, 0 });
    CHECK(s.mesh.faces().size() == 1);
    CHECK(line->overlay().lines.empty());
    CHECK(s.stack.undoCount() == 4); // câte o comandă pe segment
}

TEST_CASE("Unelte: dreptunghi pe o față verticală stă în planul feței", "[core][tools]")
{
    Scene s;
    // Un perete în planul Y = 0, privit din față.
    geom::drawPolygon(s.mesh, { { 0, 0, 0 }, { 3000, 0, 0 }, { 3000, 0, 2000 }, { 0, 0, 2000 } });
    s.camera.setPose(view::standardViewPose(view::StandardView::Front, { { 1500, 0, 1000 }, 0, 0, 8000 }));
    auto rect = tools::makeTool("rectangle");
    s.click(*rect, { 500, 0, 500 });
    s.click(*rect, { 1500, 0, 1200 });
    REQUIRE(s.mesh.faces().size() == 2);
    for (geom::VertexId v : s.mesh.vertices().keys()) {
        CHECK(std::abs(s.mesh.position(v).y) < 1e-6);
    }
}

TEST_CASE("Unelte: selecție cu modificatori, dublu și triplu click", "[core][tools]")
{
    Scene s;
    geom::drawPolygon(s.mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    geom::drawPolygon(s.mesh, { { 3000, 0, 0 }, { 4000, 0, 0 }, { 4000, 1000, 0 }, { 3000, 1000, 0 } });
    s.camera.setPose(view::standardViewPose(view::StandardView::Top, { { 2000, 500, 0 }, 0, 0, 9000 }));
    auto select = tools::makeTool("select");

    s.click(*select, { 500, 500, 0 });
    CHECK(s.selection.faces.size() == 1);

    tools::Modifiers cmd;
    cmd.command = true;
    s.click(*select, { 3500, 500, 0 }, cmd);
    CHECK(s.selection.faces.size() == 2);

    tools::Modifiers shift;
    shift.shift = true;
    s.click(*select, { 3500, 500, 0 }, shift); // Shift inversează: o scoate
    CHECK(s.selection.faces.size() == 1);

    s.click(*select, { 500, 500, 0 }, {}, 2); // dublu-click: fața și muchiile ei
    CHECK(s.selection.faces.size() == 1);
    CHECK(s.selection.edges.size() == 4);

    s.click(*select, { 1000, 500, 0 }); // click pe o muchie
    CHECK(s.selection.edges.size() == 1);
    CHECK(s.selection.faces.empty());

    s.click(*select, { 1000, 500, 0 }, {}, 3); // triplu-click: tot ce e conectat
    CHECK(s.selection.edges.size() == 4);
    CHECK(s.selection.faces.size() == 1);

    s.click(*select, { 2000, 3000, 0 }); // click în gol: deselectează
    CHECK(s.selection.empty());
}

TEST_CASE("Unelte: selecție prin dreptunghi, window și crossing", "[core][tools]")
{
    Scene s;
    geom::drawPolygon(s.mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    auto select = tools::makeTool("select");

    // Stânga → dreapta, acoperă doar jumătate din pătrat: nimic complet în interior, deci doar muchia din stânga.
    s.drag(*select, s.screen({ -200, -200, 0 }), s.screen({ 500, 1200, 0 }));
    CHECK(s.selection.faces.empty());
    CHECK(s.selection.edges.size() == 1);

    // Dreapta → stânga, aceeași zonă: tot ce atinge dreptunghiul.
    s.drag(*select, s.screen({ 500, 1200, 0 }), s.screen({ -200, -200, 0 }));
    CHECK(s.selection.faces.size() == 1);
    CHECK(s.selection.edges.size() == 3);

    // Fereastră care cuprinde tot.
    s.drag(*select, s.screen({ -200, -200, 0 }), s.screen({ 1200, 1200, 0 }));
    CHECK(s.selection.faces.size() == 1);
    CHECK(s.selection.edges.size() == 4);
}

TEST_CASE("Unelte: Eraser șterge muchia (și fețele ei) printr-o singură comandă", "[core][tools]")
{
    Scene s;
    geom::drawPolygon(s.mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    geom::drawLine(s.mesh, { 0, 0, 0 }, { 1000, 1000, 0 });
    auto eraser = tools::makeTool("eraser");

    // Trage peste diagonală și peste latura de jos.
    s.drag(*eraser, s.screen({ 500, 500, 0 }), s.screen({ 500, 0, 0 }));
    CHECK(s.mesh.faces().empty()); // ambele fețe depindeau de muchiile șterse
    CHECK(s.mesh.edges().size() == 3);
    CHECK(s.stack.undoCount() == 1);
    s.stack.undo();
    CHECK(s.mesh.faces().size() == 2);
}

TEST_CASE("Picking: vârful are prioritate, iar muchiile ascunse nu se aleg", "[core][view][picking]")
{
    Scene s;
    geom::drawPolygon(s.mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    // O muchie sub față (z = −500), ascunsă de ea privind de sus.
    geom::drawLine(s.mesh, { 200, 500, -500 }, { 800, 500, -500 });

    const auto corner = view::pick(s.mesh, s.camera, s.screen({ 1000, 1000, 0 }));
    CHECK(corner.kind == view::PickResult::Kind::Vertex);

    const auto edge = view::pick(s.mesh, s.camera, s.screen({ 1000, 500, 0 }));
    CHECK(edge.kind == view::PickResult::Kind::Edge);

    const auto hidden = view::pick(s.mesh, s.camera, s.screen({ 500, 500, 0 }));
    CHECK(hidden.kind == view::PickResult::Kind::Face);

    const auto empty = view::pick(s.mesh, s.camera, s.screen({ 3000, 3000, 0 }));
    CHECK_FALSE(empty.hit());
}

