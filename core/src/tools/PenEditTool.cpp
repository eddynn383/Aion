#include "aion/core/geom/Bezier.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"

#include "ToolHelpers.h"

#include <glm/geometric.hpp>

#include <algorithm>

namespace aion::tools {

namespace {

constexpr double kPointPx = 10.0; // un click pe un punct
constexpr double kHandlePx = 8.0; // pe capătul unui mâner
constexpr double kCurvePx = 6.0; // pe curbă: un punct nou
constexpr double kOutsidePx = 12.0; // mai departe de atât: ieși din editare
constexpr int kCurveSamples = 64; // pe segment, pentru click-ul pe curbă

// --- Editarea curbei ------------------------------------------------------------------------

// Editarea unei curbe Pen sau a unei căi (ADR 0036), ca în Figma: tragi punctele și mânerele (un mâner neted îl
// oglindește pe celălalt; Option îl rupe), Option + click comută colț ↔ neted, click pe curbă adaugă un punct, Delete
// șterge punctul selectat, Esc sau click în afară ies. Unealta nu scrie în model: Document aplică cererile.
class PenEditTool final : public Tool {
public:
    std::string_view id() const override { return "penedit"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        if (!ctx.editCurve) {
            done(ctx);
            return;
        }
        m_curve = *ctx.editCurve;
        m_normal = ctx.editNormal;
        m_pressed = true;
        m_dragging = false;
        m_press = e.screen;
        m_alt = e.modifiers.alt;
        m_grab = Grab::None;
        // Mânerele punctului selectat și ale vecinilor, apoi punctele, apoi curba.
        if (const auto handle = handleAt(ctx, e.screen)) {
            m_grab = handle->second ? Grab::Out : Grab::In;
            m_index = handle->first;
            return;
        }
        if (const auto point = pointAt(ctx, e.screen)) {
            m_grab = Grab::Point;
            m_index = *point;
            m_selected = *point;
            return;
        }
        if (const auto hit = curveAt(ctx, e.screen, kCurvePx)) {
            const geom::BezierPath inserted = geom::insertPoint(m_curve, hit->first, hit->second);
            m_selected = static_cast<int>(hit->first) + 1;
            emit(ctx, inserted);
            m_pressed = false;
            return;
        }
        if (!curveAt(ctx, e.screen, kOutsidePx)) {
            done(ctx);
        }
        m_pressed = false;
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        if (!m_dragging && ctx.editCurve) {
            m_curve = *ctx.editCurve; // la intrarea în editare (și după fiecare cerere aplicată) punctele apar imediat
            m_normal = ctx.editNormal;
        }
        if (!m_pressed || m_grab == Grab::None || !ctx.editCurve) {
            return;
        }
        m_dragging = m_dragging || glm::length(e.screen - m_press) > kDragThresholdPx;
        if (!m_dragging) {
            return;
        }
        const geom::BezierPoint& original = ctx.editCurve->points[static_cast<std::size_t>(m_index)];
        const Vec3d from = m_grab == Grab::Point ? original.point : m_grab == Grab::Out ? original.out : original.in;
        m_inference = m_engine.withGuides(ctx.guides).withGridSnap(ctx.gridStep).infer(ctx.mesh, ctx.camera, e.screen, from);
        const std::optional<Vec3d> cursor = onPlane(ctx, e.screen, original.point);
        if (!cursor) {
            return;
        }
        m_curve = *ctx.editCurve;
        geom::BezierPoint& p = m_curve.points[static_cast<std::size_t>(m_index)];
        if (m_grab == Grab::Point) {
            const Vec3d delta = *cursor - p.point;
            p.point += delta;
            p.in += delta;
            p.out += delta;
        } else {
            Vec3d& moved = m_grab == Grab::Out ? p.out : p.in;
            Vec3d& other = m_grab == Grab::Out ? p.in : p.out;
            moved = *cursor;
            if (e.modifiers.alt) {
                p.smooth = false; // Option: mânerul se rupe de pereche
            } else if (p.smooth) {
                other = 2.0 * p.point - moved;
            }
        }
    }

    void pointerUp(ToolContext& ctx, const PointerEvent& /*e*/) override
    {
        if (m_pressed && m_dragging) {
            emit(ctx, m_curve);
        } else if (m_pressed && m_grab == Grab::Point && m_alt) {
            emit(ctx, toggled(m_curve, static_cast<std::size_t>(m_index)));
        }
        m_pressed = false;
        m_dragging = false;
        m_grab = Grab::None;
        m_inference = {};
    }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Escape) {
            done(ctx);
            return true;
        }
        if (e.key == Key::Delete) {
            if (ctx.editCurve && m_selected >= 0 && ctx.editCurve->points.size() > 2
                && m_selected < static_cast<int>(ctx.editCurve->points.size())) {
                emit(ctx, geom::removePoint(*ctx.editCurve, static_cast<std::size_t>(m_selected)));
                m_selected = -1;
            }
            return true;
        }
        return false;
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_pressed = false;
        m_dragging = false;
        m_grab = Grab::None;
    }

    Overlay overlay() const override
    {
        Overlay o;
        const std::vector<Vec3d> samples = geom::sampleBezier(m_curve, geom::defaultTolerance(m_curve));
        const std::size_t count = m_curve.closed ? samples.size() : samples.size() - 1;
        for (std::size_t i = 0; i < count && samples.size() >= 2; ++i) {
            o.lines.push_back({ samples[i], samples[(i + 1) % samples.size()], false, inference::Tint::Neutral, true });
        }
        for (std::size_t i = 0; i < m_curve.points.size(); ++i) {
            const geom::BezierPoint& p = m_curve.points[i];
            o.grips.push_back({ p.point, p.smooth ? inference::Kind::Midpoint : inference::Kind::Endpoint,
                static_cast<int>(i) == m_selected, false });
        }
        for (const int i : visibleHandles()) {
            const geom::BezierPoint& p = m_curve.points[static_cast<std::size_t>(i)];
            for (const Vec3d& h : { p.in, p.out }) {
                if (glm::length(h - p.point) > tol::kLength) {
                    o.lines.push_back({ p.point, h, true, inference::Tint::Neutral, true });
                    o.points.push_back(h);
                }
            }
        }
        appendInference(o, m_inference);
        return o;
    }

