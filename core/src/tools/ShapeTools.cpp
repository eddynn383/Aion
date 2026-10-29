#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/units/Length.h"

#include "ToolHelpers.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
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
    if (inf.normal && (inf.origin == inference::Origin::Object || inf.kind == inference::Kind::OnFace)) {
        return *inf.normal;
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
        m_inference = inferAt(m_engine, ctx, e.screen, m_center);
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
        m_inference = inferAt(m_engine, ctx, e.screen, from);
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


// --- Obiecte primitive: cub, cilindru, con, sferă ---------------------------------------------
// Click: centrul bazei (pe sol sau pe o față, în planul ei); mișcarea alege mărimea; al doilea click creează un obiect
// nou (definiție proprie, ca să nu taie geometria pe care stă). VCB: „1000” (lățime/diametru) sau „1000,500” (și
// înălțimea).

class PrimitiveTool final : public Tool {
public:
    PrimitiveTool(std::string_view id, geom::Primitive kind, std::string name)
        : m_id(id)
        , m_kind(kind)
        , m_name(std::move(name))
    {
    }

    std::string_view id() const override { return m_id; }

    // Pașii: centrul bazei, mărimea (raza), apoi înălțimea (doar cub, cilindru, con; sfera are doi pași).
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
        const double width = 2.0 * glm::length(m_radius);
        if (m_heightStep) {
            if (m_height > tol::kLength) {
                commit(ctx, width, m_height);
            }
            return;
        }
        if (m_kind == geom::Primitive::Sphere || width <= tol::kLength) {
            commit(ctx, width, std::nullopt);
            return;
        }
        startHeight(width);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }
    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_center.reset();
        m_heightStep = false;
        m_inference = {};
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_center) {
            o.points.push_back(*m_center);
            const double width = 2.0 * glm::length(m_radius);
            if (width > tol::kLength) {
                const auto [u, v] = axes();
                appendBody(o, width, m_heightStep ? m_height : width, u, v);
                o.dimensions.push_back({ *m_center - u * (width / 2), *m_center + u * (width / 2), units::formatLength(width) });
                if (m_heightStep && std::abs(m_height) > tol::kLength) {
                    o.dimensions.push_back({ *m_center, *m_center + m_normal * m_height, units::formatLength(m_height) });
                }
            }
        }
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        if (m_heightStep) {
            return "Alege înălțimea sau tastează-o · Esc anulează";
        }
        if (m_center) {
            return "Alege mărimea bazei sau tastează-o (1000, sau 1000,500 cu înălțimea) · Esc anulează";
        }
        return "Alege centrul bazei " + m_name + " (pe o față: în planul ei)";
    }

    std::optional<Measurement> measurement() const override
    {
        if (m_heightStep) {
            return Measurement { "Înălțime", units::formatLength(m_height) };
        }
        return Measurement { "Lățime", m_center ? units::formatLength(2.0 * glm::length(m_radius)) : "" };
    }

    // Pasul 2: „1000” = lățimea (sfera: gata), „1000,500” = lățimea și înălțimea. Pasul 3: „500” = înălțimea,
    // „1000,500” = lățimea și înălțimea.
    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        const auto values = units::parseLengths(text, 2);
        if (!values || values->empty() || !m_center) {
            return false;
        }
        for (const auto& value : *values) {
            if (value && *value <= tol::kLength) {
                return false;
            }
        }
        const double current = 2.0 * glm::length(m_radius);
        if (m_heightStep && values->size() == 1) {
            if (!(*values)[0]) {
                return false;
            }
            commit(ctx, current, *(*values)[0]);
            return true;
        }
        const double width = (*values)[0].value_or(current);
        if (width <= tol::kLength) {
            return false;
        }
        if (values->size() > 1 && (*values)[1]) {
            commit(ctx, width, *(*values)[1]);
            return true;
        }
        if (m_heightStep) {
            return false; // „1000,” la pasul 3: lipsește înălțimea
        }
        if (m_kind == geom::Primitive::Sphere) {
            commit(ctx, width, std::nullopt);
            return true;
        }
        m_radius = axes().first * (width / 2);
        startHeight(width);
        return true;
    }

