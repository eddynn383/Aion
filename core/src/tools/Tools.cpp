#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/tools/Tool.h"
#include "aion/core/units/Length.h"

#include <glm/geometric.hpp>

#include <array>
#include <cmath>
#include <set>

namespace aion::tools {

namespace {

// Distanța minimă (în pixeli) de la care o apăsare devine tragere.
constexpr double kDragThresholdPx = 4.0;

const math::Plane kGround = math::Plane::fromPointNormal({ 0.0, 0.0, 0.0 }, { 0.0, 0.0, 1.0 });

} // namespace

bool Tool::key(ToolContext& ctx, const KeyEvent& e)
{
    if (e.key == Key::Escape) {
        cancel(ctx);
        return true;
    }
    return false;
}

void Tool::cancel(ToolContext& /*ctx*/)
{
}

std::optional<SurfacePoint> surfacePoint(const ToolContext& ctx, Vec2d screen)
{
    SurfacePoint out;
    out.pick = view::pick(ctx.mesh, ctx.camera, screen);
    if (out.pick.hit()) {
        out.point = out.pick.point;
        return out;
    }
    const math::Ray ray = ctx.camera.rayAt(screen);
    if (const auto t = math::intersectRayPlane(ray, kGround)) {
        out.point = ray.at(*t);
        return out;
    }
    return std::nullopt;
}

bool deleteSelection(ToolContext& ctx)
{
    ctx.selection.prune(ctx.mesh);
    if (ctx.selection.empty()) {
        return false;
    }
    const geom::Selection doomed = ctx.selection;
    ctx.selection.clear();
    return ctx.stack.run("Delete", [&](geom::Mesh& mesh) {
        for (geom::FaceId f : doomed.faces) {
            if (mesh.faces().contains(f)) {
                geom::eraseFace(mesh, f);
            }
        }
        for (geom::EdgeId e : doomed.edges) {
            if (mesh.edges().contains(e)) {
                geom::eraseEdge(mesh, e);
            }
        }
    });
}

namespace {

// --- Select ---------------------------------------------------------------------------------

class SelectTool final : public Tool {
public:
    std::string_view id() const override { return "select"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        m_press = e.screen;
        m_current = e.screen;
        m_dragging = false;
        m_hover = view::pick(ctx.mesh, ctx.camera, e.screen);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        m_current = e.screen;
        if (m_press && glm::length(e.screen - *m_press) > kDragThresholdPx) {
            m_dragging = true;
        }
        m_hover = m_dragging ? view::PickResult {} : view::pick(ctx.mesh, ctx.camera, e.screen);
    }

    void pointerUp(ToolContext& ctx, const PointerEvent& e) override
    {
        if (!m_press) {
            return;
        }
        geom::Selection picked;
        if (m_dragging) {
            const bool crossing = e.screen.x < m_press->x;
            picked = view::pickInRectangle(ctx.mesh, ctx.camera, *m_press, e.screen, crossing);
        } else {
            picked = clickSelection(ctx, e);
        }
        apply(ctx.selection, picked, e.modifiers);
        m_press.reset();
        m_dragging = false;
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_press.reset();
        m_dragging = false;
    }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Escape && !m_dragging) {
            ctx.selection.clear(); // Esc fără tragere: deselectează tot
            return true;
        }
        return Tool::key(ctx, e);
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_dragging && m_press) {
            o.selectionRect = std::pair { *m_press, m_current };
            o.crossing = m_current.x < m_press->x;
        }
        return o;
    }

    view::PickResult hover() const override { return m_hover; }

    std::string status() const override
    {
        return "Click pentru a selecta · Shift: adaugă/scoate · Cmd: adaugă · trage pentru dreptunghi";
    }

private:
    static geom::Selection clickSelection(ToolContext& ctx, const PointerEvent& e)
    {
        // Vârfurile nu se selectează: căutăm muchia sau fața de sub cursor.
        const view::PickResult hit = view::pick(ctx.mesh, ctx.camera, e.screen, { 0.0, 5.0 });
        geom::Selection s;
        if (hit.kind == view::PickResult::Kind::Edge) {
            s = e.clickCount >= 2 ? geom::edgeWithFaces(ctx.mesh, hit.edge) : geom::Selection { { hit.edge }, {} };
        } else if (hit.kind == view::PickResult::Kind::Face) {
            s = e.clickCount >= 2 ? geom::faceWithEdges(ctx.mesh, hit.face) : geom::Selection { {}, { hit.face } };
        }
        if (e.clickCount >= 3 && !s.empty()) {
            s = geom::connectedTo(ctx.mesh, s);
        }
        return s;
    }

    // Fără modificatori: înlocuiește; Cmd: adaugă; Shift: inversează; Shift+Cmd: scoate (ca în SketchUp).
    static void apply(geom::Selection& selection, const geom::Selection& picked, const Modifiers& m)
    {
        if (m.shift && m.command) {
            selection.remove(picked);
        } else if (m.shift) {
            selection.toggle(picked);
        } else if (m.command) {
            selection.add(picked);
        } else {
            selection = picked;
        }
    }

    std::optional<Vec2d> m_press;
    Vec2d m_current { 0.0 };
    bool m_dragging = false;
    view::PickResult m_hover;
};

