#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Triangulate.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/units/Length.h"

#include "ToolHelpers.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <cstdio>
#include <numbers>

// Uneltele de măsurare și adnotare: Tape Measure, Protractor, Dimension, Text, Section Plane.
namespace aion::tools {

namespace {

constexpr double kDegree = std::numbers::pi / 180.0;

std::string formatAngle(double radians)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f°", radians / kDegree);
    return buffer;
}

std::optional<double> parseDegrees(std::string_view text)
{
    for (char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            return std::nullopt;
        }
    }
    return units::parseLength(text);
}

Vec3d anyPerpendicular(const Vec3d& n)
{
    const Vec3d a = std::abs(n.z) < 0.9 ? Vec3d { 0, 0, 1 } : Vec3d { 1, 0, 0 };
    return glm::normalize(glm::cross(a, n));
}

// --- Tape Measure ---------------------------------------------------------------------------

// Măsoară distanța dintre două puncte. Pornind de pe o muchie, lasă o linie de construcție paralelă cu ea prin
// punctul final; altfel, un punct de construcție. Option: doar măsoară.
class TapeMeasureTool final : public Tool {
public:
    std::string_view id() const override { return "tape"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (!m_start) {
            if (!m_inference.valid()) {
                return;
            }
            m_start = m_inference.point;
            m_current = *m_start;
            m_edgeDirection.reset();
            if (m_inference.kind == inference::Kind::OnEdge && m_inference.edge && ctx.mesh.edges().contains(m_inference.edge)) {
                const geom::Edge& edge = ctx.mesh.edge(m_inference.edge);
                m_edgeDirection = glm::normalize(ctx.mesh.position(edge.v1) - ctx.mesh.position(edge.v0));
            }
            m_engine.restartFrom(*m_start);
            return;
        }
        finish(ctx);
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
        m_start.reset();
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_start) {
            o.lines.push_back({ *m_start, m_current, true });
            o.points.push_back(*m_start);
            const double d = glm::length(m_current - *m_start);
            if (d > tol::kLength) {
                o.dimensions.push_back({ *m_start, m_current, units::formatLength(d) });
            }
        }
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        if (!m_start) {
            return "Alege primul punct (pe o muchie: o linie de construcție paralelă) · Option: doar măsoară";
        }
        return "Distanța: " + units::formatLength(glm::length(m_current - *m_start))
            + " · click sau tastează o lungime pentru ghidaj · Esc anulează";
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Lungime", m_start ? units::formatLength(glm::length(m_current - *m_start)) : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        const auto length = units::parseLength(text);
        if (!length || !m_start || glm::length(m_current - *m_start) <= tol::kLength) {
            return false;
        }
        m_current = *m_start + glm::normalize(m_current - *m_start) * *length;
        m_measureOnly = false;
        finish(ctx);
        return true;
    }

private:
    void track(ToolContext& ctx, const PointerEvent& e)
    {
        m_measureOnly = e.modifiers.alt;
        m_inference = m_engine.withGuides(ctx.guides).infer(ctx.mesh, ctx.camera, e.screen, m_start);
        if (m_inference.valid()) {
            m_current = m_inference.point;
        }
    }

    void finish(ToolContext& ctx)
    {
        if (m_start && !m_measureOnly && ctx.model != nullptr && glm::length(m_current - *m_start) > tol::kLength) {
            const model::Guide guide { ctx.context, m_current, m_edgeDirection.value_or(Vec3d { 0.0 }) };
            ctx.stack.run("Ghidaj", [&](model::Model& model) { model.addGuide(guide); });
        }
        m_start.reset();
        m_engine.resetLock();
    }

    std::optional<Vec3d> m_start;
    Vec3d m_current { 0.0 };
    std::optional<Vec3d> m_edgeDirection;
    bool m_measureOnly = false;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

// --- Protractor -----------------------------------------------------------------------------

