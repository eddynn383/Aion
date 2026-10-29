#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/SnapSources.h"
#include "aion/core/model/Texturing.h"
#include "aion/core/tools/Tool.h"
#include "aion/core/view/Picking.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <iterator>
#include <set>
#include <cmath>

using namespace aion;
using Catch::Approx;
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

TEST_CASE("Select: hover-ul în aceeași față nu schimbă overlay-ul (viewport-ul nu se reface)", "[core][tools]")
{
    Scene s;
    s.stack.run("Rect", [&](Mesh& m) {
        geom::drawPolygon(m, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 2000, 0 }, { 0, 2000, 0 } });
    });
    auto select = tools::makeTool("select");
    select->pointerMove(s.ctx, Scene::at(s.screen({ 700, 700, 0 })));
    const tools::Overlay first = select->overlay();
    const view::PickResult hover = select->hover();
    REQUIRE(hover.kind == view::PickResult::Kind::Face);
    select->pointerMove(s.ctx, Scene::at(s.screen({ 900, 1100, 0 })));
    CHECK(select->overlay() == first);
    CHECK(select->hover().kind == hover.kind);
    CHECK(select->hover().face == hover.face);
    // Pe o muchie: alt hover (desenul se schimbă).
    select->pointerMove(s.ctx, Scene::at(s.screen({ 1000, 0, 0 })));
    CHECK(select->hover().kind == view::PickResult::Kind::Edge);
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

namespace {

double maxCoord(const Mesh& mesh, int axis)
{
    double hi = -1e18;
    for (geom::VertexId v : mesh.vertices().keys()) {
        hi = std::max(hi, mesh.position(v)[axis]);
    }
    return hi;
}

// Cutie 2000 × 1000 × 1000 desenată cu uneltele.
void toolBox(Scene& s)
{
    auto rect = tools::makeTool("rectangle");
    s.click(*rect, { 0, 0, 0 });
    rect->pointerMove(s.ctx, Scene::at(s.screen({ 2000, 1000, 0 })));
    REQUIRE(rect->applyMeasurement(s.ctx, "2000,1000"));
    geom::pushPull(s.mesh, s.mesh.faces().keys().front(), -1000);
    REQUIRE(s.mesh.faces().size() == 6);
}

} // namespace

TEST_CASE("Unelte: Move cu distanța tastată, copiere și array", "[core][tools][transform]")
{
    Scene s;
    s.camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 2000, 1000, 500 }, 0, 0, 14000 }));
    toolBox(s);
    auto move = tools::makeTool("move");
    auto select = tools::makeTool("select");
    s.click(*select, { 1000, 500, 1000 }, {}, 3); // triplu-click: toată cutia
    REQUIRE(s.selection.faces.size() == 6);

    // Mutare de 500 pe axa roșie.
    s.click(*move, { 0, 0, 1000 });
    move->pointerMove(s.ctx, Scene::at(s.screen({ 800, 0, 1000 })));
    REQUIRE(move->applyMeasurement(s.ctx, "500"));
    CHECK(maxCoord(s.mesh, 0) == Approx(2500));
    CHECK(s.stack.undoName() == "Move");
    CHECK(s.selection.faces.size() == 6);

    // Copie cu Option la 3000, apoi x3: în total trei copii.
    tools::Modifiers option;
    option.alt = true;
    s.click(*move, { 500, 0, 1000 }, option);
    move->pointerMove(s.ctx, { s.screen({ 1500, 0, 1000 }), option, 1 });
    REQUIRE(move->applyMeasurement(s.ctx, "3000"));
    CHECK(s.mesh.faces().size() == 12);
    REQUIRE(move->applyMeasurement(s.ctx, "x3"));
    CHECK(s.mesh.faces().size() == 24);
    CHECK(maxCoord(s.mesh, 0) == Approx(2500 + 9000));

    // „/2” după o copie nouă: copia se înlocuiește cu două, la jumătate și la capăt.
    s.stack.undo();
    REQUIRE(s.mesh.faces().size() == 12);
    CHECK_FALSE(move->applyMeasurement(s.ctx, "/2")); // ultima comandă nu mai e o copiere
}

TEST_CASE("Unelte: Rotate cu unghiul tastat și Scale cu factorul tastat", "[core][tools][transform]")
{
    Scene s;
    s.camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 0, 0, 500 }, 0, 0, 12000 }));
    toolBox(s);
    auto select = tools::makeTool("select");
    s.click(*select, { 1000, 500, 1000 }, {}, 3);
    REQUIRE(s.selection.faces.size() == 6);

    // Centrul pe colțul de sus (0, 0, 1000). Colțul e și pe fețele laterale, deci alegem axa cu săgeata sus.
    auto rotate = tools::makeTool("rotate");
    rotate->pointerMove(s.ctx, Scene::at(s.screen({ 0, 0, 1000 })));
    REQUIRE(rotate->key(s.ctx, { tools::Key::Up, {} }));
    s.click(*rotate, { 0, 0, 1000 });
    s.click(*rotate, { 2000, 0, 1000 }); // referința: capătul laturii de pe axa roșie
    rotate->pointerMove(s.ctx, Scene::at(s.screen({ 100, 1500, 1000 })));
    REQUIRE(rotate->applyMeasurement(s.ctx, "90"));
    CHECK(s.stack.undoName() == "Rotate");
    // Cutia stă acum între x = −1000 și 0, y = 0 și 2000.
    CHECK(maxCoord(s.mesh, 1) == Approx(2000));
    CHECK(maxCoord(s.mesh, 0) == Approx(0).margin(1e-6));

    // Scale: mânerul feței +Y (centrul ei), apoi factorul 2 → doar Y se dublează, față de fața opusă (y = 0).
    auto scale = tools::makeTool("scale");
    scale->pointerMove(s.ctx, Scene::at(s.screen({ -500, 2000, 500 })));
    s.click(*scale, { -500, 2000, 500 });
    scale->pointerMove(s.ctx, Scene::at(s.screen({ -500, 2600, 500 })));
    REQUIRE(scale->applyMeasurement(s.ctx, "2"));
    CHECK(maxCoord(s.mesh, 1) == Approx(4000));
    CHECK(maxCoord(s.mesh, 0) == Approx(0).margin(1e-6));
    CHECK(maxCoord(s.mesh, 2) == Approx(1000));
    REQUIRE(s.mesh.validate(geom::ValidationLevel::Full).ok());

    // Pe axe: mânerul de colț (0, 4000, 1000) și „1,1,1.5” → doar înălțimea crește, față de colțul opus.
    scale->pointerMove(s.ctx, Scene::at(s.screen({ 0, 4000, 1000 })));
    s.click(*scale, { 0, 4000, 1000 });
    REQUIRE(scale->applyMeasurement(s.ctx, "1,1,1.5"));
    CHECK(maxCoord(s.mesh, 2) == Approx(1500));
    CHECK(maxCoord(s.mesh, 1) == Approx(4000));
    REQUIRE(s.mesh.validate(geom::ValidationLevel::Full).ok());
}

TEST_CASE("Unelte: acoperiș în două ape cu Move pe linia de coamă (săgeata sus)", "[core][tools][transform]")
{
    Scene s;
    s.camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1000, 500, 500 }, 0, 0, 9000 }));
    toolBox(s);
    auto line = tools::makeTool("line");
    // Coama: între mijloacele laturilor scurte ale feței de sus.
    s.click(*line, { 0, 500, 1000 });
    s.click(*line, { 2000, 500, 1000 });
    line->key(s.ctx, { tools::Key::Escape, {} });
    REQUIRE(s.mesh.faces().size() == 7);

    auto move = tools::makeTool("move");
    s.click(*move, { 1000, 500, 1000 }); // fără selecție: mută muchia de sub cursor
    REQUIRE(move->key(s.ctx, { tools::Key::Up, {} }));
    // Cu axa albastră blocată, cursorul poate fi oriunde: rămâne pe verticală.
    move->pointerMove(s.ctx, Scene::at(s.screen({ 1500, 900, 1300 })));
    CHECK(move->overlay().marker->locked);
    REQUIRE(move->applyMeasurement(s.ctx, "800"));
    CHECK(maxCoord(s.mesh, 2) == Approx(1800));
    CHECK(s.mesh.faces().size() == 7);
    REQUIRE(s.mesh.validate(geom::ValidationLevel::Full).ok());
}