// --- Inferență (comun pentru Line și Rectangle) -----------------------------------------------

void appendInference(Overlay& o, const inference::Inference& inf)
{
    for (const inference::Guide& g : inf.guides) {
        o.lines.push_back({ g.a, g.b, g.dashed, g.tint, true });
    }
    if (inf.valid()) {
        o.marker = InferenceMarker { inf.point, inf.kind, std::string(inference::label(inf)), inf.locked };
    }
}

// Ce se evidențiază sub cursor: muchia sau fața pe care stă punctul inferat (capetele au marcajul lor).
view::PickResult inferenceHover(const inference::Inference& inf)
{
    view::PickResult h;
    switch (inf.kind) {
    case inference::Kind::OnEdge:
    case inference::Kind::Midpoint:
    case inference::Kind::Intersection:
        if (inf.edge) {
            h.kind = view::PickResult::Kind::Edge;
            h.edge = inf.edge;
            h.point = inf.point;
        }
        break;
    case inference::Kind::OnFace:
        h.kind = view::PickResult::Kind::Face;
        h.face = inf.face;
        h.point = inf.point;
        break;
    default:
        break;
    }
    return h;
}

// Săgețile: → axa roșie, ← verde, ↑ albastră, ↓ paralel cu muchia de referință.
bool handleArrow(inference::Engine& engine, Key key)
{
    switch (key) {
    case Key::Right:
        engine.toggleAxisLock(inference::Axis::X);
        return true;
    case Key::Left:
        engine.toggleAxisLock(inference::Axis::Y);
        return true;
    case Key::Up:
        engine.toggleAxisLock(inference::Axis::Z);
        return true;
    case Key::Down:
        engine.toggleReferenceLock();
        return true;
    default:
        return false;
    }
}

// Shift ținut apăsat blochează inferența curentă; eliberat, o deblochează (doar lock-ul pus de el).
void updateShiftLock(inference::Engine& engine, bool shift, bool& shiftLock)
{
    if (shift && !shiftLock && !engine.locked()) {
        engine.lockCurrent();
        shiftLock = engine.locked();
    } else if (!shift && shiftLock) {
        engine.unlock();
        shiftLock = false;
    }
}

// --- Line -----------------------------------------------------------------------------------

class LineTool final : public Tool {
public:
    std::string_view id() const override { return "line"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (!m_inference.valid()) {
            return;
        }
        if (!m_start) {
            m_start = m_inference.point;
            m_current = m_inference.point;
            restartInference();
            return;
        }
        segmentTo(ctx, m_inference.point);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (handleArrow(m_engine, e.key)) {
            return true;
        }
        if (e.key == Key::Enter) {
            cancel(ctx);
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_start.reset();
        m_engine.reset();
        m_shiftLock = false;
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_start) {
            o.lines.push_back({ *m_start, m_current, false });
            o.points.push_back(*m_start);
            const double length = glm::length(m_current - *m_start);
            if (length > tol::kLength) {
                o.dimensions.push_back({ *m_start, m_current, units::formatLength(length) });
            }
        }
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        return m_start ? "Alege următorul punct sau tastează lungimea · săgeți/Shift: blochează direcția · Esc termină"
                       : "Alege punctul de început al liniei";
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Lungime", m_start ? units::formatLength(glm::length(m_current - *m_start)) : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        const auto length = units::parseLength(text);
        if (!length || !m_start || std::abs(*length) <= tol::kLength) {
            return false;
        }
        const Vec3d d = m_current - *m_start;
        if (glm::length(d) <= tol::kLength) {
            return false;
        }
        segmentTo(ctx, *m_start + glm::normalize(d) * *length);
        return true;
    }

private:
    void track(ToolContext& ctx, const PointerEvent& e)
    {
        updateShiftLock(m_engine, e.modifiers.shift, m_shiftLock);
        m_inference = m_engine.infer(ctx.mesh, ctx.camera, e.screen, m_start);
        if (m_inference.valid()) {
            m_current = m_inference.point;
        }
    }

