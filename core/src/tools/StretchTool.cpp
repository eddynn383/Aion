#include "ToolHelpers.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Stretch.h"
#include "aion/core/units/Length.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>

// „Întinde” (mobilier, pasul 11): clic pe o față a corpului alege axa (normala ei); planul de tăiere urmează
// cursorul, iar al doilea clic îl fixează; cursorul dă apoi distanța, cu previzualizare; al treilea clic
// confirmă. VCB, oricând după primul clic: „200” (planul ales, sau la mijloc) ori „200/2” (în 2 locuri
// egale).
namespace aion::tools {

namespace {

class StretchTool final : public Tool {
public:
    std::string_view id() const override { return "stretch"; }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        if (m_placing) {
            m_inference = inferAt(m_engine, ctx, e.screen, std::nullopt);
            if (m_inference.valid()) {
                placePlane(ctx, m_inference.point);
            }
            return;
        }
        m_inference = inferAt(m_engine, ctx, e.screen, m_start);
        if (m_body && m_inference.valid()) {
            preview(ctx, glm::dot(m_inference.point - *m_start, m_direction));
        }
    }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        if (m_placing) {
            // Planul rămâne unde e; de aici cursorul dă distanța.
            m_placing = false;
            m_inference = inferAt(m_engine, ctx, e.screen, std::nullopt);
            m_start = m_inference.valid() ? m_inference.point : m_start;
            m_engine.restartFrom(*m_start);
            return;
        }
        m_inference = inferAt(m_engine, ctx, e.screen, m_start);
        if (m_body) {
            finish(ctx);
            return;
        }
        begin(ctx);
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Escape && m_body) {
            cancel(ctx);
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& ctx) override
    {
        if (ctx.stack.inTransaction()) {
            ctx.stack.abort();
        }
        m_body = {};
        m_start.reset();
        m_placing = false;
        m_planeLines.clear();
        m_distance = 0.0;
        m_refusal = model::StretchRefusal::None;
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        for (const auto& [a, b] : m_planeLines) {
            o.lines.push_back({ a, b, true });
        }
        appendInference(o, m_inference);
        return o;
    }

    view::PickResult hover() const override { return inferenceHover(m_inference); }

    std::string status() const override
    {
        if (!m_body) {
            return "Întinde: clic pe o față a corpului (axa = normala ei)";
        }
        switch (m_refusal) {
        case model::StretchRefusal::ThroughThickness:
            return "Nu se poate: planul taie o placă prin grosime · Esc";
        case model::StretchRefusal::ModifiedBoard:
            return "Nu se poate: planul taie o placă modificată de mână sau desprinsă · Esc";
        case model::StretchRefusal::FreeGeometry:
            return "Nu se poate: planul taie geometrie care nu e placă · Esc";
        default:
            break;
        }
        if (m_placing) {
            return "Mută planul unde tai, clic îl fixează · sau tastează 200/2 (în 2 locuri egale) · Esc";
        }
        return "Mișcă pentru distanță, clic confirmă · tastează 200 sau 200/2 (în 2 locuri egale) · Esc";
    }

    std::optional<Measurement> measurement() const override
    {
        if (!m_body) {
            return std::nullopt;
        }
        return Measurement { "Distanța", units::formatLength(m_distance) };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        if (!m_body) {
            return false;
        }
        // „200/2”: distanța și numărul de locuri egale; fără „/N”, planul ales (sau cel de la mijloc).
        std::string_view length = text;
        if (const auto slash = text.find('/'); slash != std::string_view::npos) {
            length = text.substr(0, slash);
            const int places = std::max(1, std::atoi(std::string(text.substr(slash + 1)).c_str()));
            m_planes = model::equalPlanes(*ctx.model, m_body, m_axis, places);
        }
        const auto value = units::parseLength(length);
        if (!value) {
            return false;
        }
        preview(ctx, *value);
        return finish(ctx);
    }

private:
    void begin(ToolContext& ctx)
    {
        if (ctx.model == nullptr || !m_inference.valid() || !m_inference.sourceInstance) {
            return;
        }
        const model::InstanceId body = m_inference.sourceInstance;
        const math::Transform& t = ctx.model->instance(body).transform;
        const Vec3d normal = inferenceNormal(ctx, m_inference, { 0.0, 0.0, 1.0 });
        // Axa corpului cea mai apropiată de normala feței.
        const Vec3d local = t.inverse().applyToVector(normal);
        m_axis = 0;
        for (int k = 1; k < 3; ++k) {
            if (std::abs(local[k]) > std::abs(local[m_axis])) {
                m_axis = k;
            }
        }
        Vec3d axis { 0.0 };
        axis[m_axis] = local[m_axis] < 0.0 ? -1.0 : 1.0;
        m_sign = axis[m_axis];
        m_direction = glm::normalize(t.applyToVector(axis));
        m_body = body;
        m_start = m_inference.point;
        m_planes = model::equalPlanes(*ctx.model, body, m_axis, 1);
        m_placing = true;
        updatePlaneLines(ctx);
    }