// O linie de construcție la un unghi: centrul, direcția de referință, apoi unghiul (sau tastat).
class ProtractorTool final : public Tool {
public:
    std::string_view id() const override { return "protractor"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (m_phase == Phase::Center) {
            if (m_inference.valid()) {
                m_center = m_inference.point;
                const geom::FaceId face = m_inference.pick.face;
                m_normal = face && ctx.mesh.faces().contains(face) ? ctx.mesh.face(face).plane.normal : Vec3d { 0, 0, 1 };
                m_reference = m_center + anyPerpendicular(m_normal);
                m_phase = Phase::Reference;
                m_engine.restartFrom(m_center);
            }
            return;
        }
        if (m_phase == Phase::Reference) {
            if (glm::length(m_reference - m_center) > tol::kLength) {
                m_phase = Phase::Angle;
            }
            return;
        }
        commit(ctx);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_phase = Phase::Center;
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_phase != Phase::Center) {
            o.points.push_back(m_center);
            o.lines.push_back({ m_center, m_reference, true, inference::Tint::Neutral, true });
        }
        if (m_phase == Phase::Angle) {
            const Vec3d end = m_center + direction() * glm::length(m_reference - m_center);
            o.lines.push_back({ m_center, end, true, inference::Tint::Magenta, true });
            o.dimensions.push_back({ m_center, end, formatAngle(m_angle) });
        } else {
            appendInference(o, m_inference);
        }
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        switch (m_phase) {
        case Phase::Center:
            return "Alege vârful unghiului (pe o față: în planul ei)";
        case Phase::Reference:
            return "Alege direcția de referință";
        case Phase::Angle:
            return "Alege unghiul liniei de construcție sau tastează-l în grade";
        }
        return {};
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Unghi", m_phase == Phase::Angle ? formatAngle(m_angle) : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        const auto degrees = parseDegrees(text);
        if (m_phase != Phase::Angle || !degrees) {
            return false;
        }
        m_angle = (m_angle < 0.0 ? -*degrees : *degrees) * kDegree;
        commit(ctx);
        return true;
    }

private:
    enum class Phase { Center, Reference, Angle };

    [[nodiscard]] Vec3d direction() const
    {
        return math::Transform::rotation({ 0, 0, 0 }, m_normal, m_angle).applyToVector(glm::normalize(m_reference - m_center));
    }

    void track(ToolContext& ctx, const PointerEvent& e)
    {
        const std::optional<Vec3d> from = m_phase == Phase::Center ? std::nullopt : std::optional(m_center);
        m_inference = m_engine.withGuides(ctx.guides).infer(ctx.mesh, ctx.camera, e.screen, from);
        if (m_phase == Phase::Center || !m_inference.valid()) {
            return;
        }
        const math::Plane plane = math::Plane::fromPointNormal(m_center, m_normal);
        Vec3d p = plane.project(m_inference.point);
        if (m_inference.kind == inference::Kind::Ground || m_inference.kind == inference::Kind::OnFace) {
            const math::Ray ray = ctx.camera.rayAt(e.screen);
            if (const auto t = math::intersectRayPlane(ray, plane)) {
                p = ray.at(*t);
            } else if (const auto t2 = math::intersectRayPlane(ray, plane.flipped())) {
                p = ray.at(*t2);
            }
        }
        if (glm::length(p - m_center) <= tol::kLength) {
            return;
        }
        if (m_phase == Phase::Reference) {
            m_reference = p;
            return;
        }
        const Vec3d r = m_reference - m_center;
        const Vec3d c = p - m_center;
        double angle = std::atan2(glm::dot(glm::cross(r, c), m_normal), glm::dot(r, c));
        const double step = 15.0 * kDegree;
        const double snapped = std::round(angle / step) * step;
        if (std::abs(angle - snapped) <= 2.5 * kDegree) {
            angle = snapped;
        }
        m_angle = angle;
    }

    void commit(ToolContext& ctx)
    {
        if (ctx.model != nullptr) {
            const model::Guide guide { ctx.context, m_center, direction() };
            ctx.stack.run("Ghidaj", [&](model::Model& model) { model.addGuide(guide); });
        }
        m_phase = Phase::Center;
        m_engine.resetLock();
    }

    Phase m_phase = Phase::Center;
    Vec3d m_center { 0.0 };
    Vec3d m_normal { 0, 0, 1 };
    Vec3d m_reference { 0.0 };
    double m_angle = 0.0;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

// --- Dimension ------------------------------------------------------------------------------