TEST_CASE("Unelte: cerc, cilindru neted, selecția și ștergerea curbei", "[core][tools][curves]")
{
    Scene s;
    auto circle = tools::makeTool("circle");
    s.click(*circle, { 1000, 1000, 0 });
    circle->pointerMove(s.ctx, Scene::at(s.screen({ 1400, 1000, 0 })));
    REQUIRE(circle->applyMeasurement(s.ctx, "500"));
    REQUIRE(s.mesh.faces().size() == 1);
    CHECK(s.mesh.edges().size() == 24);
    const std::uint32_t curve = s.mesh.edges().values().front().curve;
    CHECK(curve != 0);
    for (const geom::Edge& e : s.mesh.edges().values()) {
        CHECK(e.curve == curve);
        CHECK(std::abs(glm::length(s.mesh.position(e.v0) - Vec3d { 1000, 1000, 0 }) - 500) < 1e-6);
    }

    // Un click pe o muchie selectează tot cercul.
    auto select = tools::makeTool("select");
    s.click(*select, { 1500, 1000, 0 });
    CHECK(s.selection.edges.size() == 24);
    s.selection.clear();

    // Extrudat: un cilindru cu laturile soft și capacul tot o curbă.
    geom::pushPull(s.mesh, s.mesh.faces().keys().front(), -800);
    REQUIRE(s.mesh.validate(geom::ValidationLevel::Full).ok());
    CHECK(s.mesh.faces().size() == 26);
    int soft = 0;
    int capCurve = 0;
    for (const geom::Edge& e : s.mesh.edges().values()) {
        soft += e.soft ? 1 : 0;
        const double z0 = s.mesh.position(e.v0).z;
        const double z1 = s.mesh.position(e.v1).z;
        if (z0 > 799 && z1 > 799) {
            capCurve += (e.curve != 0 && e.curve != curve) ? 1 : 0;
        }
    }
    CHECK(soft == 24);
    CHECK(capCurve == 24);

    // Guma pe o muchie a cercului de jos: dispare toată curba (și fețele ei).
    auto eraser = tools::makeTool("eraser");
    s.camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1000, 1000, 400 }, 0, 0, 8000 }));
    const Vec2d onBottom = s.screen({ 1000, 500, 0 });
    s.drag(*eraser, onBottom, onBottom + Vec2d { 1, 0 });
    CHECK(std::none_of(s.mesh.edges().values().begin(), s.mesh.edges().values().end(),
        [&](const geom::Edge& e) { return e.curve == curve; }));
}

TEST_CASE("Unelte: poligonul extrudat are laturi vizibile; arcul trece prin capete și săgeată", "[core][tools][curves]")
{
    Scene s;
    auto polygon = tools::makeTool("polygon");
    s.click(*polygon, { 1000, 1000, 0 });
    polygon->pointerMove(s.ctx, Scene::at(s.screen({ 1400, 1000, 0 })));
    REQUIRE(polygon->applyMeasurement(s.ctx, "8s"));
    REQUIRE(polygon->applyMeasurement(s.ctx, "400"));
    REQUIRE(s.mesh.faces().size() == 1);
    CHECK(s.mesh.edges().size() == 8);
    geom::pushPull(s.mesh, s.mesh.faces().keys().front(), -500);
    CHECK(std::none_of(s.mesh.edges().values().begin(), s.mesh.edges().values().end(),
        [](const geom::Edge& e) { return e.soft; }));

    Scene a;
    auto arc = tools::makeTool("arc");
    a.click(*arc, { 0, 0, 0 });
    a.click(*arc, { 2000, 0, 0 });
    arc->pointerMove(a.ctx, Scene::at(a.screen({ 1000, 300, 0 })));
    REQUIRE(arc->applyMeasurement(a.ctx, "500"));
    CHECK(a.mesh.edges().size() == 12);
    CHECK(a.mesh.faces().empty());
    REQUIRE(a.mesh.findVertexNear({ 0, 0, 0 }, 1e-6));
    REQUIRE(a.mesh.findVertexNear({ 2000, 0, 0 }, 1e-6));
    CHECK(a.mesh.findVertexNear({ 1000, 500, 0 }, 1e-6)); // vârful din mijloc, pe săgeată
}

TEST_CASE("Unelte pe model: selectarea unei componente, copii unice cu Move+Option, dublu-click intră", "[core][tools][model]")
{
    model::Model m;
    commands::CommandStack stack { m };
    stack.setValidation(true, geom::ValidationLevel::Full);
    stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1500, 500, 300 }, 0, 0, 9000 }));
    tools::ToolRequests requests;
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    ctx.requests = &requests;
    const auto at = [&](Vec3d p) {
        const auto s = camera.project(p);
        REQUIRE(s);
        return *s;
    };

    // O cutie făcută componentă.
    stack.run("Box", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, 0, 0 }, { 600, 0, 0 }, { 600, 600, 0 }, { 0, 600, 0 } });
        geom::pushPull(mesh, mesh.faces().keys().front(), -600);
    });
    geom::Selection everything;
    everything.faces.insert(m.mesh(m.root()).faces().keys().begin(), m.mesh(m.root()).faces().keys().end());
    model::InstanceId box;
    stack.run("Make Component", [&](model::Model& model) { box = model::makeGroup(model, model.root(), everything, true); });
    REQUIRE(box);

    // Click pe ea: se selectează instanța întreagă.
    auto select = tools::makeTool("select");
    const tools::PointerEvent onTop { at({ 300, 300, 600 }), {}, 1 };
    select->pointerMove(ctx, onTop);
    select->pointerDown(ctx, onTop);
    select->pointerUp(ctx, onTop);
    REQUIRE(selection.instances.size() == 1);
    CHECK(*selection.instances.begin() == box);

    // Move + Option, 1000 pe axa roșie: o copie unică (definiție proprie, aceeași geometrie; doar ⌥D și Array leagă).
    auto move = tools::makeTool("move");
    tools::Modifiers option;
    option.alt = true;
    const tools::PointerEvent grab { at({ 600, 0, 600 }), option, 1 };
    move->pointerMove(ctx, grab);
    move->pointerDown(ctx, grab);
    move->pointerUp(ctx, grab);
    move->pointerMove(ctx, { at({ 1300, 0, 600 }), option, 1 });
    REQUIRE(move->applyMeasurement(ctx, "1000"));
    const model::DefinitionId def = m.instance(box).definition;
    CHECK(m.instancesOf(def).size() == 1);
    REQUIRE(m.childrenOf(m.root()).size() == 2);
    const model::InstanceId copy = m.childrenOf(m.root()).back();
    CHECK(m.instance(copy).definition != def);
    CHECK(m.mesh(m.instance(copy).definition).faces().size() == m.mesh(def).faces().size());
    CHECK(glm::length(m.instance(copy).transform.applyToPoint({ 0, 0, 0 }) - Vec3d { 1000, 0, 0 }) < 1e-6);

    // „x3”: încă două copii, tot unice.
    REQUIRE(move->applyMeasurement(ctx, "x3"));
    CHECK(m.childrenOf(m.root()).size() == 4);
    CHECK(m.instancesOf(def).size() == 1);

    // Dublu-click pe o copie cere intrarea în ea.
    const tools::PointerEvent onCopy { at({ 1300, 300, 600 }), {}, 2 };
    select->pointerMove(ctx, onCopy);
    select->pointerDown(ctx, onCopy);
    select->pointerUp(ctx, onCopy);
    CHECK(requests.enter);
}

TEST_CASE("Unelte: Paint Bucket vopsește, cu Cmd tot ce e conectat, cu Option ia materialul", "[core][tools][paint]")
{
    model::Model m;
    commands::CommandStack stack { m };
    stack.setValidation(true, geom::ValidationLevel::Full);
    stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 300, 300, 300 }, 0, 0, 6000 }));
    tools::ToolRequests requests;
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    ctx.requests = &requests;
    stack.run("Box", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, 0, 0 }, { 600, 0, 0 }, { 600, 600, 0 }, { 0, 600, 0 } });
        geom::pushPull(mesh, mesh.faces().keys().front(), -600);
    });
    const auto at = [&](Vec3d p) { return *camera.project(p); };
    const model::Material wood { "Lemn", { 176, 132, 86, 255 }, 0.7f, 0.0f };
    ctx.libraryMaterial = &wood;

    auto paint = tools::makeTool("paint");
    paint->pointerDown(ctx, { at({ 300, 300, 600 }), {}, 1 });
    REQUIRE(requests.addedMaterial);
    CHECK(m.materials().size() == 1);
    int painted = 0;
    for (const geom::Face& f : m.mesh(m.root()).faces().values()) {
        painted += f.material == requests.addedMaterial ? 1 : 0;
    }
    CHECK(painted == 1);

    // Cu materialul din model, Cmd vopsește toată cutia.
    ctx.libraryMaterial = nullptr;
    ctx.paintMaterial = requests.addedMaterial;
    tools::Modifiers cmd;
    cmd.command = true;
    paint->pointerDown(ctx, { at({ 600, 300, 300 }), cmd, 1 });
    painted = 0;
    for (const geom::Face& f : m.mesh(m.root()).faces().values()) {
        painted += f.material == requests.addedMaterial ? 1 : 0;
    }
    CHECK(painted == 6);

    // Pipeta.
    tools::Modifiers option;
    option.alt = true;
    requests = {};
    paint->pointerDown(ctx, { at({ 300, 300, 600 }), option, 1 });
    REQUIRE(requests.sampledMaterial);
    CHECK(*requests.sampledMaterial == ctx.paintMaterial);
}