    void segmentTo(ToolContext& ctx, const Vec3d& b)
    {
        if (!m_start || tol::samePoint(*m_start, b)) {
            return;
        }
        geom::DrawLineResult result;
        const Vec3d a = *m_start;
        ctx.stack.run("Line", [&](geom::Mesh& mesh) { result = geom::drawLine(mesh, a, b); });
        // Ca în SketchUp: linia continuă din ultimul punct, dar se oprește când a închis o față.
        if (result.createdFaces.empty()) {
            m_start = b;
        } else {
            m_start.reset();
        }
        m_current = b;
        restartInference();
    }

    void restartInference()
    {
        m_engine.resetLock();
        m_shiftLock = false;
    }

    std::optional<Vec3d> m_start;
    Vec3d m_current { 0.0 };
    inference::Engine m_engine;
    inference::Inference m_inference;
    bool m_shiftLock = false;
};

// --- Rectangle ------------------------------------------------------------------------------

class RectangleTool final : public Tool {
public:
    std::string_view id() const override { return "rectangle"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (!m_origin) {
            if (m_inference.valid()) {
                begin(ctx, m_inference);
            }
            return;
        }
        create(ctx);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (handleArrow(m_engine, e.key)) {
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_origin.reset();
        m_engine.reset();
        m_shiftLock = false;
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (const auto c = currentCorners()) {
            for (std::size_t i = 0; i < 4; ++i) {
                o.lines.push_back({ (*c)[i], (*c)[(i + 1) % 4], false });
            }
            o.lines.push_back({ (*c)[0], (*c)[2], true }); // diagonala, punctată
            o.dimensions.push_back({ (*c)[0], (*c)[1], units::formatLength(glm::length((*c)[1] - (*c)[0])) });
            o.dimensions.push_back({ (*c)[1], (*c)[2], units::formatLength(glm::length((*c)[2] - (*c)[1])) });
        }
        if (m_origin) {
            o.points.push_back(*m_origin);
        }
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        return m_origin ? "Alege colțul opus sau tastează dimensiunile (3000,2000) · Esc anulează"
                        : "Alege primul colț (pe sol sau pe o față)";
    }

    std::optional<Measurement> measurement() const override
    {
        if (!m_origin) {
            return Measurement { "Dimensiuni", "" };
        }
        const Vec3d d = m_current - *m_origin;
        return Measurement { "Dimensiuni",
            units::formatLength(std::abs(glm::dot(d, m_u))) + ", " + units::formatLength(std::abs(glm::dot(d, m_v))) };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        const auto values = units::parseLengths(text, 2);
        if (!values || !m_origin) {
            return false;
        }
        const Vec3d d = m_current - *m_origin;
        double du = glm::dot(d, m_u);
        double dv = glm::dot(d, m_v);
        // Valoarea tastată dă mărimea; sensul vine din poziția cursorului (sau e pozitiv, dacă nu există încă).
        const auto apply = [](double current, const std::optional<double>& typed) {
            if (!typed) {
                return current;
            }
            return current < 0.0 ? -*typed : *typed;
        };
        du = apply(du, (*values)[0]);
        if (values->size() > 1) {
            dv = apply(dv, (*values)[1]);
        }
        if (std::abs(du) <= tol::kLength || std::abs(dv) <= tol::kLength) {
            return false;
        }
        m_current = *m_origin + m_u * du + m_v * dv;
        create(ctx);
        return true;
    }

private:
    void track(ToolContext& ctx, const PointerEvent& e)
    {
        updateShiftLock(m_engine, e.modifiers.shift, m_shiftLock);
        // Colțul opus nu folosește direcții din primul colț (ar da un dreptunghi degenerat),
        // dar folosește capetele, muchiile și alinierea cu punctele de referință, proiectate pe plan.
        m_inference = m_engine.infer(ctx.mesh, ctx.camera, e.screen, std::nullopt,
            m_origin ? std::optional(m_plane) : std::nullopt);
        if (m_inference.valid()) {
            m_current = m_origin ? m_plane.project(m_inference.point) : m_inference.point;
        }
    }

    void begin(ToolContext& ctx, const inference::Inference& p)
    {
        // Pe o față: dreptunghiul stă în planul ei. Altfel: plan orizontal prin punct.
        math::Plane plane = math::Plane::fromPointNormal(p.point, { 0.0, 0.0, 1.0 });
        const geom::FaceId face = p.kind == inference::Kind::OnFace ? p.face : p.pick.face;
        if (face && ctx.mesh.faces().contains(face)) {
            const math::Plane& fp = ctx.mesh.face(face).plane;
            if (tol::isZero(fp.signedDistance(p.point), 0.01)) {
                plane = math::Plane::fromPointNormal(p.point, fp.normal);
            }
        }
        // Laturile se aliniază cu axele globale proiectate pe plan.
        Vec3d u = Vec3d { 1, 0, 0 } - plane.normal * plane.normal.x;
        if (glm::length(u) < 1e-6) {
            u = Vec3d { 0, 1, 0 } - plane.normal * plane.normal.y;
        }
        m_u = glm::normalize(u);
        m_v = glm::cross(plane.normal, m_u);
        m_plane = plane;
        m_origin = p.point;
        m_current = p.point;
        m_engine.resetLock();
        m_shiftLock = false;
    }

    void create(ToolContext& ctx)
    {
        const auto corners = currentCorners();
        if (!corners) {
            return;
        }
        const std::vector<Vec3d> pts(corners->begin(), corners->end());
        ctx.stack.run("Rectangle", [&](geom::Mesh& mesh) { geom::drawPolygon(mesh, pts); });
        m_origin.reset();
        m_engine.resetLock();
        m_shiftLock = false;
    }

    std::optional<std::array<Vec3d, 4>> currentCorners() const
    {
        if (!m_origin) {
            return std::nullopt;
        }
        const Vec3d d = m_current - *m_origin;
        const double du = glm::dot(d, m_u);
        const double dv = glm::dot(d, m_v);
        if (std::abs(du) <= tol::kLength || std::abs(dv) <= tol::kLength) {
            return std::nullopt;
        }
        const Vec3d o = *m_origin;
        return std::array { o, o + m_u * du, o + m_u * du + m_v * dv, o + m_v * dv };
    }

    std::optional<Vec3d> m_origin;
    Vec3d m_current { 0.0 };
    math::Plane m_plane;
    Vec3d m_u { 1, 0, 0 };
    Vec3d m_v { 0, 1, 0 };
    inference::Engine m_engine;
    inference::Inference m_inference;
    bool m_shiftLock = false;
};

// --- Eraser ---------------------------------------------------------------------------------

class EraserTool final : public Tool {
public:
    std::string_view id() const override { return "eraser"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        m_active = true;
        m_marked.clear();
        mark(ctx, e.screen);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        m_hover = view::pick(ctx.mesh, ctx.camera, e.screen, { 0.0, 6.0 });
        if (m_active) {
            mark(ctx, e.screen);
        }
    }