// O cotă: două puncte, apoi poziția liniei de cotă (deplasarea ei față de segment).
class DimensionTool final : public Tool {
public:
    std::string_view id() const override { return "dimension"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        switch (m_phase) {
        case Phase::First:
            if (m_inference.valid()) {
                m_a = m_inference.point;
                m_b = m_a;
                m_phase = Phase::Second;
                m_engine.restartFrom(m_a);
            }
            return;
        case Phase::Second:
            if (glm::length(m_b - m_a) > tol::kLength) {
                m_phase = Phase::Offset;
            }
            return;
        case Phase::Offset:
            if (ctx.model != nullptr) {
                const model::Annotation annotation { model::Annotation::Kind::Dimension, ctx.context, m_a, m_b, m_offset, {} };
                ctx.stack.run("Cotă", [&](model::Model& model) { model.addAnnotation(annotation); });
            }
            m_phase = Phase::First;
            m_engine.resetLock();
            return;
        }
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_phase = Phase::First;
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_phase != Phase::First) {
            o.points.push_back(m_a);
        }
        if (m_phase == Phase::Second) {
            o.lines.push_back({ m_a, m_b, true });
        }
        if (m_phase == Phase::Offset) {
            const Vec3d a = m_a + m_offset;
            const Vec3d b = m_b + m_offset;
            o.lines.push_back({ m_a, a, false });
            o.lines.push_back({ m_b, b, false });
            o.lines.push_back({ a, b, false });
            o.dimensions.push_back({ a, b, units::formatLength(glm::length(m_b - m_a)) });
        } else {
            appendInference(o, m_inference);
        }
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        switch (m_phase) {
        case Phase::First:
            return "Alege primul punct al cotei";
        case Phase::Second:
            return "Alege al doilea punct";
        case Phase::Offset:
            return "Trage linia de cotă la distanța dorită și dă click";
        }
        return {};
    }

private:
    enum class Phase { First, Second, Offset };

    void track(ToolContext& ctx, const PointerEvent& e)
    {
        const std::optional<Vec3d> from = m_phase == Phase::Second ? std::optional(m_a) : std::nullopt;
        m_inference = m_engine.withGuides(ctx.guides).infer(ctx.mesh, ctx.camera, e.screen, from);
        if (m_phase == Phase::Second && m_inference.valid()) {
            m_b = m_inference.point;
        }
        if (m_phase == Phase::Offset) {
            // Deplasarea: perpendiculara pe segment din punctul de sub cursor, în planul care conține și camera.
            const Vec3d dir = glm::normalize(m_b - m_a);
            const math::Ray ray = ctx.camera.rayAt(e.screen);
            Vec3d normal = glm::cross(dir, ctx.camera.forward());
            if (glm::length(normal) < 1e-6) {
                normal = anyPerpendicular(dir);
            }
            const math::Plane plane = math::Plane::fromPointNormal(m_a, glm::normalize(glm::cross(dir, normal)));
            auto t = math::intersectRayPlane(ray, plane);
            if (!t) {
                t = math::intersectRayPlane(ray, plane.flipped());
            }
            if (t) {
                const Vec3d p = ray.at(*t);
                const Vec3d rel = p - m_a;
                m_offset = rel - dir * glm::dot(rel, dir);
            }
        }
    }

    Phase m_phase = Phase::First;
    Vec3d m_a { 0.0 };
    Vec3d m_b { 0.0 };
    Vec3d m_offset { 0.0 };
    inference::Engine m_engine;
    inference::Inference m_inference;
};

// --- Text -----------------------------------------------------------------------------------

