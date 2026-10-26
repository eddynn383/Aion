#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/tools/Tool.h"

#include <glm/geometric.hpp>

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

// --- Line -----------------------------------------------------------------------------------

class LineTool final : public Tool {
public:
    std::string_view id() const override { return "line"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        const auto p = surfacePoint(ctx, e.screen);
        if (!p) {
            return;
        }
        if (!m_start) {
            m_start = p->point;
            m_current = p->point;
            return;
        }
        if (tol::samePoint(*m_start, p->point)) {
            return;
        }
        geom::DrawLineResult result;
        const Vec3d a = *m_start;
        const Vec3d b = p->point;
        ctx.stack.run("Line", [&](geom::Mesh& mesh) { result = geom::drawLine(mesh, a, b); });
        // Ca în SketchUp: linia continuă din ultimul punct, dar se oprește când a închis o față.
        if (result.createdFaces.empty()) {
            m_start = b;
        } else {
            m_start.reset();
        }
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        if (const auto p = surfacePoint(ctx, e.screen)) {
            m_current = p->point;
            m_hover = p->pick;
        }
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Enter) {
            cancel(ctx);
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override { m_start.reset(); }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_start) {
            o.lines.push_back({ *m_start, m_current, false });
            o.points.push_back(*m_start);
        }
        o.points.push_back(m_current);
        return o;
    }

    view::PickResult hover() const override { return m_hover; }

    std::string status() const override
    {
        return m_start ? "Alege următorul punct · Esc sau Enter termină linia" : "Alege punctul de început al liniei";
    }

private:
    std::optional<Vec3d> m_start;
    Vec3d m_current { 0.0 };
    view::PickResult m_hover;
};

// --- Rectangle ------------------------------------------------------------------------------

class RectangleTool final : public Tool {
public:
    std::string_view id() const override { return "rectangle"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        if (!m_origin) {
            const auto p = surfacePoint(ctx, e.screen);
            if (!p) {
                return;
            }
            begin(ctx, *p);
            return;
        }
        update(ctx, e.screen);
        const auto corners = currentCorners();
        if (!corners) {
            return;
        }
        const std::vector<Vec3d> pts(corners->begin(), corners->end());
        ctx.stack.run("Rectangle", [&](geom::Mesh& mesh) { geom::drawPolygon(mesh, pts); });
        m_origin.reset();
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        if (m_origin) {
            update(ctx, e.screen);
        } else if (const auto p = surfacePoint(ctx, e.screen)) {
            m_current = p->point;
            m_hover = p->pick;
        }
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    void cancel(ToolContext& /*ctx*/) override { m_origin.reset(); }

    Overlay overlay() const override
    {
        Overlay o;
        if (const auto c = currentCorners()) {
            for (std::size_t i = 0; i < 4; ++i) {
                o.lines.push_back({ (*c)[i], (*c)[(i + 1) % 4], false });
            }
            o.lines.push_back({ (*c)[0], (*c)[2], true }); // diagonala, punctată
        }
        o.points.push_back(m_origin.value_or(m_current));
        return o;
    }

    view::PickResult hover() const override { return m_hover; }

    std::string status() const override
    {
        return m_origin ? "Alege colțul opus · Esc anulează" : "Alege primul colț (pe sol sau pe o față)";
    }

private:
    void begin(ToolContext& ctx, const SurfacePoint& p)
    {
        // Pe o față: dreptunghiul stă în planul ei. Altfel: plan orizontal prin punct.
        math::Plane plane = math::Plane::fromPointNormal(p.point, { 0.0, 0.0, 1.0 });
        if (p.pick.face) {
            plane = ctx.mesh.face(p.pick.face).plane;
            plane = math::Plane::fromPointNormal(p.point, plane.normal);
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
    }

    void update(ToolContext& ctx, Vec2d screen)
    {
        const math::Ray ray = ctx.camera.rayAt(screen);
        const double denom = glm::dot(m_plane.normal, ray.direction);
        if (std::abs(denom) < 1e-9) {
            return;
        }
        const double t = (m_plane.d - glm::dot(m_plane.normal, ray.origin)) / denom;
        if (t > 0.0) {
            m_current = ray.at(t);
        }
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
    view::PickResult m_hover;
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
