#include "aion/core/geom/Bezier.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/PenCurves.h"

#include "ToolHelpers.h"

#include <glm/geometric.hpp>

namespace aion::tools {

namespace {

constexpr double kClosePx = 10.0; // cât de aproape de primul punct închide un click

// --- Pen ------------------------------------------------------------------------------------

// Pen-ul, ca în Figma (ADR 0036): click = punct cu colț, click și tragi = punct neted (mânerele simetrice), click pe
// primul punct = închide, Enter / Esc / dublu click = termină deschisă. Curba stă pe planul primului click (solul sau
// o față). În Modelare devine geometrie (o curbă Pen editabilă); în Animare, o cale (cerere către Document).
class PenTool final : public Tool {
public:
    std::string_view id() const override { return "pen"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (e.clickCount >= 2 && m_points.size() >= 2) {
            finish(ctx, false); // al doilea click al unui dublu click: primul a pus deja ultimul punct
            return;
        }
        if (m_points.size() >= 3 && nearFirst(ctx, e.screen)) {
            finish(ctx, true);
            return;
        }
        if (m_points.empty()) {
            choosePlane(ctx, e.screen);
            m_cursor = m_origin;
        }
        m_points.push_back({ m_cursor, m_cursor, m_cursor, false });
        m_pressed = true;
        m_dragging = false;
        m_press = e.screen;
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (!m_pressed || m_points.empty()) {
            return;
        }
        m_dragging = m_dragging || glm::length(e.screen - m_press) > kDragThresholdPx;
        if (m_dragging) {
            geom::BezierPoint& p = m_points.back();
            p.out = m_cursor;
            p.in = 2.0 * p.point - m_cursor;
            p.smooth = true;
        }
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override
    {
        m_pressed = false;
        m_dragging = false;
    }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Enter || e.key == Key::Escape) {
            if (m_points.size() >= 2) {
                finish(ctx, false);
            } else {
                reset();
            }
            return true;
        }
        return handleArrow(m_engine, e.key);
    }

    void cancel(ToolContext& /*ctx*/) override { reset(); }

    Overlay overlay() const override
    {
        Overlay o;
        if (!m_points.empty()) {
            // Curba de până acum și segmentul spre cursor (cât nu se trage un mâner).
            geom::BezierPath path { m_points, false };
            if (!m_pressed) {
                path.points.push_back({ m_cursor, m_cursor, m_cursor, false });
            }
            const std::vector<Vec3d> samples = geom::sampleBezier(path, geom::defaultTolerance(path));
            for (std::size_t i = 1; i < samples.size(); ++i) {
                o.lines.push_back({ samples[i - 1], samples[i], false, inference::Tint::Neutral, true });
            }
            // Mânerele ultimului punct.
            const geom::BezierPoint& last = m_points.back();
            if (last.smooth) {
                o.lines.push_back({ last.in, last.out, true, inference::Tint::Neutral, true });
                o.points.push_back(last.in);
                o.points.push_back(last.out);
            }
            for (const geom::BezierPoint& p : m_points) {
                o.points.push_back(p.point);
            }
            if (m_closing) {
                o.marker = InferenceMarker { m_points.front().point, inference::Kind::Endpoint, inference::Tint::Neutral,
                    "Închide", false };
                return o;
            }
        }
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return {}; }

    std::string status() const override
    {
        return "Click: punct cu colț · trage: punct neted · click pe primul punct: închide · Enter: termină · Esc";
    }

private:
    void track(ToolContext& ctx, const PointerEvent& e)
    {
        const std::optional<Vec3d> from = m_points.empty() ? std::nullopt : std::optional(m_points.back().point);
        m_inference = m_engine.withGuides(ctx.guides).withGridSnap(ctx.gridStep).infer(ctx.mesh, ctx.camera, e.screen, from);
        m_closing = !m_pressed && m_points.size() >= 3 && nearFirst(ctx, e.screen);
        if (m_points.empty()) {
            if (m_inference.valid()) {
                m_cursor = m_inference.point;
            }
            return;
        }
        if (m_closing) {
            m_cursor = m_points.front().point;
            return;
        }
        if (const auto p = onPlane(ctx, e.screen)) {
            m_cursor = *p;
        }
    }