TEST_CASE("Unelte: Tape Measure lasă ghidaje (de care se prinde inferența), cote, text, secțiune", "[core][tools][measure]")
{
    model::Model m;
    commands::CommandStack stack { m };
    stack.setValidation(true, geom::ValidationLevel::Full);
    stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Top, { { 1000, 1000, 0 }, 0, 0, 6000 }));
    tools::ToolRequests requests;
    std::vector<inference::GuideRef> guides;
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    ctx.requests = &requests;
    const auto refreshGuides = [&] {
        guides.clear();
        for (const model::Guide& g : m.guides().values()) {
            guides.push_back({ g.point, g.direction });
        }
        ctx.guides = guides;
    };
    const auto at = [&](Vec3d p) { return *camera.project(p); };
    const auto click = [&](tools::Tool& t, Vec3d p) {
        const tools::PointerEvent e { at(p), {}, 1 };
        t.pointerMove(ctx, e);
        t.pointerDown(ctx, e);
        t.pointerUp(ctx, e);
    };
    stack.run("Linie", [](geom::Mesh& mesh) { geom::drawLine(mesh, { 0, 0, 0 }, { 2000, 0, 0 }); });

    // De pe muchie, 600 în sus pe verde: o linie de construcție paralelă cu muchia, la y = 600.
    auto tape = tools::makeTool("tape");
    click(*tape, { 700, 0, 0 });
    tape->pointerMove(ctx, { at({ 700, 400, 0 }), {}, 1 });
    REQUIRE(tape->applyMeasurement(ctx, "600"));
    REQUIRE(m.guides().size() == 1);
    const model::Guide& g = m.guides().values().front();
    CHECK(g.isLine());
    CHECK(std::abs(g.point.y - 600) < 1e-6);
    CHECK(std::abs(std::abs(g.direction.x) - 1) < 1e-9);

    // Linia se prinde de ghidaj.
    refreshGuides();
    auto line = tools::makeTool("line");
    line->pointerMove(ctx, { at({ 1500, 603, 0 }), {}, 1 });
    CHECK(line->overlay().marker->label == "On Guide");

    // O cotă și un text.
    auto dim = tools::makeTool("dimension");
    click(*dim, { 0, 0, 0 });
    click(*dim, { 2000, 0, 0 });
    click(*dim, { 1000, -400, 0 });
    REQUIRE(m.annotations().size() == 1);
    CHECK(std::abs(m.annotations().values().front().offset.y + 400) < 30);

    auto text = tools::makeTool("text");
    click(*text, { 1000, 0, 0 });
    REQUIRE(m.annotations().size() == 2);
    CHECK(requests.editAnnotation);

    // Secțiune pe sol.
    auto section = tools::makeTool("section");
    click(*section, { 500, 500, 0 });
    REQUIRE(m.sections().size() == 1);
    CHECK(m.sections().values().front().active);
    CHECK(m.sections().values().front().normal.z > 0.99);
}

TEST_CASE("Unelte: Section Plane pe fața laterală a unei cutii e vertical", "[core][tools][measure]")
{
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1000, 750, 600 }, 0, 0, 7000 }));
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    stack.run("Box", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 1500, 0 }, { 0, 1500, 0 } });
        geom::pushPull(mesh, mesh.faces().keys().front(), -1200);
    });
    auto section = tools::makeTool("section");
    // Fețele vizibile din vederea izometrică: cea de sus și două laterale.
    for (Vec3d p : { Vec3d { 2000, 800, 600 }, Vec3d { 1000, 0, 600 }, Vec3d { 1000, 1500, 600 }, Vec3d { 0, 800, 600 } }) {
        const auto s = camera.project(p);
        INFO(p.x << "," << p.y << "," << p.z);
        section->pointerMove(ctx, { *s, {}, 1 });
        UNSCOPED_INFO("marcaj: " << section->overlay().marker->label);
    }
    const auto s = camera.project({ 1000, 0, 600 });
    section->pointerMove(ctx, { *s, {}, 1 });
    section->pointerDown(ctx, { *s, {}, 1 });
    REQUIRE(m.sections().size() == 1);
    const auto& sec = m.sections().values().front();
    INFO("n = " << sec.normal.x << "," << sec.normal.y << "," << sec.normal.z << " p = " << sec.point.x << "," << sec.point.y << "," << sec.point.z);
    CHECK(std::abs(sec.normal.z) < 1e-6);
}

TEST_CASE("Paint Bucket: o față văzută din spate primește materialul pe verso", "[core][tools][paint]")
{
    model::Model m;
    geom::Mesh& mesh = m.mesh(m.root());
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 2000, 0 }, { 0, 2000, 0 } });
    const geom::FaceId face = mesh.faces().keys().front();
    const model::MaterialId red = m.addMaterial({ "Roșu", { 220, 40, 40, 255 }, 0.8f, 0.0f });
    commands::CommandStack stack(m);
    geom::Selection selection;
    // Camera de sus: fața de pe sol are normala în jos (ca în SketchUp), deci de sus vedem versoul.
    view::Camera camera;
    camera.setViewportSize(800, 600);
    camera.setPose(view::standardViewPose(view::StandardView::Top, { { 1000, 1000, 0 }, 0, 0, 5000 }));
    tools::ToolContext ctx { mesh, stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    ctx.paintMaterial = red;
    auto paint = tools::makeTool("paint");
    const auto s = camera.project({ 1000, 1000, 0 });
    REQUIRE(s);
    tools::PointerEvent e;
    e.screen = *s;
    paint->pointerDown(ctx, e);
    const bool up = mesh.face(face).plane.normal.z > 0.0;
    CHECK(mesh.face(face).material == (up ? red : model::MaterialId {}));
    CHECK(mesh.face(face).backMaterial == (up ? model::MaterialId {} : red));
}

TEST_CASE("Tag-uri pe forme: o față cu tag ascuns nu se mai alege", "[core][view][tags]")
{
    Mesh mesh;
    geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 2000, 0 }, { 0, 2000, 0 } });
    const geom::FaceId face = mesh.faces().keys().front();
    const model::TagId walls { 7 };
    mesh.setFaceTag(face, walls);
    for (geom::EdgeId e : mesh.edges().keys()) {
        mesh.setEdgeTag(e, walls);
    }
    view::Camera camera;
    camera.setViewportSize(800, 600);
    camera.setPose(view::standardViewPose(view::StandardView::Top, { { 1000, 1000, 0 }, 0, 0, 5000 }));
    const auto center = camera.project({ 1000, 1000, 0 });
    REQUIRE(center);
    CHECK(view::pick(mesh, camera, *center).kind == view::PickResult::Kind::Face);
    view::setHiddenTags({ walls });
    CHECK_FALSE(view::pick(mesh, camera, *center).hit());
    CHECK(view::isHidden(mesh, mesh.vertices().keys().front()));
    view::setHiddenTags({});
    CHECK(view::pick(mesh, camera, *center).hit());
}

TEST_CASE("Poziționează textura: tragerea mută textura cu cursorul, Shift o rotește în jurul punctului apăsat", "[core][tools][textures]")
{
    model::Model m;
    commands::CommandStack stack(m);
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Top, { { 0, 0, 0 }, 0, 0, 6000 }));
    geom::Mesh& mesh = m.mesh(m.root());
    geom::drawPolygon(mesh, { { -2000, -2000, 0 }, { 2000, -2000, 0 }, { 2000, 2000, 0 }, { -2000, 2000, 0 } });
    REQUIRE(mesh.faces().size() == 1);
    const geom::FaceId face = mesh.faces().keys().front();
    tools::ToolContext ctx { mesh, stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    auto tool = tools::makeTool("texture");
    REQUIRE(tool);

    const auto event = [&](Vec3d p, bool shift = false) {
        tools::PointerEvent e;
        e.screen = *camera.project(p);
        e.modifiers.shift = shift;
        return e;
    };
    tool->pointerMove(ctx, event({ 0, 0, 0 }));
    tool->pointerDown(ctx, event({ 0, 0, 0 }));
    tool->pointerMove(ctx, event({ 300, 0, 0 }));
    tool->pointerUp(ctx, event({ 300, 0, 0 }));
    const geom::TextureTransform moved = mesh.face(face).texture;
    CHECK(std::hypot(moved.offset.x, moved.offset.y) == Approx(300.0).margin(1.0));

    // Rotire în jurul punctului apăsat: punctul de textură de acolo nu se mută.
    const Vec3d pivot { 500, 500, 0 };
    const math::Vec2d before = model::textureCoordinates(mesh.face(face).plane.normal, pivot, mesh.face(face).texture, { 1000, 1000 });
    tool->pointerMove(ctx, event(pivot, true));
    tool->pointerDown(ctx, event(pivot, true));
    tool->pointerMove(ctx, event({ 800, 500, 0 }, true));
    tool->pointerUp(ctx, event({ 800, 500, 0 }, true));
    CHECK(mesh.face(face).texture.rotation != Approx(0.0));
    const math::Vec2d after = model::textureCoordinates(mesh.face(face).plane.normal, pivot, mesh.face(face).texture, { 1000, 1000 });
    CHECK(after.x == Approx(before.x).margin(1e-3));
    CHECK(after.y == Approx(before.y).margin(1e-3));

    // Două trageri = doi pași de undo.
    REQUIRE(stack.undo());
    REQUIRE(stack.undo());
    CHECK(mesh.face(face).texture.isIdentity());
}

