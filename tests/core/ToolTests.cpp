#include "aion/core/geom/Operations.h"
#include "aion/core/tools/Tool.h"
#include "aion/core/view/Picking.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

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

// Liniile de previzualizare, fără ghidajele inferenței.
std::size_t previewLines(const tools::Tool& tool)
{
    const auto lines = tool.overlay().lines;
    return static_cast<std::size_t>(
        std::count_if(lines.begin(), lines.end(), [](const tools::OverlayLine& l) { return !l.onTop; }));
}

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
    CHECK(previewLines(*line) == 1); // linia „elastică” spre cursor
    s.click(*line, { 0, 0, 0 });
    CHECK(s.mesh.faces().size() == 1);
    CHECK(previewLines(*line) == 0);
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


TEST_CASE("Unelte: o cameră de 4 × 3 m numai din inferențe și Measurements box (Faza 4)", "[core][tools][inference]")
{
    Scene s;
    auto line = tools::makeTool("line");
    const auto move = [&](Vec3d world, Vec2d offsetPx = { 0, 0 }) {
        line->pointerMove(s.ctx, Scene::at(s.screen(world) + offsetPx));
    };
    const auto type = [&](std::string_view text) { REQUIRE(line->applyMeasurement(s.ctx, text)); };

    // Primul punct în origine; cursorul doar „aproape” de axe, lungimile tastate.
    s.click(*line, { 0, 0, 0 });
    move({ 1300, 0, 0 }, { 0, 6 }); // lângă axa roșie
    CHECK(line->overlay().marker->label == "On Red Axis");
    type("4m");
    move({ 4000, 900, 0 }, { 5, 0 }); // lângă axa verde din (4000, 0)
    CHECK(line->overlay().marker->label == "On Green Axis");
    type("3000");
    move({ 2500, 3000, 0 }, { 0, -5 }); // înapoi pe axa roșie
    type("4000");
    // Ultimul segment: cursorul la câțiva pixeli de origine se lipește de capăt și închide fața.
    move({ 0, 0, 0 }, { 6, -5 });
    CHECK(line->overlay().marker->label == "Endpoint");
    line->pointerDown(s.ctx, Scene::at(s.screen({ 0, 0, 0 }) + Vec2d { 6, -5 }));

    REQUIRE(s.mesh.faces().size() == 1);
    const geom::Face& face = s.mesh.face(s.mesh.faces().keys().front());
    const auto pts = s.mesh.loopPoints(face.outer);
    REQUIRE(pts.size() == 4);
    for (const Vec3d& p : { Vec3d { 0, 0, 0 }, Vec3d { 4000, 0, 0 }, Vec3d { 4000, 3000, 0 }, Vec3d { 0, 3000, 0 } }) {
        CHECK(std::any_of(pts.begin(), pts.end(), [&](const Vec3d& q) { return glm::length(q - p) < 1e-6; }));
    }
}

TEST_CASE("Unelte: lungimea tastată și săgețile", "[core][tools][inference]")
{
    Scene s;
    auto line = tools::makeTool("line");
    CHECK(line->measurement()->label == "Lungime");
    CHECK_FALSE(line->applyMeasurement(s.ctx, "1000")); // încă nu există punct de început

    s.click(*line, { 0, 0, 0 });
    // Săgeata sus blochează pe axa albastră. În perspectivă, de sus, axa verticală prin origine apare
    // ca o rază din centrul vederii: în sus (spre cameră) înseamnă dinspre centru spre exterior.
    line->pointerMove(s.ctx, Scene::at(s.screen({ -300, -300, 0 })));
    REQUIRE(line->key(s.ctx, { tools::Key::Up, {} }));
    line->pointerMove(s.ctx, Scene::at(s.screen({ -300, -300, 0 })));
    CHECK(line->overlay().marker->locked);
    CHECK_FALSE(line->applyMeasurement(s.ctx, "abc"));
    REQUIRE(line->applyMeasurement(s.ctx, "2.5m"));
    REQUIRE(s.mesh.edges().size() == 1);
    const geom::Edge& e = s.mesh.edge(s.mesh.edges().keys().front());
    const Vec3d top = s.mesh.position(e.v0).z > 1.0 ? s.mesh.position(e.v0) : s.mesh.position(e.v1);
    CHECK(glm::length(top - Vec3d { 0, 0, 2500 }) < 1e-6);
    // Linia continuă din capătul nou.
    CHECK(line->measurement()->value.size() > 0);
}