    // Planul de tăiere la poziția punctului de sub cursor (pe axa corpului), în interiorul corpului.
    void placePlane(const ToolContext& ctx, const Vec3d& point)
    {
        const model::Instance& inst = ctx.model->instance(m_body);
        const auto [lo, hi] = model::bounds(*ctx.model, inst.definition);
        const double at = inst.transform.inverse().applyToPoint(point)[m_axis];
        m_planes = { std::clamp(at, lo[m_axis] + 1.0, hi[m_axis] - 1.0) };
        updatePlaneLines(ctx);
    }

    // Previzualizarea: tranzacția uneltei refăcută la fiecare mișcare.
    void preview(ToolContext& ctx, double distance)
    {
        if (ctx.stack.inTransaction()) {
            ctx.stack.abort();
        }
        m_distance = distance;
        m_refusal = model::StretchRefusal::None;
        if (std::abs(distance) < 1e-6) {
            return;
        }
        ctx.stack.beginTransaction("Întinde");
        const model::StretchResult r = model::stretch(*ctx.model, m_body, m_axis, m_planes, distance);
        if (!r.ok) {
            m_refusal = r.reason;
            ctx.stack.abort();
            return;
        }
        if (m_sign < 0.0) {
            // Fața apucată e pe partea − a corpului, iar corpul a crescut spre +: îl mutăm spre fața apucată,
            // ca ea să urmeze cursorul și cealaltă să rămână pe loc.
            model::Instance inst = ctx.model->instance(m_body);
            inst.transform = math::Transform::translation(m_direction * distance) * inst.transform;
            ctx.model->setInstance(m_body, inst);
        }
    }

    bool finish(ToolContext& ctx)
    {
        const bool ok = ctx.stack.inTransaction() && m_refusal == model::StretchRefusal::None;
        if (ok) {
            ctx.stack.commit();
        } else if (ctx.stack.inTransaction()) {
            ctx.stack.abort();
        }
        m_body = {};
        m_start.reset();
        m_placing = false;
        m_planeLines.clear();
        m_engine.reset();
        return ok;
    }

    // Planele, ca dreptunghiuri punctate peste cutia corpului (în coordonatele contextului).
    void updatePlaneLines(const ToolContext& ctx)
    {
        m_planeLines.clear();
        const model::Instance& inst = ctx.model->instance(m_body);
        const auto [lo, hi] = model::bounds(*ctx.model, inst.definition);
        const int u = (m_axis + 1) % 3;
        const int v = (m_axis + 2) % 3;
        for (const double at : m_planes) {
            std::array<Vec3d, 4> c {};
            for (int k = 0; k < 4; ++k) {
                Vec3d p { 0.0 };
                p[m_axis] = at;
                p[u] = (k == 1 || k == 2) ? hi[u] : lo[u];
                p[v] = (k >= 2) ? hi[v] : lo[v];
                c[static_cast<std::size_t>(k)] = inst.transform.applyToPoint(p);
            }
            for (std::size_t k = 0; k < 4; ++k) {
                m_planeLines.emplace_back(c[k], c[(k + 1) % 4]);
            }
        }
    }

    model::InstanceId m_body;
    int m_axis = 0;
    double m_sign = 1.0;
    Vec3d m_direction { 1.0, 0.0, 0.0 };
    std::optional<Vec3d> m_start;
    bool m_placing = false; // planul urmează cursorul (între primul și al doilea clic)
    std::vector<double> m_planes;
    std::vector<std::pair<Vec3d, Vec3d>> m_planeLines;
    double m_distance = 0.0;
    model::StretchRefusal m_refusal = model::StretchRefusal::None;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

} // namespace

std::unique_ptr<Tool> makeStretchTool()
{
    return std::make_unique<StretchTool>();
}

} // namespace aion::tools