TEST_CASE("Unelte: o muchie redesenată reface fața ștearsă", "[core][tools]")
{
    Scene s;
    s.stack.run("Pătrat", [](Mesh& mesh) { geom::drawPolygon(mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } }); });
    REQUIRE(s.mesh.faces().size() == 1);
    s.stack.run("Șterge fața", [](Mesh& mesh) { geom::eraseFace(mesh, mesh.faces().keys().front()); });
    REQUIRE(s.mesh.faces().empty());
    auto line = tools::makeTool("line");
    s.click(*line, { 0, 0, 0 });
    s.click(*line, { 1000, 0, 0 });
    CHECK(s.mesh.faces().size() == 1);
    CHECK(s.mesh.edges().size() == 4);
}

TEST_CASE("Unelte: Move pornește dintr-un colț sau mijloc al obiectului și se prinde de colțul altuia", "[core][tools][model]")
{
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1500, 500, 300 }, 0, 0, 9000 }));
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    const auto at = [&](Vec3d p) {
        const auto s = camera.project(p);
        REQUIRE(s);
        return *s;
    };
    // Două cutii-obiect: A (0..600) și B (2000..2600).
    const auto makeBox = [&](double x0) {
        stack.run("Box", [&](geom::Mesh& mesh) {
            geom::drawPolygon(mesh, { { x0, 0, 0 }, { x0 + 600, 0, 0 }, { x0 + 600, 600, 0 }, { x0, 600, 0 } });
        });
        geom::Selection faces;
        for (geom::FaceId f : m.mesh(m.root()).faces().keys()) {
            faces.faces.insert(f);
        }
        stack.run("Push", [&](geom::Mesh& mesh) { geom::pushPull(mesh, *faces.faces.begin(), -600); });
        geom::Selection all;
        all.faces.insert(m.mesh(m.root()).faces().keys().begin(), m.mesh(m.root()).faces().keys().end());
        model::InstanceId id;
        stack.run("Group", [&](model::Model& model) { id = model::makeGroup(model, model.root(), all, false); });
        return id;
    };
    const model::InstanceId a = makeBox(0);
    const model::InstanceId b = makeBox(2000);

    auto move = tools::makeTool("move");
    // Peste A: 8 colțuri + 12 mijloace; cel de lângă cursor e „fierbinte”.
    move->pointerMove(ctx, { at({ 300, 300, 600 }), {}, 1 });
    CHECK(move->overlay().grips.size() == 20);
    // Mijlocul muchiei de sus din față (300, 0, 600), apoi colțul de jos al lui B (2000, 0, 0).
    const tools::PointerEvent grab { at({ 300, 0, 600 }) + Vec2d { 3, -2 }, {}, 1 };
    move->pointerMove(ctx, grab);
    const tools::Overlay hot = move->overlay();
    REQUIRE(hot.marker);
    CHECK(hot.marker->label == "Mijlocul muchiei");
    move->pointerDown(ctx, grab);
    move->pointerUp(ctx, grab);
    const tools::PointerEvent drop { at({ 2000, 0, 0 }) + Vec2d { -4, 3 }, {}, 1 };
    move->pointerMove(ctx, drop);
    move->pointerDown(ctx, drop);
    move->pointerUp(ctx, drop);
    // Mijlocul (300, 0, 600) al lui A a ajuns în (2000, 0, 0).
    CHECK(glm::length(m.instance(a).transform.applyToPoint({ 300, 0, 600 }) - Vec3d { 2000, 0, 0 }) < 1e-6);
    CHECK(m.instances().contains(b));
}

TEST_CASE("Unelte: Follow Me — traseul selectat, click pe profil", "[core][tools][sweep]")
{
    Scene s;
    s.camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 500, 500, 50 }, 0, 0, 5000 }));
    std::set<geom::EdgeId> path;
    s.stack.run("Desen", [&](Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, -50, 0 }, { 0, 50, 0 }, { 0, 50, 100 }, { 0, -50, 100 } });
        for (geom::EdgeId e : geom::drawLine(mesh, { 0, 0, 0 }, { 1000, 0, 0 }).edges) {
            path.insert(e);
        }
        for (geom::EdgeId e : geom::drawLine(mesh, { 1000, 0, 0 }, { 1000, 1000, 0 }).edges) {
            path.insert(e);
        }
    });
    auto tool = tools::makeTool("followme");
    REQUIRE(tool);
    // Fără traseu: un click pe profil nu face nimic și spune ce lipsește.
    s.click(*tool, { 0, 0, 50 });
    CHECK(s.mesh.faces().size() == 1);
    CHECK(tool->status().find("traseul") != std::string::npos);
    s.selection.edges = path;
    s.click(*tool, { 0, 0, 50 });
    CHECK(s.mesh.faces().size() >= 10);
    CHECK(s.stack.undoName() == "Follow Me");
    CHECK_FALSE(s.selection.faces.empty());
}

namespace {

// O cutie-obiect 600 × 600 × 600 cu colțul minim în (x0, 0, 0), în rădăcina modelului.
model::InstanceId boxObject(model::Model& m, commands::CommandStack& stack, double x0)
{
    stack.run("Box", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { x0, 0, 0 }, { x0 + 600, 0, 0 }, { x0 + 600, 600, 0 }, { x0, 600, 0 } });
    });
    const geom::FaceId bottom = *m.mesh(m.root()).faces().keys().begin();
    stack.run("Push", [&](geom::Mesh& mesh) { geom::pushPull(mesh, bottom, -600); });
    geom::Selection all;
    all.faces.insert(m.mesh(m.root()).faces().keys().begin(), m.mesh(m.root()).faces().keys().end());
    model::InstanceId id;
    stack.run("Group", [&](model::Model& model) { id = model::makeGroup(model, model.root(), all, false); });
    return id;
}

} // namespace

