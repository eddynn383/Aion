#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/units/Length.h"

#include "ToolHelpers.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

// Uneltele de curbe: Circle, Polygon, Arc. Curbele sunt aproximate cu segmente; toate muchiile unei curbe
// au același identificator, ca să se selecteze și să se șteargă împreună.
namespace aion::tools {

namespace {

constexpr int kMinSegments = 3;
constexpr int kMaxSegments = 999;

// „24s” = numărul de segmente (ca în SketchUp).
std::optional<int> parseSegments(std::string_view text)
{
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    if (text.size() < 2 || (text.back() != 's' && text.back() != 'S')) {
        return std::nullopt;
    }
    int n = 0;
    for (char c : text.substr(0, text.size() - 1)) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        n = n * 10 + (c - '0');
        if (n > kMaxSegments) {
            return std::nullopt;
        }
    }
    return n >= kMinSegments ? std::optional(n) : std::nullopt;
}

Vec3d anyPerpendicular(const Vec3d& n)
{
    const Vec3d a = std::abs(n.z) < 0.9 ? Vec3d { 0, 0, 1 } : Vec3d { 1, 0, 0 };
    return glm::normalize(glm::cross(a, n));
}

// Planul de desen al unei curbe: al feței de sub cursor, altfel orizontal; săgețile îl fixează pe o axă.
Vec3d drawingNormal(const ToolContext& ctx, const inference::Inference& inf, const std::optional<inference::Axis>& lock)
{
    if (lock) {
        return inference::axisDirection(*lock);
    }
    const geom::FaceId face = inf.kind == inference::Kind::OnFace ? inf.face : inf.pick.face;
    if (face && ctx.mesh.faces().contains(face)) {
        const math::Plane& plane = ctx.mesh.face(face).plane;
        if (std::abs(plane.signedDistance(inf.point)) <= 0.01) {
            return plane.normal;
        }
    }
    return { 0, 0, 1 };
}

bool toggleAxis(std::optional<inference::Axis>& lock, Key key)
{
    const auto axis = key == Key::Right ? inference::Axis::X
        : key == Key::Left              ? inference::Axis::Y
        : key == Key::Up                ? inference::Axis::Z
                                        : inference::Axis::None;
    if (axis == inference::Axis::None) {
        return false;
    }
    lock = lock == axis ? std::nullopt : std::optional(axis);
    return true;
}

// Punctul de sub cursor adus în planul (origin, normal): din inferență, dacă e un punct, altfel din rază.
std::optional<Vec3d> pointOnPlane(
    const ToolContext& ctx, Vec2d screen, const inference::Inference& inf, const Vec3d& origin, const Vec3d& normal)
{
    const math::Plane plane = math::Plane::fromPointNormal(origin, normal);
    if (inf.valid() && inf.kind != inference::Kind::Ground && inf.kind != inference::Kind::OnFace) {
        return plane.project(inf.point);
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

void appendPolyline(Overlay& o, const std::vector<Vec3d>& points, bool closed)
{
    const std::size_t count = closed ? points.size() : points.size() - 1;
    for (std::size_t i = 0; i < count && points.size() >= 2; ++i) {
        o.lines.push_back({ points[i], points[(i + 1) % points.size()], false });
    }
}

// --- Circle și Polygon ----------------------------------------------------------------------

class RadialTool final : public Tool {
public:
    RadialTool(std::string_view id, int segments, bool smooth)
        : m_id(id)
        , m_segments(segments)
        , m_smooth(smooth)
    {
    }

    std::string_view id() const override { return m_id; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (!m_center) {
            if (m_inference.valid()) {
                m_center = m_inference.point;
                m_normal = m_hoverNormal;
                m_radius = Vec3d { 0.0 };
            }
            return;
        }
        commit(ctx);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (!m_center && toggleAxis(m_axisLock, e.key)) {
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_center.reset();
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_center) {
            o.points.push_back(*m_center);
            const double r = glm::length(m_radius);
            if (r > tol::kLength) {
                appendPolyline(o, points(), true);
                o.lines.push_back({ *m_center, *m_center + m_radius, true });
                o.dimensions.push_back({ *m_center, *m_center + m_radius, units::formatLength(r) });
            }
        }
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        const std::string sides = std::to_string(m_segments) + (m_smooth ? " segmente" : " laturi");
        if (m_center) {
            return "Alege raza sau tastează-o · „" + std::to_string(m_segments) + "s” schimbă numărul de " +
                (m_smooth ? "segmente" : "laturi") + " · Esc anulează";
        }
        return "Alege centrul (pe o față: în planul ei) · " + sides + " · săgeți: planul X/Y/Z";
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Rază", m_center ? units::formatLength(glm::length(m_radius)) : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        if (const auto n = parseSegments(text)) {
            m_segments = *n;
            return true;
        }
        const auto r = units::parseLength(text);
        if (!r || !m_center || *r <= tol::kLength) {
            return false;
        }
        const Vec3d dir = glm::length(m_radius) > tol::kLength ? glm::normalize(m_radius) : anyPerpendicular(m_normal);
        m_radius = dir * *r;
        commit(ctx);
        return true;
    }

private:
    [[nodiscard]] std::vector<Vec3d> points() const
    {
        std::vector<Vec3d> out;
        const double r = glm::length(m_radius);
        const Vec3d u = m_radius / r;
        const Vec3d v = glm::cross(m_normal, u);
        for (int k = 0; k < m_segments; ++k) {
            const double a = 2.0 * std::numbers::pi * k / m_segments;
            out.push_back(*m_center + (u * std::cos(a) + v * std::sin(a)) * r);
        }
        return out;
    }

    void track(ToolContext& ctx, const PointerEvent& e)
    {
        m_inference = m_engine.withGuides(ctx.guides).withGridSnap(ctx.gridStep).infer(ctx.mesh, ctx.camera, e.screen, m_center);
        if (!m_center) {
            if (m_inference.valid()) {
                m_hoverNormal = drawingNormal(ctx, m_inference, m_axisLock);
            }
            return;
        }
        if (const auto p = pointOnPlane(ctx, e.screen, m_inference, *m_center, m_normal)) {
            m_radius = *p - *m_center;
        }
    }

    void commit(ToolContext& ctx)
    {
        if (m_center && glm::length(m_radius) > tol::kLength) {
            const std::vector<Vec3d> pts = points();
            const bool smooth = m_smooth;
            ctx.stack.run(m_smooth ? "Circle" : "Polygon",
                [&](geom::Mesh& mesh) { geom::drawCurve(mesh, pts, true, smooth); });
        }
        m_center.reset();
        m_engine.resetLock();
    }

    std::string_view m_id;
    int m_segments;
    bool m_smooth;
    std::optional<Vec3d> m_center;
    Vec3d m_normal { 0, 0, 1 };
    Vec3d m_hoverNormal { 0, 0, 1 };
    Vec3d m_radius { 0.0 };
    std::optional<inference::Axis> m_axisLock;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

// --- Arc (din două puncte și săgeată) --------------------------------------------------------

class ArcTool final : public Tool {
public:
    std::string_view id() const override { return "arc"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        switch (m_phase) {
        case Phase::Start:
            if (m_inference.valid()) {
                m_start = m_inference.point;
                m_end = m_start;
                m_normal = m_hoverNormal;
                m_phase = Phase::End;
            }
            return;
        case Phase::End:
            if (glm::length(m_end - m_start) > tol::kLength) {
                m_phase = Phase::Bulge;
                m_bulge = 0.0;
            }
            return;
        case Phase::Bulge:
            commit(ctx);
            return;
        }
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (m_phase == Phase::Start && toggleAxis(m_axisLock, e.key)) {
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_phase = Phase::Start;
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_phase != Phase::Start) {
            o.points.push_back(m_start);
        }
        if (m_phase == Phase::End) {
            o.lines.push_back({ m_start, m_end, false });
            const double d = glm::length(m_end - m_start);
            if (d > tol::kLength) {
                o.dimensions.push_back({ m_start, m_end, units::formatLength(d) });
            }
        }
        if (m_phase == Phase::Bulge) {
            o.points.push_back(m_end);
            if (std::abs(m_bulge) > tol::kLength) {
                appendPolyline(o, points(), false);
                const Vec3d mid = (m_start + m_end) * 0.5;
                const Vec3d top = mid + side() * m_bulge;
                o.lines.push_back({ mid, top, true });
                o.dimensions.push_back({ mid, top, units::formatLength(std::abs(m_bulge)) });
            } else {
                o.lines.push_back({ m_start, m_end, true });
            }
        }
        if (m_phase != Phase::Bulge) {
            appendInference(o, m_inference);
        }
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        switch (m_phase) {
        case Phase::Start:
            return "Alege începutul arcului · săgeți: planul X/Y/Z";
        case Phase::End:
            return "Alege capătul arcului sau tastează lungimea coardei";
        case Phase::Bulge:
            return "Alege săgeata (cât de curbat e) sau tastează-o · „" + std::to_string(m_segments) +
                "s” schimbă segmentele · Esc anulează";
        }
        return {};
    }

    std::optional<Measurement> measurement() const override
    {
        switch (m_phase) {
        case Phase::Start:
            return Measurement { "Lungime", "" };
        case Phase::End:
            return Measurement { "Lungime", units::formatLength(glm::length(m_end - m_start)) };
        case Phase::Bulge:
            return Measurement { "Săgeată", units::formatLength(std::abs(m_bulge)) };
        }
        return std::nullopt;
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        if (const auto n = parseSegments(text)) {
            m_segments = *n;
            return true;
        }
        const auto value = units::parseLength(text);
        if (!value || std::abs(*value) <= tol::kLength) {
            return false;
        }
        if (m_phase == Phase::End) {
            const Vec3d d = m_end - m_start;
            if (glm::length(d) <= tol::kLength) {
                return false;
            }
            m_end = m_start + glm::normalize(d) * *value;
            m_phase = Phase::Bulge;
            m_bulge = 0.0;
            return true;
        }
        if (m_phase == Phase::Bulge) {
            m_bulge = (m_bulge < 0.0 ? -1.0 : 1.0) * *value;
            commit(ctx);
            return true;
        }
        return false;
    }

private:
    enum class Phase { Start, End, Bulge };

    // Direcția săgeții: perpendiculară pe coardă, în planul arcului.
    [[nodiscard]] Vec3d side() const { return glm::normalize(glm::cross(m_normal, m_end - m_start)); }

    [[nodiscard]] std::vector<Vec3d> points() const
    {
        // Din coarda L și săgeata h: raza R = (h² + (L/2)²) / 2|h|, unghiul la centru θ = 4·atan(2|h| / L).
        const double length = glm::length(m_end - m_start);
        const double h = std::abs(m_bulge);
        const double radius = (h * h + length * length / 4.0) / (2.0 * h);
        const Vec3d mid = (m_start + m_end) * 0.5;
        const Vec3d w = side() * (m_bulge < 0.0 ? -1.0 : 1.0);
        const Vec3d center = mid - w * (radius - h);
        const double sweep = 4.0 * std::atan(2.0 * h / length);
        const math::Transform probe = math::Transform::rotation(center, m_normal, sweep * 0.5);
        // Sensul rotației: cel în care mijlocul arcului ajunge de partea săgeții.
        const double sign = glm::dot(probe.applyToPoint(m_start) - mid, w) >= 0.0 ? 1.0 : -1.0;
        std::vector<Vec3d> out;
        for (int k = 0; k <= m_segments; ++k) {
            const auto t = math::Transform::rotation(center, m_normal, sign * sweep * k / m_segments);
            out.push_back(k == m_segments ? m_end : t.applyToPoint(m_start));
        }
        return out;
    }

    void track(ToolContext& ctx, const PointerEvent& e)
    {
        const std::optional<Vec3d> from = m_phase == Phase::End ? std::optional(m_start) : std::nullopt;
        m_inference = m_engine.withGuides(ctx.guides).withGridSnap(ctx.gridStep).infer(ctx.mesh, ctx.camera, e.screen, from);
        switch (m_phase) {
        case Phase::Start:
            if (m_inference.valid()) {
                m_hoverNormal = drawingNormal(ctx, m_inference, m_axisLock);
            }
            return;
        case Phase::End:
            if (const auto p = pointOnPlane(ctx, e.screen, m_inference, m_start, m_normal)) {
                m_end = *p;
            }
            return;
        case Phase::Bulge:
            if (const auto p = pointOnPlane(ctx, e.screen, m_inference, m_start, m_normal)) {
                m_bulge = glm::dot(*p - (m_start + m_end) * 0.5, side());
            }
            return;
        }
    }

    void commit(ToolContext& ctx)
    {
        if (std::abs(m_bulge) > tol::kLength) {
            const std::vector<Vec3d> pts = points();
            ctx.stack.run("Arc", [&](geom::Mesh& mesh) { geom::drawCurve(mesh, pts, false, true); });
        }
        m_phase = Phase::Start;
        m_engine.resetLock();
    }

    Phase m_phase = Phase::Start;
    Vec3d m_start { 0.0 };
    Vec3d m_end { 0.0 };
    Vec3d m_normal { 0, 0, 1 };
    Vec3d m_hoverNormal { 0, 0, 1 };
    double m_bulge = 0.0;
    int m_segments = 12;
    std::optional<inference::Axis> m_axisLock;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

// --- Offset ---------------------------------------------------------------------------------

// Offset (F): click pe o față, apoi distanța față de contur (spre interior sau spre exterior).
class OffsetTool final : public Tool {
public:
    std::string_view id() const override { return "offset"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        if (!m_face) {
            const geom::FaceId face = faceAt(ctx, e.screen);
            if (face) {
                m_face = face;
                m_distance = 0.0;
                track(ctx, e.screen);
            }
            return;
        }
        track(ctx, e.screen);
        commit(ctx);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        if (m_face) {
            track(ctx, e.screen);
        } else {
            m_hover = faceAt(ctx, e.screen);
        }
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_face = {};
        m_preview.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_preview) {
            appendPolyline(o, *m_preview, true);
        }
        return o;
    }

    view::PickResult hover() const override
    {
        view::PickResult h;
        const geom::FaceId face = m_face ? m_face : m_hover;
        if (face) {
            h.kind = view::PickResult::Kind::Face;
            h.face = face;
        }
        return h;
    }

    std::string status() const override
    {
        return m_face ? "Mută spre interior sau exterior și dă click, sau tastează distanța · Esc anulează"
                      : "Alege fața al cărei contur îl deplasezi";
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Distanță", m_face ? units::formatLength(std::abs(m_distance)) : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        const auto value = units::parseLength(text);
        if (!value || !m_face || std::abs(*value) <= tol::kLength) {
            return false;
        }
        m_distance = m_distance < 0.0 ? -*value : *value;
        return commit(ctx);
    }

private:
    // Fața selectată (dacă e una singură) sau cea de sub cursor.
    static geom::FaceId faceAt(const ToolContext& ctx, Vec2d screen)
    {
        if (ctx.selection.faces.size() == 1) {
            return *ctx.selection.faces.begin();
        }
        return view::pick(ctx.mesh, ctx.camera, screen, { -1.0, -1.0 }).face;
    }

    void track(ToolContext& ctx, Vec2d screen)
    {
        if (!ctx.mesh.faces().contains(m_face)) {
            m_face = {};
            return;
        }
        const geom::Face& face = ctx.mesh.face(m_face);
        const math::Ray ray = ctx.camera.rayAt(screen);
        auto t = math::intersectRayPlane(ray, face.plane);
        if (!t) {
            t = math::intersectRayPlane(ray, face.plane.flipped());
        }
        if (!t) {
            return;
        }
        const Vec3d p = ray.at(*t);
        // Distanța cu semn până la contur: pozitivă în interior.
        const std::vector<Vec3d> outline = ctx.mesh.loopPoints(face.outer);
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t i = 0; i < outline.size(); ++i) {
            best = std::min(best, math::projectOnSegment(p, { outline[i], outline[(i + 1) % outline.size()] }).distance);
        }
        const math::PlaneFrame frame(face.plane);
        std::vector<math::Vec2d> poly;
        for (const Vec3d& q : outline) {
            poly.push_back(frame.toLocal(q));
        }
        const bool inside = math::locatePoint(frame.toLocal(p), poly, tol::kLength) == math::PointLocation::Inside;
        m_distance = inside ? best : -best;
        m_preview = geom::offsetOutline(ctx.mesh, m_face, m_distance);
    }

    bool commit(ToolContext& ctx)
    {
        bool done = false;
        if (m_face && std::abs(m_distance) > tol::kLength) {
            const geom::FaceId face = m_face;
            const double distance = m_distance;
            if (geom::offsetOutline(ctx.mesh, face, distance)) {
                ctx.stack.run("Offset", [&](geom::Mesh& mesh) { done = geom::offsetFace(mesh, face, distance); });
                ctx.selection.prune(ctx.mesh);
            }
        }
        m_face = {};
        m_preview.reset();
        return done;
    }

    geom::FaceId m_face;
    geom::FaceId m_hover;
    double m_distance = 0.0;
    std::optional<std::vector<Vec3d>> m_preview;
};

} // namespace

std::unique_ptr<Tool> makeCircleTool()
{
    return std::make_unique<RadialTool>("circle", 24, true);
}

std::unique_ptr<Tool> makePolygonTool()
{
    return std::make_unique<RadialTool>("polygon", 6, false);
}

std::unique_ptr<Tool> makeOffsetTool()
{
    return std::make_unique<OffsetTool>();
}

std::unique_ptr<Tool> makeArcTool()
{
    return std::make_unique<ArcTool>();
}

} // namespace aion::tools