// Un text ancorat într-un punct; implicit arată ce e acolo (aria feței, lungimea muchiei, coordonatele).
class TextTool final : public Tool {
public:
    std::string_view id() const override { return "text"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        pointerMove(ctx, e);
        if (!m_inference.valid() || ctx.model == nullptr) {
            return;
        }
        std::string text;
        if (m_inference.kind == inference::Kind::OnEdge && ctx.mesh.edges().contains(m_inference.edge)) {
            const geom::Edge& edge = ctx.mesh.edge(m_inference.edge);
            text = units::formatLength(glm::length(ctx.mesh.position(edge.v1) - ctx.mesh.position(edge.v0)));
        } else if (m_inference.pick.face && ctx.mesh.faces().contains(m_inference.pick.face)
            && m_inference.kind == inference::Kind::OnFace) {
            const geom::FaceTriangles tri = geom::triangulateFace(ctx.mesh, m_inference.pick.face);
            double area = 0.0;
            for (std::size_t i = 0; i + 2 < tri.indices.size(); i += 3) {
                const Vec3d a = tri.points[tri.indices[i]];
                area += 0.5 * glm::length(glm::cross(tri.points[tri.indices[i + 1]] - a, tri.points[tri.indices[i + 2]] - a));
            }
            char buffer[64];
            std::snprintf(buffer, sizeof buffer, "%.2f m²", area / 1e6);
            text = buffer;
        } else {
            const Vec3d p = m_inference.point;
            char buffer[96];
            std::snprintf(buffer, sizeof buffer, "%.0f, %.0f, %.0f", p.x, p.y, p.z);
            text = buffer;
        }
        // Eticheta: deasupra punctului, la o distanță vizibilă indiferent de zoom.
        const Vec3d offset = ctx.camera.up() * (ctx.camera.worldPerPixelAtTarget() * 60.0)
            + ctx.camera.right() * (ctx.camera.worldPerPixelAtTarget() * 40.0);
        const model::Annotation annotation { model::Annotation::Kind::Text, ctx.context, m_inference.point, {}, offset, text };
        model::AnnotationId id;
        ctx.stack.run("Text", [&](model::Model& model) { id = model.addAnnotation(annotation); });
        if (ctx.requests != nullptr) {
            ctx.requests->editAnnotation = id;
        }
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        m_inference = m_engine.withGuides(ctx.guides).infer(ctx.mesh, ctx.camera, e.screen, std::nullopt);
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    Overlay overlay() const override
    {
        Overlay o;
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        return "Click pe o față, o muchie sau un punct pentru un text (îl poți edita imediat)";
    }

private:
    inference::Engine m_engine;
    inference::Inference m_inference;
};

// --- Section Plane --------------------------------------------------------------------------

// Un plan de secțiune pe fața de sub cursor (sau orizontal pe sol): taie modelul pe partea dinspre cameră.
class SectionTool final : public Tool {
public:
    std::string_view id() const override { return "section"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        pointerMove(ctx, e);
        if (!m_inference.valid() || ctx.model == nullptr) {
            return;
        }
        // Planul se exprimă în coordonatele modelului; uneltele lucrează în cele ale contextului editat.
        model::SectionPlane section;
        section.point = ctx.toWorld.applyToPoint(m_inference.point);
        section.normal = glm::normalize(ctx.toWorld.applyToNormal(m_normal));
        section.active = true;
        ctx.stack.run("Secțiune", [&](model::Model& model) {
            for (model::SectionId other : std::vector(model.sections().keys())) {
                model::SectionPlane s = model.sections().at(other);
                if (s.active) {
                    s.active = false;
                    model.setSection(other, s);
                }
            }
            section.name = "Secțiune " + std::to_string(model.sections().size() + 1);
            model.addSection(section);
        });
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        m_inference = m_engine.withGuides(ctx.guides).infer(ctx.mesh, ctx.camera, e.screen, std::nullopt);
        const geom::FaceId face = m_inference.pick.face;
        m_normal = face && ctx.mesh.faces().contains(face) ? ctx.mesh.face(face).plane.normal : Vec3d { 0, 0, 1 };
        // Normala spre cameră: partea tăiată e cea pe care o vezi.
        const Vec3d toEye = ctx.camera.eye() - m_inference.point;
        if (glm::dot(m_normal, toEye) < 0.0) {
            m_normal = -m_normal;
        }
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_inference.valid()) {
            const Vec3d u = anyPerpendicular(m_normal);
            const Vec3d v = glm::cross(m_normal, u);
            const double s = 600.0;
            const Vec3d c = m_inference.point;
            const Vec3d p[4] { c + (u + v) * s, c + (u - v) * s, c - (u + v) * s, c - (u - v) * s };
            for (int i = 0; i < 4; ++i) {
                o.lines.push_back({ p[i], p[(i + 1) % 4], false, inference::Tint::Magenta, true });
            }
            o.lines.push_back({ c, c + m_normal * s, true, inference::Tint::Magenta, true });
        }
        appendInference(o, m_inference);
        return o;
    }

    std::string status() const override { return "Click pe o față pentru un plan de secțiune paralel cu ea (sau pe sol)"; }

private:
    inference::Engine m_engine;
    inference::Inference m_inference;
    Vec3d m_normal { 0, 0, 1 };
};

} // namespace

std::unique_ptr<Tool> makeTapeMeasureTool()
{
    return std::make_unique<TapeMeasureTool>();
}

std::unique_ptr<Tool> makeProtractorTool()
{
    return std::make_unique<ProtractorTool>();
}

std::unique_ptr<Tool> makeDimensionTool()
{
    return std::make_unique<DimensionTool>();
}

std::unique_ptr<Tool> makeTextTool()
{
    return std::make_unique<TextTool>();
}

std::unique_ptr<Tool> makeSectionTool()
{
    return std::make_unique<SectionTool>();
}

} // namespace aion::tools