TEST_CASE("Unelte: Rotate și Scale pornesc din pivotul selecției (pivotul în colț ține muchia pe loc)", "[core][tools][transform]")
{
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1300, 300, 300 }, 0, 0, 7000 }));
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    const auto at = [&](Vec3d p, tools::Modifiers mods = {}) {
        const auto s = camera.project(p);
        REQUIRE(s);
        return tools::PointerEvent { *s, mods, 1 };
    };
    const auto click = [&](tools::Tool& tool, Vec3d p, tools::Modifiers mods = {}) {
        tool.pointerMove(ctx, at(p, mods));
        tool.pointerDown(ctx, at(p, mods));
        tool.pointerUp(ctx, at(p, mods));
    };
    const model::InstanceId box = boxObject(m, stack, 1000);
    // Pivotul în colțul minim (1000, 0, 0): distanța de la colț e zero.
    stack.run("Pivot", [&](model::Model& model) {
        model::Placement p = model::placementOf(model, box);
        p.pivot = Vec3d { 0.0 };
        model::setPlacement(model, box, p);
    });
    selection.instances.insert(box);
    ctx.pivot = model::pivotPoint(m, box);
    REQUIRE(glm::length(*ctx.pivot - Vec3d { 1000, 0, 0 }) < 1e-9);
    const math::Transform before = m.instance(box).transform;
    const auto world = [&](Vec3d local) { return m.instance(box).transform.applyToPoint(before.inverse().applyToPoint(local)); };

    SECTION("Rotate pe Y: centrul e deja pivotul, primul click alege referința")
    {
        auto rotate = tools::makeTool("rotate");
        rotate->pointerMove(ctx, at({ 1300, 0, 300 }));
        REQUIRE(rotate->key(ctx, { tools::Key::Left, {} })); // axa verde
        // Raportorul stă pe pivot înainte de orice click.
        const std::vector<Vec3d> points = rotate->overlay().points;
        CHECK(std::any_of(points.begin(), points.end(), [](const Vec3d& p) { return glm::length(p - Vec3d { 1000, 0, 0 }) < 1e-9; }));
        click(*rotate, { 1600, 0, 0 }); // referința: colțul de pe axa roșie
        rotate->pointerMove(ctx, at({ 1500, 0, 400 }));
        REQUIRE(rotate->applyMeasurement(ctx, "30"));
        CHECK(stack.undoName() == "Rotate");
        // Muchia din colțul pivotului, pe axa verde, nu se mișcă.
        CHECK(glm::length(world({ 1000, 0, 0 }) - Vec3d { 1000, 0, 0 }) < 1e-6);
        CHECK(glm::length(world({ 1000, 600, 0 }) - Vec3d { 1000, 600, 0 }) < 1e-6);
        // Colțul opus de pe axa roșie s-a ridicat pe arc, la 600 mm de pivot.
        const Vec3d far = world({ 1600, 0, 0 });
        CHECK(far.y == Approx(0).margin(1e-6));
        CHECK(glm::length(far - Vec3d { 1000, 0, 0 }) == Approx(600));
        CHECK(std::abs(far.z) == Approx(300));
    }

    SECTION("Rotate cu Option: primul click alege alt centru, care se prinde de colțurile obiectului")
    {
        auto rotate = tools::makeTool("rotate");
        rotate->pointerMove(ctx, at({ 1600, 600, 600 }));
        REQUIRE(rotate->key(ctx, { tools::Key::Up, {} })); // axa albastră
        const tools::PointerEvent near { at({ 1600, 600, 600 }).screen + Vec2d { 4, -3 }, { false, false, true }, 1 };
        rotate->pointerMove(ctx, near);
        rotate->pointerDown(ctx, near);
        rotate->pointerUp(ctx, near);
        click(*rotate, { 1000, 600, 600 }); // referința
        rotate->pointerMove(ctx, at({ 1300, 300, 600 }));
        REQUIRE(rotate->applyMeasurement(ctx, "90"));
        // Colțul ales ca centru rămâne pe loc.
        CHECK(glm::length(world({ 1600, 600, 600 }) - Vec3d { 1600, 600, 600 }) < 1e-6);
        CHECK(glm::length(world({ 1000, 0, 0 }) - Vec3d { 1000, 0, 0 }) > 100.0);
    }

    SECTION("Scale cu Option: punctul fix e pivotul")
    {
        auto scale = tools::makeTool("scale");
        const tools::PointerEvent grab = at({ 1600, 600, 600 }, { false, false, true });
        scale->pointerMove(ctx, grab);
        scale->pointerDown(ctx, grab);
        scale->pointerUp(ctx, grab);
        scale->pointerMove(ctx, at({ 1700, 700, 700 }, { false, false, true }));
        REQUIRE(scale->applyMeasurement(ctx, "2"));
        CHECK(glm::length(world({ 1000, 0, 0 }) - Vec3d { 1000, 0, 0 }) < 1e-6);
        CHECK(glm::length(world({ 1600, 600, 600 }) - Vec3d { 2200, 1200, 1200 }) < 1e-6);
    }
}

TEST_CASE("Unelte: camerele și luminile se mută, se rotesc și se scalează ca obiectele (cu prindere)", "[core][tools][transform]")
{
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Top, { { 0, 0, 0 }, 0, 0, 12000 }));
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    const auto at = [&](Vec3d p, tools::Modifiers mods = {}) {
        const auto s = camera.project(p);
        REQUIRE(s);
        return tools::PointerEvent { *s, mods, 1 };
    };
    const auto click = [&](tools::Tool& tool, Vec3d p) {
        tool.pointerMove(ctx, at(p));
        tool.pointerDown(ctx, at(p));
        tool.pointerUp(ctx, at(p));
    };
    model::CameraObject c;
    c.position = { 0, 0, 1500 };
    c.target = { 0, 3000, 1500 };
    model::CameraId cam;
    stack.run("Cameră", [&](model::Model& model) { cam = model.addCamera(c); });
    ctx.object = model::SceneObjectRef { cam, {} };
    ctx.pivot = c.position;

    SECTION("Move: din poziția camerei (punct de prindere) până într-un punct; ținta vine cu ea")
    {
        auto move = tools::makeTool("move");
        move->pointerMove(ctx, at({ 0, 0, 1500 }));
        REQUIRE(move->overlay().grips.size() >= 2); // poziția și ținta
        click(*move, { 0, 0, 1500 });
        click(*move, { 2000, 1000, 1500 }); // un punct liber: în planul punctului de pornire
        const model::CameraObject& moved = m.cameras().at(cam);
        CHECK(moved.position.x == Approx(2000));
        CHECK(moved.position.y == Approx(1000));
        CHECK(moved.target.y == Approx(4000)); // ținta s-a mutat la fel
        CHECK(stack.undoName() == "Move");
    }

    SECTION("Rotate: în jurul poziției camerei, 90° pe Z → ținta se rotește, camera rămâne")
    {
        auto rotate = tools::makeTool("rotate");
        rotate->pointerMove(ctx, at({ 0, 3000, 1500 }));
        REQUIRE(rotate->key(ctx, { tools::Key::Up, {} })); // axa albastră
        click(*rotate, { 0, 3000, 1500 }); // referința: spre țintă
        rotate->pointerMove(ctx, at({ -3000, 0, 1500 }));
        REQUIRE(rotate->applyMeasurement(ctx, "90"));
        const model::CameraObject& r = m.cameras().at(cam);
        CHECK(glm::length(r.position - Vec3d { 0, 0, 1500 }) < 1e-6);
        CHECK(glm::length(r.target - Vec3d { -3000, 0, 1500 }) < 1e-6);
        CHECK(r.roll == Approx(0.0).margin(1e-6));
    }

    SECTION("Scale: în perspectivă doar mărimea gizmo-ului; ortografic, scara cadrului")
    {
        model::SceneObjectRef ref { cam, {} };
        stack.run("Scale", [&](model::Model& model) { model::scaleSceneObject(model, ref, Vec3d { 2.0 }); });
        CHECK(m.cameras().at(cam).displaySize == Approx(2.0));
        CHECK(glm::length(m.cameras().at(cam).position - c.position) < 1e-9);
        stack.run("Orto", [&](model::Model& model) {
            model::CameraObject o = model.cameras().at(cam);
            o.orthographic = true;
            model.setCamera(cam, o);
        });
        stack.run("Scale", [&](model::Model& model) { model::scaleSceneObject(model, ref, Vec3d { 0.5 }); });
        CHECK(m.cameras().at(cam).orthoScale == Approx(5000.0));
    }

    SECTION("Roll: o rotație în jurul direcției de privire înclină camera")
    {
        model::SceneObjectRef ref { cam, {} };
        stack.run("Roll", [&](model::Model& model) {
            model::transformSceneObject(model, ref, math::Transform::rotation(c.position, { 0, 1, 0 }, glm::radians(30.0)));
        });
        CHECK(std::abs(m.cameras().at(cam).roll) == Approx(30.0));
        CHECK(glm::length(m.cameras().at(cam).target - c.target) < 1e-6);
    }

    SECTION("Luminile: direcția se rotește, panoul se scalează pe laturi, spotul își deschide conul")
    {
        model::Light l;
        l.kind = model::Light::Kind::Area;
        l.position = { 0, 0, 2000 };
        l.direction = { 0, 0, -1 };
        model::LightId light;
        stack.run("Lumină", [&](model::Model& model) { light = model.addLight(l); });
        const model::SceneObjectRef ref { {}, light };
        stack.run("Rotate", [&](model::Model& model) {
            model::transformSceneObject(model, ref, math::Transform::rotation(l.position, { 1, 0, 0 }, glm::radians(90.0)));
        });
        CHECK(glm::length(m.lights().at(light).direction - Vec3d { 0, 1, 0 }) < 1e-9);
        stack.run("Scale", [&](model::Model& model) { model::scaleSceneObject(model, ref, Vec3d { 2.0, 0.5, 1.0 }); });
        CHECK(m.lights().at(light).width == Approx(2000.0));
        CHECK(m.lights().at(light).height == Approx(500.0));
        stack.run("Spot", [&](model::Model& model) {
            model::Light s = model.lights().at(light);
            s.kind = model::Light::Kind::Spot;
            model.setLight(light, s);
        });
        stack.run("Scale", [&](model::Model& model) { model::scaleSceneObject(model, ref, Vec3d { 1.5 }); });
        CHECK(m.lights().at(light).cone == Approx(67.5));
    }
}