    bool nearFirst(const ToolContext& ctx, Vec2d screen) const
    {
        const auto s = ctx.camera.project(m_points.front().point);
        return s && glm::length(*s - screen) <= kClosePx;
    }

    // Planul curbei: fața de sub cursor (a geometriei libere sau a unui obiect), altfel solul.
    void choosePlane(const ToolContext& ctx, Vec2d screen)
    {
        m_normal = { 0, 0, 1 };
        m_origin = m_inference.valid() ? m_inference.point : Vec3d { 0.0 };
        const geom::FaceId face = m_inference.kind == inference::Kind::OnFace ? m_inference.face : m_inference.pick.face;
        if (face && ctx.mesh.faces().contains(face)) {
            m_normal = ctx.mesh.face(face).plane.normal;
            return;
        }
        if (ctx.model == nullptr) {
            return;
        }
        const view::PickResult hit = pickWithInstances(ctx, screen);
        if (hit.kind == view::PickResult::Kind::Instance && hit.face && ctx.model->instances().contains(hit.instance)) {
            const model::Instance& inst = ctx.model->instance(hit.instance);
            const geom::Mesh& mesh = ctx.model->mesh(inst.definition);
            if (mesh.faces().contains(hit.face)) {
                m_normal = glm::normalize(inst.transform.applyToVector(mesh.face(hit.face).plane.normal));
                m_origin = hit.point;
            }
        }
    }

    // Cursorul pe planul curbei: punctul inferat (dacă e unul anume), proiectat; altfel raza intersectată cu planul.
    std::optional<Vec3d> onPlane(const ToolContext& ctx, Vec2d screen) const
    {
        const math::Plane plane = math::Plane::fromPointNormal(m_origin, m_normal);
        if (m_inference.valid() && m_inference.kind != inference::Kind::Ground && m_inference.kind != inference::Kind::OnFace) {
            return plane.project(m_inference.point);
        }
        const math::Ray ray = ctx.camera.rayAt(screen);
        if (const auto t = math::intersectRayPlane(ray, plane)) {
            return ray.at(*t);
        }
        if (const auto t = math::intersectRayPlane(ray, plane.flipped())) {
            return ray.at(*t);
        }
        return std::nullopt;
    }

    void finish(ToolContext& ctx, bool closed)
    {
        if (m_points.size() < 2) {
            reset();
            return;
        }
        const geom::BezierPath path { m_points, closed };
        const Vec3d normal = m_normal;
        if (ctx.drawsPaths) {
            if (ctx.requests != nullptr) {
                ctx.requests->penPath = path;
                ctx.requests->penNormal = normal;
            }
        } else if (ctx.model != nullptr) {
            ctx.stack.run("Pen", [&](model::Model& model) { model::addPenCurveGeometry(model, ctx.context, path, normal); });
        } else {
            ctx.stack.run("Pen", [&](geom::Mesh& mesh) {
                geom::drawCurve(mesh, geom::sampleBezier(path, geom::defaultTolerance(path)), closed, true);
            });
        }
        reset();
    }

    void reset()
    {
        m_points.clear();
        m_pressed = false;
        m_dragging = false;
        m_closing = false;
        m_engine.reset();
        m_inference = {};
    }

    std::vector<geom::BezierPoint> m_points;
    Vec3d m_origin { 0.0 };
    Vec3d m_normal { 0.0, 0.0, 1.0 };
    Vec3d m_cursor { 0.0 };
    Vec2d m_press { 0.0 };
    bool m_pressed = false;
    bool m_dragging = false;
    bool m_closing = false;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

} // namespace

std::unique_ptr<Tool> makePenTool()
{
    return std::make_unique<PenTool>();
}

} // namespace aion::tools