    void pointerUp(ToolContext& ctx, const PointerEvent& /*e*/) override
    {
        if (!m_active) {
            return;
        }
        m_active = false;
        const std::set<geom::EdgeId> doomed = std::move(m_marked);
        m_marked.clear();
        if (doomed.empty()) {
            return;
        }
        ctx.stack.run("Erase", [&](geom::Mesh& mesh) {
            for (geom::EdgeId e : doomed) {
                if (mesh.edges().contains(e)) {
                    geom::eraseEdge(mesh, e);
                }
            }
        });
        ctx.selection.prune(ctx.mesh);
        m_hover = {};
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_active = false;
        m_marked.clear();
    }

    Overlay overlay() const override
    {
        Overlay o;
        o.markedEdges.assign(m_marked.begin(), m_marked.end());
        return o;
    }

    view::PickResult hover() const override
    {
        return m_hover.kind == view::PickResult::Kind::Edge ? m_hover : view::PickResult {};
    }

    std::string status() const override
    {
        return "Click pe o muchie pentru a o șterge · trage peste mai multe · Esc anulează";
    }

private:
    void mark(ToolContext& ctx, Vec2d screen)
    {
        const view::PickResult hit = view::pick(ctx.mesh, ctx.camera, screen, { 0.0, 6.0 });
        if (hit.kind == view::PickResult::Kind::Edge) {
            m_marked.insert(hit.edge);
        }
    }

    bool m_active = false;
    std::set<geom::EdgeId> m_marked;
    view::PickResult m_hover;
};

} // namespace

std::unique_ptr<Tool> makeTool(std::string_view id)
{
    if (id == "select") {
        return std::make_unique<SelectTool>();
    }
    if (id == "line") {
        return std::make_unique<LineTool>();
    }
    if (id == "rectangle") {
        return std::make_unique<RectangleTool>();
    }
    if (id == "eraser") {
        return std::make_unique<EraserTool>();
    }
    return nullptr;
}

} // namespace aion::tools