TEST_CASE("Unelte: Rotate — referința și unghiul se prind exact de axa inferată (linia nu iese înclinată)", "[core][tools][transform]")
{
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1300, 300, 300 }, 0, 0, 7000 }));
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    const model::InstanceId box = boxObject(m, stack, 1000);
    selection.instances.insert(box);
    ctx.pivot = Vec3d { 1000, 0, 0 };
    const auto at = [&](Vec3d p) {
        const auto s = camera.project(p);
        REQUIRE(s);
        return tools::PointerEvent { *s, {}, 1 };
    };
    auto rotate = tools::makeTool("rotate");
    rotate->pointerMove(ctx, at({ 1500, 0, 0 }));
    REQUIRE(rotate->key(ctx, { tools::Key::Up, {} })); // rotație pe Z, planul z = 0 prin pivot
    // Cursorul puțin lângă axa roșie din pivot: inferența o arată, iar referința trebuie să cadă pe ea.
    const tools::PointerEvent near = at({ 1500, 12, 0 });
    rotate->pointerMove(ctx, near);
    rotate->pointerDown(ctx, near);
    rotate->pointerUp(ctx, near);
    const auto referenceLine = [&]() -> std::optional<tools::OverlayLine> {
        for (const tools::OverlayLine& l : rotate->overlay().lines) {
            if (l.dashed && glm::length(l.a - Vec3d { 1000, 0, 0 }) < 1e-9) {
                return l;
            }
        }
        return std::nullopt;
    };
    const auto ref = referenceLine();
    REQUIRE(ref);
    CHECK(std::abs(ref->b.y) < 1e-6); // exact pe axa X
    // Unghiul: cursorul lângă axa verde → exact 90°, cu linia de final pe axă.
    rotate->pointerMove(ctx, at({ 1000, 500, 0 }) );
    const tools::PointerEvent green = at({ 1009, 500, 0 });
    rotate->pointerMove(ctx, green);
    REQUIRE(rotate->measurement());
    CHECK(rotate->measurement()->value == "90°");
}

TEST_CASE("Unelte: tragerea pivotului se prinde ca la Move (colțul altui obiect, originea, centrul unei fețe)", "[core][tools][transform]")
{
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1800, 300, 300 }, 0, 0, 9000 }));
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    tools::ToolRequests requests;
    ctx.requests = &requests;
    const model::InstanceId a = boxObject(m, stack, 1000);
    boxObject(m, stack, 3000);
    selection.instances.insert(a);
    const Vec3d center = model::pivotPoint(m, a);
    ctx.pivot = center;
    // Câțiva pixeli lângă punct, ca la o tragere reală.
    const auto near = [&](Vec3d p) {
        const auto s = camera.project(p);
        REQUIRE(s);
        return tools::PointerEvent { *s + Vec2d { 3, -2 }, {}, 1 };
    };
    auto pivot = tools::makeTool("pivot");
    REQUIRE(pivot);

    SECTION("colțul celuilalt obiect")
    {
        const Vec3d corner { 3600, 0, 600 };
        pivot->pointerDown(ctx, near(center));
        pivot->pointerMove(ctx, near((center + corner) * 0.5));
        pivot->pointerMove(ctx, near(corner));
        REQUIRE(requests.pivot);
        CHECK(glm::length(*requests.pivot - corner) < 1e-6); // previzualizarea
        CHECK_FALSE(requests.pivotDone);
        pivot->pointerUp(ctx, near(corner));
        CHECK(requests.pivotDone);
        REQUIRE(requests.pivot);
        CHECK(glm::length(*requests.pivot - corner) < 1e-6);
    }
    SECTION("originea")
    {
        pivot->pointerDown(ctx, near(center));
        pivot->pointerMove(ctx, near({ 0, 0, 0 }));
        pivot->pointerUp(ctx, near({ 0, 0, 0 }));
        CHECK(requests.pivotDone);
        REQUIRE(requests.pivot);
        CHECK(glm::length(*requests.pivot) < 1e-6);
    }
    SECTION("centrul feței de sus a propriului obiect")
    {
        // De sus: centrul feței văzute e un punct obișnuit (centrul cutiei și fața de jos, sub el, sunt estompate).
        camera.setPose(view::standardViewPose(view::StandardView::Top, { { 1800, 300, 300 }, 0, 0, 9000 }));
        const Vec3d top { 1300, 300, 600 };
        ctx.pivot = Vec3d { 1000, 0, 0 };
        pivot->pointerDown(ctx, near({ 1000, 0, 0 }));
        pivot->pointerMove(ctx, near(top));
        pivot->pointerUp(ctx, near(top));
        REQUIRE(requests.pivot);
        CHECK(glm::length(*requests.pivot - top) < 1e-6);
    }
    SECTION("Esc anulează, un click fără tragere nu schimbă nimic")
    {
        pivot->pointerDown(ctx, near(center));
        pivot->pointerMove(ctx, near({ 3600, 0, 600 }));
        CHECK(pivot->key(ctx, { tools::Key::Escape, {} }));
        CHECK(requests.pivotDone);
        CHECK_FALSE(requests.pivot);
        requests = {};
        pivot->pointerDown(ctx, near(center));
        pivot->pointerUp(ctx, near(center));
        CHECK(requests.pivotDone);
        CHECK_FALSE(requests.pivot);
    }
}

TEST_CASE("Unelte: guma pe o linie care împarte o față o lasă întreagă; deleteSelection cu fețe o ia", "[core][tools][erase]")
{
    Scene s;
    s.stack.run("Rect", [&](Mesh& mesh) { geom::drawPolygon(mesh, { { 0, 0, 0 }, { 2000, 0, 0 }, { 2000, 2000, 0 }, { 0, 2000, 0 } }); });
    s.stack.run("Line", [&](Mesh& mesh) { geom::drawLine(mesh, { 1000, 0, 0 }, { 1000, 2000, 0 }); });
    REQUIRE(s.mesh.faces().size() == 2);
    SECTION("guma")
    {
        auto eraser = tools::makeTool("eraser");
        s.click(*eraser, { 1000, 1000, 0 });
        CHECK(s.mesh.faces().size() == 1);
        CHECK(s.mesh.edges().size() == 4);
    }
    SECTION("⌫: deleteSelection păstrând fețele")
    {
        s.selection = tools::selectionAt(s.ctx, s.screen({ 1000, 1000, 0 }));
        REQUIRE(s.selection.edges.size() == 1);
        CHECK(tools::deleteSelection(s.ctx, true));
        CHECK(s.mesh.faces().size() == 1);
    }
    SECTION("Delete: cu fețe")
    {
        s.selection = tools::selectionAt(s.ctx, s.screen({ 1000, 1000, 0 }));
        CHECK(tools::deleteSelection(s.ctx, false));
        CHECK(s.mesh.faces().empty());
    }
}

namespace {

// Unealta „place” pe un model văzut de sus, cu o copie de lipit (o latură de 100 mm din origine).
struct PlaceScene {
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    tools::ToolRequests requests;
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    std::unique_ptr<tools::Tool> place = tools::makeTool("place");

    PlaceScene()
    {
        camera.setViewportSize(1000, 800);
        camera.setPose(view::standardViewPose(view::StandardView::Top, { { 1000, 1000, 0 }, 0, 0, 6000 }));
        ctx.model = &m;
        ctx.context = m.root();
        ctx.requests = &requests;
        ctx.place = tools::PlaceSource { { 0, 0, 0 }, { { { 0, 0, 0 }, { 100, 0, 0 } } } };
        REQUIRE(place);
    }

    Vec2d screen(Vec3d world) const
    {
        const auto s = camera.project(world);
        REQUIRE(s.has_value());
        return *s;
    }
};

} // namespace

TEST_CASE("Place: urmează cursorul și desenează copia", "[core][tools]")
{
    PlaceScene s;
    s.place->pointerMove(s.ctx, { s.screen({ 1000, 500, 0 }), {}, 1 });
    const tools::Overlay o = s.place->overlay();
    const bool drawn = std::any_of(o.lines.begin(), o.lines.end(), [](const tools::OverlayLine& l) {
        return glm::length(l.a - Vec3d { 1000, 500, 0 }) < 1.0 && glm::length(l.b - Vec3d { 1100, 500, 0 }) < 1.0;
    });
    CHECK(drawn);
}