TEST_CASE("Unelte: dreptunghi din Measurements box, în sensul cursorului", "[core][tools][inference]")
{
    Scene s;
    auto rect = tools::makeTool("rectangle");
    s.click(*rect, { 1000, 1000, 0 });
    rect->pointerMove(s.ctx, Scene::at(s.screen({ 600, 1300, 0 }))); // spre −X, +Y
    CHECK(rect->overlay().dimensions.size() == 2);
    REQUIRE(rect->applyMeasurement(s.ctx, "3000,2000"));
    REQUIRE(s.mesh.faces().size() == 1);
    Vec3d lo { 1e9 };
    Vec3d hi { -1e9 };
    for (geom::VertexId v : s.mesh.vertices().keys()) {
        lo = glm::min(lo, s.mesh.position(v));
        hi = glm::max(hi, s.mesh.position(v));
    }
    CHECK(glm::length(lo - Vec3d { -2000, 1000, 0 }) < 1e-6);
    CHECK(glm::length(hi - Vec3d { 1000, 3000, 0 }) < 1e-6);
}

TEST_CASE("Unelte: Push/Pull prin tragere, prin VCB și dublu-click", "[core][tools][pushpull]")
{
    Scene s;
    s.camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1000, 1000, 500 }, 0, 0, 9000 }));
    auto rect = tools::makeTool("rectangle");
    auto pp = tools::makeTool("pushpull");
    s.click(*rect, { 0, 0, 0 });
    s.click(*rect, { 2000, 1000, 0 });
    REQUIRE(s.mesh.faces().size() == 1);

    // Click pe față, mișcare în sus, apoi tastăm înălțimea.
    s.click(*pp, { 1000, 500, 0 });
    pp->pointerMove(s.ctx, Scene::at(s.screen({ 1000, 500, 800 })));
    CHECK(pp->measurement()->value == "800 mm");
    REQUIRE(pp->applyMeasurement(s.ctx, "1.2m"));
    CHECK(s.mesh.faces().size() == 6);
    Vec3d hi { -1e9 };
    for (geom::VertexId v : s.mesh.vertices().keys()) {
        hi = glm::max(hi, s.mesh.position(v));
    }
    CHECK(std::abs(hi.z - 1200) < 1e-6);
    CHECK(s.stack.undoName() == "Push/Pull");

    // Tragere pe fața de sus până la capătul unei muchii aflate mai sus (inferență pe adâncime).
    geom::drawLine(s.mesh, { 3000, 0, 0 }, { 3000, 0, 2000 });
    const tools::PointerEvent down { s.screen({ 1000, 500, 1200 }), {}, 1 };
    pp->pointerDown(s.ctx, down);
    pp->pointerMove(s.ctx, Scene::at(s.screen({ 1000, 500, 1600 })));
    pp->pointerMove(s.ctx, Scene::at(s.screen({ 3000, 0, 2000 }) + Vec2d { 3, -2 }));
    CHECK(pp->overlay().marker->label == "Endpoint");
    pp->pointerUp(s.ctx, Scene::at(s.screen({ 3000, 0, 2000 })));
    hi = Vec3d { -1e9 };
    for (geom::VertexId v : s.mesh.vertices().keys()) {
        hi = glm::max(hi, s.mesh.position(v));
    }
    CHECK(std::abs(hi.z - 2000) < 1e-6);

    // Dublu-click pe fața de sus repetă ultima distanță (+800).
    const Vec2d top = s.screen({ 1000, 500, 2000 });
    pp->pointerMove(s.ctx, Scene::at(top));
    pp->pointerDown(s.ctx, { top, {}, 1 });
    pp->pointerUp(s.ctx, Scene::at(top));
    pp->pointerDown(s.ctx, { top, {}, 2 });
    pp->pointerUp(s.ctx, Scene::at(top));
    hi = Vec3d { -1e9 };
    for (geom::VertexId v : s.mesh.vertices().keys()) {
        hi = glm::max(hi, s.mesh.position(v));
    }
    CHECK(std::abs(hi.z - 2800) < 1e-6);
}
