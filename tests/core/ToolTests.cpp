#include "aion/core/geom/Operations.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Texturing.h"
#include "aion/core/tools/Tool.h"
#include "aion/core/view/Picking.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>

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

    auto scale = tools::makeTool("scale");
    s.click(*scale, { 0, 0, 1000 });
    s.click(*scale, { 0, 2000, 1000 });
    scale->pointerMove(s.ctx, Scene::at(s.screen({ 0, 2500, 1000 })));
    REQUIRE(scale->applyMeasurement(s.ctx, "2"));
    CHECK(maxCoord(s.mesh, 1) == Approx(4000));
    CHECK(maxCoord(s.mesh, 2) == Approx(1000));
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

TEST_CASE("Unelte pe model: selectarea unei componente, copii legate cu Move+Option, dublu-click intră", "[core][tools][model]")
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

    // Move + Option, 1000 pe axa roșie: o copie a aceleiași definiții.
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
    REQUIRE(m.instancesOf(def).size() == 2);
    const model::InstanceId copy = m.instancesOf(def).back();
    CHECK(glm::length(m.instance(copy).transform.applyToPoint({ 0, 0, 0 }) - Vec3d { 1000, 0, 0 }) < 1e-6);

    // „x3”: încă două copii.
    REQUIRE(move->applyMeasurement(ctx, "x3"));
    CHECK(m.instancesOf(def).size() == 4);

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

TEST_CASE("Obiecte primitive: două clicuri creează un obiect nou, selectat, cu mărimea aleasă", "[core][tools][primitives]")
{
    model::Model m;
    commands::CommandStack stack(m);
    geom::Selection selection;
    view::Camera camera;
    camera.setViewportSize(1000, 800);
    camera.setPose(view::standardViewPose(view::StandardView::Top, { { 0, 0, 0 }, 0, 0, 6000 }));
    tools::ToolContext ctx { m.mesh(m.root()), stack, selection, camera };
    ctx.model = &m;
    ctx.context = m.root();
    auto cube = tools::makeTool("cube");
    REQUIRE(cube);
    const auto click = [&](Vec3d p) {
        const auto s = camera.project(p);
        REQUIRE(s);
        tools::PointerEvent e;
        e.screen = *s;
        cube->pointerMove(ctx, e);
        cube->pointerDown(ctx, e);
    };
    click({ 1000, 1000, 0 });
    click({ 1400, 1000, 0 }); // raza 400 → latura 800
    REQUIRE(selection.instances.size() == 1);
    const model::InstanceId id = *selection.instances.begin();
    const model::Placement p = model::placementOf(m, id);
    CHECK(p.size.x == Approx(800.0).margin(1.0));
    CHECK(p.size.z == Approx(800.0).margin(1.0));
    CHECK(m.mesh(m.root()).faces().empty()); // geometria e în obiect, nu în model
    CHECK(model::isSolid(m, m.instance(id).definition));
    CHECK(m.definition(m.instance(id).definition).shape == "box"); // iconița din listă

    // VCB: lățime și înălțime.
    auto cylinder = tools::makeTool("cylinder");
    const auto s = camera.project({ -1000, -1000, 0 });
    tools::PointerEvent e;
    e.screen = *s;
    cylinder->pointerMove(ctx, e);
    cylinder->pointerDown(ctx, e);
    REQUIRE(cylinder->applyMeasurement(ctx, "600,1500"));
    const model::Placement c = model::placementOf(m, *selection.instances.begin());
    CHECK(c.size.x == Approx(600.0).margin(1.0));
    CHECK(c.size.z == Approx(1500.0).margin(1e-6));
    CHECK(m.definition(m.instance(*selection.instances.begin()).definition).shape == "cylinder");
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