TEST_CASE("Place: se prinde de colțul altui obiect", "[core][tools]")
{
    PlaceScene s;
    const model::DefinitionId d = s.m.addDefinition(model::Definition { "Cutie", false });
    geom::buildPrimitive(s.m.mesh(d), geom::Primitive::Box, { 400, 400, 400 }, 24);
    model::Instance inst;
    inst.parent = s.m.root();
    inst.definition = d;
    inst.transform = math::Transform::translation({ 1200, 1200, 0 }); // colțurile pe sol: 1000…1400
    s.m.addInstance(inst);
    s.place->pointerMove(s.ctx, { s.screen({ 1000, 1000, 400 }) + Vec2d { 3, 2 }, {}, 1 });
    REQUIRE(s.place->overlay().marker);
    CHECK(s.place->overlay().marker->label == "Colț");
    s.place->pointerDown(s.ctx, { s.screen({ 1000, 1000, 400 }) + Vec2d { 3, 2 }, {}, 1 });
    REQUIRE(s.requests.placeAt);
    CHECK(std::abs(s.requests.placeAt->x - 1000.0) < 1e-6);
    CHECK(std::abs(s.requests.placeAt->y - 1000.0) < 1e-6);
}

TEST_CASE("Place: click cere plasarea, Esc o anulează", "[core][tools]")
{
    PlaceScene s;
    s.place->pointerMove(s.ctx, { s.screen({ 600, 700, 0 }), {}, 1 });
    s.place->pointerDown(s.ctx, { s.screen({ 600, 700, 0 }), {}, 1 });
    REQUIRE(s.requests.placeAt);
    CHECK(glm::length(*s.requests.placeAt - Vec3d { 600, 700, 0 }) < 1.0);
    CHECK_FALSE(s.requests.placeCancel);
    CHECK(s.place->key(s.ctx, { tools::Key::Escape, {} }));
    CHECK(s.requests.placeCancel);
    CHECK(s.m.instances().empty()); // unealta nu scrie în model
}

TEST_CASE("Place: snap la grid", "[core][tools]")
{
    PlaceScene s;
    s.ctx.gridStep = 500.0;
    s.place->pointerMove(s.ctx, { s.screen({ 1130, 980, 0 }), {}, 1 });
    s.place->pointerDown(s.ctx, { s.screen({ 1130, 980, 0 }), {}, 1 });
    REQUIRE(s.requests.placeAt);
    CHECK(glm::length(*s.requests.placeAt - Vec3d { 1000, 1000, 0 }) < 1e-6);
}

namespace {

// Un model cu o cutie-obiect (xy centrat în `at`, înălțime 1000), camera izometrică și scena de prindere.
struct ObjectScene {
    model::Model m;
    commands::CommandStack stack { m };
    geom::Selection selection;
    view::Camera camera;
    tools::ToolRequests requests;
    inference::SnapScene scene;
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };

    ObjectScene()
    {
        stack.setValidation(true, geom::ValidationLevel::Full);
        stack.setOnInvalid([](const geom::ValidationReport& r) { FAIL(r.text()); });
        camera.setViewportSize(1000, 800);
        camera.setPose(view::standardViewPose(view::StandardView::Iso, { { 1500, 500, 300 }, 0, 0, 9000 }));
        ctx.model = &m;
        ctx.context = m.root();
        ctx.requests = &requests;
        ctx.snap = &scene;
    }

    model::InstanceId addBox(Vec3d at, double side = 1000.0)
    {
        model::Definition def { "Cutie", true };
        def.shape = "box";
        const model::DefinitionId d = m.addDefinition(def);
        geom::buildPrimitive(m.mesh(d), geom::Primitive::Box, { side, side, side }, 24);
        model::Instance inst;
        inst.parent = m.root();
        inst.definition = d;
        inst.transform = math::Transform::translation(at);
        return m.addInstance(inst);
    }

    // Cilindru cu baza la z = 0, diametru și înălțime 1000, 24 de laturi (centrat în `at` pe X, Y).
    model::InstanceId addCylinder(Vec3d at)
    {
        model::Definition def { "Cilindru", true };
        def.shape = "cylinder";
        const model::DefinitionId d = m.addDefinition(def);
        geom::buildPrimitive(m.mesh(d), geom::Primitive::Cylinder, { 1000, 1000, 1000 }, 24);
        model::Instance inst;
        inst.parent = m.root();
        inst.definition = d;
        inst.transform = math::Transform::translation(at);
        return m.addInstance(inst);
    }

    void rebuildScene(const std::set<model::InstanceId>& exclude = {}) { scene = model::snapScene(m, {}, exclude); }

    std::unique_ptr<tools::Tool> make(std::string_view id)
    {
        auto tool = tools::makeTool(id);
        REQUIRE(tool);
        return tool;
    }

    Vec2d screen(Vec3d world) const
    {
        const auto s = camera.project(world);
        REQUIRE(s.has_value());
        return *s;
    }

    void click(tools::Tool& tool, Vec3d world)
    {
        const tools::PointerEvent e { screen(world), {}, 1 };
        tool.pointerMove(ctx, e);
        tool.pointerDown(ctx, e);
        tool.pointerUp(ctx, e);
    }
};

} // namespace

TEST_CASE("Line: capătul pe colțul unui obiect nu atinge obiectul", "[core][tools][snap]")
{
    ObjectScene s;
    const model::InstanceId box = s.addBox({ 3000, 0, 0 });
    s.rebuildScene();
    const geom::Mesh before = s.m.mesh(s.m.instance(box).definition);

    auto line = tools::makeTool("line");
    const Vec3d corner { 3500, 500, 1000 };
    s.click(*line, { 0, 0, 0 });
    s.click(*line, corner);

    const geom::Mesh& root = s.m.mesh(s.m.root());
    REQUIRE(root.edges().size() == 1);
    bool found = false;
    for (geom::VertexId v : root.vertices().keys()) {
        found = found || glm::length(root.position(v) - corner) < 1e-6;
    }
    CHECK(found);
    CHECK(s.m.mesh(s.m.instance(box).definition).sameAs(before));
}

TEST_CASE("Push/Pull: distanța până în dreptul vârfului unui obiect", "[core][tools][snap]")
{
    ObjectScene s;
    s.addBox({ 3000, 0, 0 });
    s.rebuildScene();
    s.stack.run("Rect", [&](geom::Mesh& mesh) {
        geom::drawPolygon(mesh, { { 0, 0, 0 }, { 1000, 0, 0 }, { 1000, 1000, 0 }, { 0, 1000, 0 } });
    });
    REQUIRE(s.m.mesh(s.m.root()).faces().size() == 1);

    auto pp = tools::makeTool("pushpull");
    const tools::PointerEvent down { s.screen({ 500, 500, 0 }), {}, 1 };
    pp->pointerMove(s.ctx, down);
    pp->pointerDown(s.ctx, down);
    pp->pointerMove(s.ctx, tools::PointerEvent { s.screen({ 3500, 500, 1000 }) + Vec2d { 2, 0 }, {}, 1 });
    const tools::PointerEvent up { s.screen({ 3500, 500, 1000 }), {}, 1 };
    pp->pointerMove(s.ctx, up);
    pp->pointerUp(s.ctx, up);

    const geom::Mesh& root = s.m.mesh(s.m.root());
    double top = -1e9;
    for (geom::VertexId v : root.vertices().keys()) {
        top = std::max(top, root.position(v).z);
    }
    CHECK(top == Approx(1000).margin(1e-6));
}

TEST_CASE("Primitive: trei clicuri pentru cub (centru, mărime, înălțime)", "[core][tools][primitives]")
{
    ObjectScene s;
    auto cube = s.make("cube");
    s.click(*cube, { 1000, 1000, 0 });
    s.click(*cube, { 1400, 1000, 0 }); // raza 400 → latura 800
    CHECK(s.m.instances().empty()); // înălțimea abia urmează
    CHECK(cube->measurement()->label == "Înălțime");
    s.click(*cube, { 1000, 1000, 1500 });

    REQUIRE(s.selection.instances.size() == 1);
    const model::InstanceId id = *s.selection.instances.begin();
    const model::Placement p = model::placementOf(s.m, id);
    CHECK(p.size.x == Approx(800.0).margin(1e-3));
    CHECK(p.size.z == Approx(1500.0).margin(1e-3));
    CHECK(s.m.mesh(s.m.root()).faces().empty()); // geometria e în obiect, nu în model
    CHECK(model::isSolid(s.m, s.m.instance(id).definition));
    CHECK(s.m.definition(s.m.instance(id).definition).shape == "box"); // iconița din listă
}