    std::string status() const override
    {
        return "Trage punctele sau mânerele · Option: rupe mânerul, Option + click: colț ↔ neted · click pe curbă: "
               "punct nou · ⌫: șterge punctul · Esc: gata";
    }

private:
    enum class Grab { None, Point, In, Out };

    // Punctele ale căror mânere se văd și se pot trage: cel selectat și vecinii lui.
    std::vector<int> visibleHandles() const
    {
        std::vector<int> out;
        const int n = static_cast<int>(m_curve.points.size());
        if (m_selected < 0 || m_selected >= n) {
            return out;
        }
        for (const int d : { 0, -1, 1 }) {
            int i = m_selected + d;
            if (m_curve.closed) {
                i = (i + n) % n;
            }
            if (i >= 0 && i < n && std::find(out.begin(), out.end(), i) == out.end()) {
                out.push_back(i);
            }
        }
        return out;
    }

    std::optional<std::pair<int, bool>> handleAt(const ToolContext& ctx, Vec2d screen) const
    {
        double best = kHandlePx;
        std::optional<std::pair<int, bool>> found;
        for (const int i : visibleHandles()) {
            const geom::BezierPoint& p = m_curve.points[static_cast<std::size_t>(i)];
            for (const bool out : { true, false }) {
                const Vec3d h = out ? p.out : p.in;
                if (glm::length(h - p.point) <= tol::kLength) {
                    continue;
                }
                if (const auto s = ctx.camera.project(h); s && glm::length(*s - screen) < best) {
                    best = glm::length(*s - screen);
                    found = std::pair { i, out };
                }
            }
        }
        return found;
    }

    std::optional<int> pointAt(const ToolContext& ctx, Vec2d screen) const
    {
        double best = kPointPx;
        std::optional<int> found;
        for (std::size_t i = 0; i < m_curve.points.size(); ++i) {
            if (const auto s = ctx.camera.project(m_curve.points[i].point); s && glm::length(*s - screen) < best) {
                best = glm::length(*s - screen);
                found = static_cast<int>(i);
            }
        }
        return found;
    }

    // Segmentul și parametrul celui mai apropiat punct al curbei de cursor (pe ecran), sub `radius` pixeli.
    std::optional<std::pair<std::size_t, double>> curveAt(const ToolContext& ctx, Vec2d screen, double radius) const
    {
        double best = radius;
        std::optional<std::pair<std::size_t, double>> found;
        for (std::size_t s = 0; s < m_curve.segmentCount(); ++s) {
            for (int i = 1; i < kCurveSamples; ++i) {
                const double t = static_cast<double>(i) / kCurveSamples;
                if (const auto p = ctx.camera.project(geom::bezierPoint(m_curve, s, t)); p && glm::length(*p - screen) < best) {
                    best = glm::length(*p - screen);
                    found = std::pair { s, t };
                }
            }
        }
        return found;
    }

    std::optional<Vec3d> onPlane(const ToolContext& ctx, Vec2d screen, const Vec3d& anchor) const
    {
        const math::Plane plane = math::Plane::fromPointNormal(anchor, m_normal);
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

    // Colț ↔ neted: un colț are mânerele pe punct; unul neted le primește pe direcția vecinilor, la ⅓ din distanță.
    static geom::BezierPath toggled(geom::BezierPath path, std::size_t index)
    {
        geom::BezierPoint& p = path.points[index];
        if (p.smooth || glm::length(p.in - p.point) > tol::kLength || glm::length(p.out - p.point) > tol::kLength) {
            p.in = p.point;
            p.out = p.point;
            p.smooth = false;
            return path;
        }
        const std::size_t n = path.points.size();
        const bool hasPrev = path.closed || index > 0;
        const bool hasNext = path.closed || index + 1 < n;
        const Vec3d prev = hasPrev ? path.points[(index + n - 1) % n].point : p.point;
        const Vec3d next = hasNext ? path.points[(index + 1) % n].point : p.point;
        Vec3d dir = next - prev;
        if (glm::length(dir) <= tol::kLength) {
            return path;
        }
        dir = glm::normalize(dir);
        p.in = p.point - dir * (glm::length(p.point - prev) / 3.0);
        p.out = p.point + dir * (glm::length(next - p.point) / 3.0);
        p.smooth = true;
        return path;
    }

    void emit(ToolContext& ctx, const geom::BezierPath& path)
    {
        m_curve = path;
        if (ctx.requests != nullptr) {
            ctx.requests->curveEdit = path;
        }
    }

    void done(ToolContext& ctx)
    {
        if (ctx.requests != nullptr) {
            ctx.requests->curveEditDone = true;
        }
        m_selected = -1;
    }

    geom::BezierPath m_curve;
    Vec3d m_normal { 0.0, 0.0, 1.0 };
    int m_selected = -1;
    int m_index = -1;
    Grab m_grab = Grab::None;
    bool m_pressed = false;
    bool m_dragging = false;
    bool m_alt = false;
    Vec2d m_press { 0.0 };
    inference::Engine m_engine;
    inference::Inference m_inference;
};

} // namespace

std::unique_ptr<Tool> makePenEditTool()
{
    return std::make_unique<PenEditTool>();
}

} // namespace aion::tools