private:
    static constexpr int kSegments = 16; // rezoluția cercurilor din previzualizare

    [[nodiscard]] std::pair<Vec3d, Vec3d> axes() const
    {
        const Vec3d u = glm::length(m_radius) > tol::kLength ? glm::normalize(m_radius) : anyPerpendicular(m_normal);
        return { u, glm::cross(m_normal, u) };
    }

    // Direcția privirii spre `point`, din camera de la ultimul `track` (perspectivă: din ochi; ortografic: înainte).
    [[nodiscard]] Vec3d viewDirection(const Vec3d& point) const
    {
        if (m_perspective && glm::length(point - m_eye) > tol::kLength) {
            return glm::normalize(point - m_eye);
        }
        return m_forward;
    }

    // Cercul de rază `r` din jurul lui `c`, în planul (a, b), ca segmente.
    static void appendCircle(Overlay& o, const Vec3d& c, const Vec3d& a, const Vec3d& b, double r)
    {
        std::vector<Vec3d> pts;
        for (int k = 0; k < kSegments; ++k) {
            const double t = 2.0 * std::numbers::pi * k / kSegments;
            pts.push_back(c + (a * std::cos(t) + b * std::sin(t)) * r);
        }
        appendPolyline(o, pts, true);
    }

    // Generatoarele de contur (cilindru, con): punctele bazei unde privirea e tangentă la suprafață.
    [[nodiscard]] std::vector<Vec3d> silhouetteDirections() const
    {
        const Vec3d d = viewDirection(*m_center);
        const Vec3d inPlane = d - m_normal * glm::dot(d, m_normal);
        if (glm::length(inPlane) <= tol::kLength) {
            return {}; // privit de-a lungul axei: conturul e chiar cercul
        }
        const Vec3d side = glm::normalize(glm::cross(m_normal, inPlane));
        return { side, -side };
    }

    // Corpul la mărimea curentă: rețeaua sferei, cercurile și generatoarele cilindrului / conului, muchiile cubului.
    void appendBody(Overlay& o, double width, double height, const Vec3d& u, const Vec3d& v) const
    {
        const Vec3d c = *m_center;
        const double r = width / 2;
        const Vec3d up = m_normal * height;
        switch (m_kind) {
        case geom::Primitive::Box: {
            std::array<Vec3d, 4> base;
            for (std::size_t k = 0; k < base.size(); ++k) {
                const double su = k == 0 || k == 3 ? 1.0 : -1.0;
                const double sv = k < 2 ? 1.0 : -1.0;
                base[k] = c + u * (su * r) + v * (sv * r);
            }
            for (std::size_t k = 0; k < base.size(); ++k) {
                const Vec3d& a = base[k];
                const Vec3d& b = base[(k + 1) % base.size()];
                o.lines.push_back({ a, b, false });
                o.lines.push_back({ a + up, b + up, false });
                o.lines.push_back({ a, a + up, false });
            }
            return;
        }
        case geom::Primitive::Cylinder:
        case geom::Primitive::Cone: {
            const bool cone = m_kind == geom::Primitive::Cone;
            appendCircle(o, c, u, v, r);
            if (!cone) {
                appendCircle(o, c + up, u, v, r);
            }
            std::vector<Vec3d> rims;
            for (int k = 0; k < 8; ++k) {
                const double t = 2.0 * std::numbers::pi * k / 8;
                rims.push_back(u * std::cos(t) + v * std::sin(t));
            }
            for (const Vec3d& s : silhouetteDirections()) {
                rims.push_back(s);
            }
            for (const Vec3d& dir : rims) {
                const Vec3d a = c + dir * r;
                o.lines.push_back({ a, cone ? c + up : a + up, false });
            }
            return;
        }
        case geom::Primitive::Sphere: {
            // Sfera stă pe bază: centrul la înălțimea razei. 8 meridiane (de la pol la pol) și 7 paralele.
            const Vec3d center = c + m_normal * r;
            for (int k = 0; k < 8; ++k) {
                const double t = 2.0 * std::numbers::pi * k / 8;
                const Vec3d dir = u * std::cos(t) + v * std::sin(t);
                std::vector<Vec3d> pts;
                for (int i = 0; i <= kSegments / 2; ++i) {
                    const double phi = 2.0 * std::numbers::pi * i / kSegments;
                    pts.push_back(center + (m_normal * std::cos(phi) + dir * std::sin(phi)) * r);
                }
                appendPolyline(o, pts, false);
            }
            for (int i = 1; i < kSegments / 2; ++i) {
                const double phi = 2.0 * std::numbers::pi * i / kSegments;
                appendCircle(o, center + m_normal * (r * std::cos(phi)), u, v, r * std::sin(phi));
            }
            // Conturul văzut din cameră: cercul mare perpendicular pe privire.
            const Vec3d d = viewDirection(center);
            const Vec3d a = anyPerpendicular(d);
            appendCircle(o, center, a, glm::cross(d, a), r);
            return;
        }
        }
    }

    void startHeight(double width)
    {
        m_heightStep = true;
        m_height = width;
        m_inference = {};
        m_engine.resetLock();
    }

    void track(ToolContext& ctx, const PointerEvent& e)
    {
        m_eye = ctx.camera.eye();
        m_forward = ctx.camera.forward();
        m_perspective = ctx.camera.projection() == view::Projection::Perspective;
        if (m_heightStep) {
            trackHeight(ctx, e.screen);
            return;
        }
        m_inference = inferAt(m_engine, ctx, e.screen, m_center);
        if (!m_center) {
            if (m_inference.valid()) {
                m_hoverNormal = drawingNormal(ctx, m_inference, std::nullopt);
            }
            return;
        }
        if (const auto p = pointOnPlane(ctx, e.screen, m_inference, *m_center, m_normal)) {
            m_radius = *p - *m_center;
        }
    }

    // Înălțimea, ca la Push/Pull: în dreptul unui punct al geometriei (capăt, mijloc, centru, muchie), cu ghidaj
    // punctat până la vârful înălțimii; altfel punctul de pe normala bazei cel mai apropiat de cursor.
    void trackHeight(ToolContext& ctx, Vec2d screen)
    {
        inference::Inference inf = inferAt(m_engine, ctx, screen, std::nullopt);
        const double depth = glm::dot(inf.point - *m_center, m_normal);
        if ((inf.isPoint() || inf.kind == inference::Kind::OnEdge) && std::abs(depth) > tol::kLength) {
            m_height = depth;
            const Vec3d top = *m_center + m_normal * m_height;
            if (!tol::samePoint(top, inf.point)) {
                inf.guides.push_back({ inf.point, top, inference::Tint::Neutral, true });
            }
            m_inference = inf;
            return;
        }
        m_inference = {};
        if (const auto p = inference::pointOnLineUnderCursor(ctx.camera, screen, *m_center, m_normal)) {
            m_height = glm::dot(*p - *m_center, m_normal);
        }
    }

    void commit(ToolContext& ctx, double width, std::optional<double> height)
    {
        m_heightStep = false;
        m_inference = {};
        if (!m_center || width <= tol::kLength || ctx.model == nullptr) {
            m_center.reset();
            return;
        }
        const auto [u, v] = axes();
        // Local: baza centrată în origine, în sus pe Z → global: centrul ales, normala planului.
        glm::dmat4 m(1.0);
        m[0] = glm::dvec4(u, 0.0);
        m[1] = glm::dvec4(v, 0.0);
        m[2] = glm::dvec4(m_normal, 0.0);
        m[3] = glm::dvec4(*m_center, 1.0);
        const Vec3d size { width, width, height.value_or(width) };
        const geom::Primitive kind = m_kind;
        const std::string name = m_name;
        model::InstanceId created;
        ctx.stack.run(name, [&](model::Model& model) {
            model::Definition definition { name, true };
            constexpr std::array<const char*, 4> kShapes { "box", "cylinder", "cone", "sphere" };
            definition.shape = kShapes[static_cast<std::size_t>(kind)];
            const model::DefinitionId def = model.addDefinition(std::move(definition));
            geom::buildPrimitive(model.mesh(def), kind, size, 24);
            created = model.addInstance({ ctx.context, def, math::Transform(m), name, {}, false, false, {}, {}, {} });
        });
        if (created) {
            ctx.selection.clear();
            ctx.selection.instances.insert(created);
        }
        m_center.reset();
        m_engine.resetLock();
    }

    std::string_view m_id;
    geom::Primitive m_kind;
    std::string m_name;
    std::optional<Vec3d> m_center;
    Vec3d m_normal { 0, 0, 1 };
    Vec3d m_hoverNormal { 0, 0, 1 };
    Vec3d m_radius { 0.0 };
    bool m_heightStep = false;
    double m_height = 0.0;
    // Camera de la ultimul `track` (în coordonatele contextului), pentru conturul din previzualizare.
    Vec3d m_eye { 0.0 };
    Vec3d m_forward { 0.0, 0.0, -1.0 };
    bool m_perspective = true;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

} // namespace

std::unique_ptr<Tool> makePrimitiveTool(std::string_view id)
{
    if (id == "cube") {
        return std::make_unique<PrimitiveTool>("cube", geom::Primitive::Box, "Cub");
    }
    if (id == "cylinder") {
        return std::make_unique<PrimitiveTool>("cylinder", geom::Primitive::Cylinder, "Cilindru");
    }
    if (id == "cone") {
        return std::make_unique<PrimitiveTool>("cone", geom::Primitive::Cone, "Con");
    }
    if (id == "sphere") {
        return std::make_unique<PrimitiveTool>("sphere", geom::Primitive::Sphere, "Sferă");
    }
    return nullptr;
}

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