TEST_CASE("Primitive: înălțimea în dreptul colțului altui obiect", "[core][tools][primitives]")
{
    ObjectScene s;
    s.addBox({ 3000, 0, 0 }, 2500.0);
    s.rebuildScene();
    auto cylinder = s.make("cylinder");
    s.click(*cylinder, { 0, 0, 0 });
    s.click(*cylinder, { 400, 0, 0 });
    const Vec3d corner { 4250, -1250, 2500 };
    cylinder->pointerMove(s.ctx, tools::PointerEvent { s.screen(corner) + Vec2d { 2, 0 }, {}, 1 });
    const tools::Overlay o = cylinder->overlay();
    REQUIRE(o.marker);
    CHECK(o.marker->kind == inference::Kind::Endpoint);
    // Ghidajul punctat de la colț la vârful înălțimii.
    CHECK(std::ranges::any_of(o.lines, [&](const tools::OverlayLine& l) {
        return l.dashed && glm::length(l.a - corner) < 1e-6 && glm::length(l.b - Vec3d { 0, 0, 2500 }) < 1e-6;
    }));
    s.click(*cylinder, corner);

    REQUIRE(s.selection.instances.size() == 1);
    const model::InstanceId id = *s.selection.instances.begin();
    CHECK(model::placementOf(s.m, id).size.z == Approx(2500.0).margin(1e-6));
    CHECK(s.m.definition(s.m.instance(id).definition).shape == "cylinder");
}

TEST_CASE("Primitive: sfera rămâne cu două clicuri", "[core][tools][primitives]")
{
    ObjectScene s;
    auto sphere = s.make("sphere");
    s.click(*sphere, { 1000, 1000, 0 });
    s.click(*sphere, { 1400, 1000, 0 });
    REQUIRE(s.selection.instances.size() == 1);
    const model::InstanceId id = *s.selection.instances.begin();
    const model::Placement p = model::placementOf(s.m, id);
    CHECK(p.size.x == Approx(800.0).margin(1e-3));
    CHECK(p.size.z == Approx(800.0).margin(1e-3));
    CHECK(s.m.definition(s.m.instance(id).definition).shape == "sphere");
}

TEST_CASE("Primitive: VCB pe pași", "[core][tools][primitives]")
{
    ObjectScene s;
    auto cube = s.make("cube");
    CHECK_FALSE(cube->applyMeasurement(s.ctx, "1000")); // fără centru nu e nimic de măsurat
    s.click(*cube, { 0, 0, 0 });
    cube->pointerMove(s.ctx, tools::PointerEvent { s.screen({ 300, 0, 0 }), {}, 1 });
    CHECK(cube->measurement()->label == "Lățime");
    CHECK_FALSE(cube->applyMeasurement(s.ctx, "0"));
    CHECK_FALSE(cube->applyMeasurement(s.ctx, "-5"));
    REQUIRE(cube->applyMeasurement(s.ctx, "1000"));
    CHECK(s.m.instances().empty());
    CHECK(cube->status().find("înălțimea") != std::string::npos);
    CHECK(cube->measurement()->label == "Înălțime");
    CHECK_FALSE(cube->applyMeasurement(s.ctx, "0"));
    CHECK_FALSE(cube->applyMeasurement(s.ctx, "-5"));
    REQUIRE(cube->applyMeasurement(s.ctx, "500"));
    REQUIRE(s.selection.instances.size() == 1);
    const model::Placement p = model::placementOf(s.m, *s.selection.instances.begin());
    CHECK(p.size.x == Approx(1000.0).margin(1e-6));
    CHECK(p.size.z == Approx(500.0).margin(1e-6));

    // Lățimea și înălțimea deodată, la pasul 2.
    auto other = s.make("cube");
    s.click(*other, { -2000, -2000, 0 });
    REQUIRE(other->applyMeasurement(s.ctx, "600,1500"));
    REQUIRE(s.m.instances().size() == 2);
    const model::Placement c = model::placementOf(s.m, *s.selection.instances.begin());
    CHECK(c.size.x == Approx(600.0).margin(1e-6));
    CHECK(c.size.z == Approx(1500.0).margin(1e-6));

    // Lățimea și înălțimea la pasul 3.
    auto third = s.make("cone");
    s.click(*third, { -2000, 2000, 0 });
    s.click(*third, { -1800, 2000, 0 });
    REQUIRE(third->applyMeasurement(s.ctx, "700,900"));
    const model::Placement k = model::placementOf(s.m, *s.selection.instances.begin());
    CHECK(k.size.x == Approx(700.0).margin(1e-6));
    CHECK(k.size.z == Approx(900.0).margin(1e-6));
}

TEST_CASE("Primitive: previzualizarea sferei are meridiane și paralele", "[core][tools][primitives]")
{
    ObjectScene s;
    auto sphere = s.make("sphere");
    s.click(*sphere, { 0, 0, 0 });
    sphere->pointerMove(s.ctx, tools::PointerEvent { s.screen({ 600, 0, 0 }), {}, 1 });
    const double r = 600.0; // raza bazei = raza sferei; centrul la înălțimea razei
    const tools::Overlay o = sphere->overlay();
    std::vector<tools::OverlayLine> body;
    std::ranges::copy_if(o.lines, std::back_inserter(body), [](const tools::OverlayLine& l) { return !l.dashed && !l.onTop; });
    CHECK(body.size() >= 8 * 16);
    const Vec3d center { 0, 0, r };
    for (const tools::OverlayLine& l : body) {
        CHECK(glm::length(l.a - center) == Approx(r).margin(1e-6));
        CHECK(glm::length(l.b - center) == Approx(r).margin(1e-6));
    }
}

TEST_CASE("Rectangle pe fața altui obiect", "[core][tools][snap]")
{
    ObjectScene s;
    s.addBox({ 3000, 0, 0 });
    s.rebuildScene();

    auto rect = tools::makeTool("rectangle");
    s.click(*rect, { 2800, -200, 1000 });
    rect->pointerMove(s.ctx, tools::PointerEvent { s.screen({ 3200, 200, 1000 }), {}, 1 });
    s.click(*rect, { 3200, 200, 1000 });

    const geom::Mesh& root = s.m.mesh(s.m.root());
    REQUIRE(root.faces().size() == 1);
    for (geom::VertexId v : root.vertices().keys()) {
        CHECK(root.position(v).z == Approx(1000).margin(1e-6));
    }
}

TEST_CASE("Move: colțul obiectului mutat pe colțul altui obiect", "[core][tools][snap]")
{
    ObjectScene s;
    const model::InstanceId a = s.addBox({ 0, 0, 0 });
    s.addBox({ 3000, 0, 0 });
    s.rebuildScene({ a });
    s.selection.instances.insert(a);

    auto move = tools::makeTool("move");
    s.click(*move, { 500, 500, 1000 });                  // colțul lui A
    s.click(*move, { 2500, 500, 1000 });                 // colțul lui B (cel din stânga)
    CHECK(glm::length(s.m.instance(a).transform.applyToPoint({ 0, 0, 0 }) - Vec3d { 2000, 0, 0 }) < 1e-6);
}

TEST_CASE("Move: un colț real bate punctul cutiei la aceeași distanță", "[core][tools][snap]")
{
    ObjectScene s;
    const model::InstanceId a = s.addBox({ 0, 0, 0 });
    s.addCylinder({ 3000, 0, 0 });
    s.rebuildScene({ a });
    s.selection.instances.insert(a);

    const Vec3d airCorner { 3500, 500, 1000 };                          // colțul cutiei cilindrului (în aer)
    const Vec3d capVertex = Vec3d { 3000, 0, 1000 } + Vec3d { 500 * std::cos(M_PI / 4), 500 * std::sin(M_PI / 4), 0 };
    const Vec2d mid = (s.screen(airCorner) + s.screen(capVertex)) * 0.5; // egal departe de amândouă

    auto move = tools::makeTool("move");
    s.click(*move, { 500, 500, 1000 });
    const tools::PointerEvent e { mid, {}, 1 };
    move->pointerMove(s.ctx, e);
    move->pointerDown(s.ctx, e);
    move->pointerUp(s.ctx, e);
    CHECK(glm::length(s.m.instance(a).transform.applyToPoint({ 0, 0, 0 }) - (capVertex - Vec3d { 500, 500, 1000 })) < 1e-3);
}

TEST_CASE("Move: vârful capacului unui cilindru, pe care punctele cutiei nu-l oferă", "[core][tools][snap]")
{
    ObjectScene s;
    const model::InstanceId a = s.addBox({ 0, 0, 0 });
    s.addCylinder({ 3000, 0, 0 });
    s.rebuildScene({ a });
    s.selection.instances.insert(a);

    const Vec3d capVertex = Vec3d { 3000, 0, 1000 } + Vec3d { 500 * std::cos(M_PI / 4), 500 * std::sin(M_PI / 4), 0 };
    auto move = tools::makeTool("move");
    s.click(*move, { 500, 500, 1000 });
    s.click(*move, capVertex);
    CHECK(glm::length(s.m.instance(a).transform.applyToPoint({ 0, 0, 0 }) - (capVertex - Vec3d { 500, 500, 1000 })) < 1e-3);
}
