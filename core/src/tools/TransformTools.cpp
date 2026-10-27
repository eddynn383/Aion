#include "aion/core/geom/Operations.h"
#include "aion/core/model/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/units/Length.h"

#include "ToolHelpers.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <functional>
#include <numbers>

// Uneltele de transformare: Move, Rotate, Scale. Lucrează pe selecție sau, dacă nu e nimic selectat,
// pe entitatea de sub cursor (ca în SketchUp). Option face o copie; după copiere, „x3” sau „/3” în VCB
// face un șir de copii (array).
namespace aion::tools {

namespace {

using math::Transform;

constexpr double kDegree = std::numbers::pi / 180.0;
// Unghiurile se „prind” de multiplii lui 15° când cursorul e aproape.
constexpr double kAngleSnapStep = 15.0 * kDegree;
constexpr double kAngleSnapTolerance = 2.5 * kDegree;

// Ce transformă unealta: entitățile (pentru copiere) și vârfurile lor (pentru mutare).
struct Target {
    geom::Selection selection;
    std::set<geom::VertexId> vertices;

    [[nodiscard]] bool empty() const { return vertices.empty() && selection.instances.empty(); }
};

Target targetAt(const ToolContext& ctx, Vec2d screen)
{
    Target t;
    if (!ctx.selection.empty()) {
        t.selection = ctx.selection;
    } else {
        const view::PickResult hit = pickWithInstances(ctx, screen);
        switch (hit.kind) {
        case view::PickResult::Kind::Instance:
            t.selection.instances.insert(hit.instance);
            break;
        case view::PickResult::Kind::Vertex:
            t.vertices.insert(hit.vertex);
            return t;
        case view::PickResult::Kind::Edge:
            t.selection.edges.insert(hit.edge);
            break;
        case view::PickResult::Kind::Face:
            t.selection.faces.insert(hit.face);
            break;
        case view::PickResult::Kind::None:
            break;
        }
    }
    // Obiectele blocate nu se mută, nu se rotesc și nu se scalează.
    if (ctx.model != nullptr) {
        std::erase_if(t.selection.instances, [&](model::InstanceId i) { return ctx.model->instance(i).locked; });
    }
    t.vertices = geom::verticesOf(ctx.mesh, t.selection);
    return t;
}

// Ce se evidențiază la hover când nu e nimic selectat: entitatea pe care ar lucra unealta.
view::PickResult targetHover(const ToolContext& ctx, Vec2d screen)
{
    return ctx.selection.empty() ? pickWithInstances(ctx, screen) : view::PickResult {};
}

// Muchiile atinse de transformare, pentru previzualizare.
struct PreviewEdge {
    Vec3d a;
    Vec3d b;
    bool movesA = false;
    bool movesB = false;
};

std::vector<PreviewEdge> previewEdges(const ToolContext& ctx, const Target& target)
{
    const geom::Mesh& mesh = ctx.mesh;
    const std::set<geom::VertexId>& vertices = target.vertices;
    std::vector<PreviewEdge> out;
    // Instanțele: muchiile cutiei lor, mutate întregi.
    if (ctx.model != nullptr) {
        for (model::InstanceId id : target.selection.instances) {
            const model::Instance& inst = ctx.model->instance(id);
            const auto [lo, hi] = model::bounds(*ctx.model, inst.definition);
            if (lo.x > hi.x) {
                continue;
            }
            std::array<Vec3d, 8> c {};
            for (std::size_t k = 0; k < 8; ++k) {
                c[k] = inst.transform.applyToPoint(
                    { (k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z });
            }
            constexpr std::array<std::pair<int, int>, 12> kEdges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 },
                { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
            for (const auto& [a, b] : kEdges) {
                out.push_back({ c[std::size_t(a)], c[std::size_t(b)], true, true });
            }
        }
    }
    for (std::size_t i = 0; i < mesh.edges().size(); ++i) {
        const geom::Edge& e = mesh.edges().values()[i];
        const bool a = vertices.contains(e.v0);
        const bool b = vertices.contains(e.v1);
        if (a || b) {
            out.push_back({ mesh.position(e.v0), mesh.position(e.v1), a, b });
        }
    }
    return out;
}

// Punctele de prindere ale obiectelor (Move): colțurile și mijloacele muchiilor cutiei fiecăruia, orientate după
// obiect (o cutie rotită are punctele pe muchiile ei, nu pe ale cutiei aliniate cu axele).
std::vector<Grip> instanceGrips(const ToolContext& ctx, const std::set<model::InstanceId>& ids)
{
    std::vector<Grip> out;
    if (ctx.model == nullptr) {
        return out;
    }
    constexpr std::array<std::pair<int, int>, 12> kEdges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 }, { 1, 3 },
        { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
    for (model::InstanceId id : ids) {
        if (!ctx.model->instances().contains(id)) {
            continue;
        }
        const model::Instance& inst = ctx.model->instance(id);
        const auto [lo, hi] = model::bounds(*ctx.model, inst.definition);
        if (lo.x > hi.x) {
            continue;
        }
        std::array<Vec3d, 8> c {};
        for (std::size_t k = 0; k < 8; ++k) {
            c[k] = inst.transform.applyToPoint({ (k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z });
        }
        // Fețele cutiei văzute de cameră: un punct e vizibil dacă stă pe cel puțin una dintre ele.
        const Vec3d center = inst.transform.applyToPoint((lo + hi) * 0.5);
        std::array<bool, 6> facing {}; // −X, +X, −Y, +Y, −Z, +Z (local)
        bool flat = false;
        for (int a = 0; a < 3; ++a) {
            flat = flat || hi[a] - lo[a] < tol::kLength;
            for (int side = 0; side < 2; ++side) {
                Vec3d local = (lo + hi) * 0.5;
                local[a] = side == 0 ? lo[a] : hi[a];
                const Vec3d p = inst.transform.applyToPoint(local);
                const Vec3d outward = p - center;
                const Vec3d toEye = ctx.camera.projection() == view::Projection::Orthographic ? -ctx.camera.forward()
                                                                                               : ctx.camera.eye() - p;
                facing[std::size_t(a * 2 + side)] = glm::dot(outward, toEye) > 0.0;
            }
        }
        const auto visible = [&](std::size_t k) { // colțul k (biții = partea pe X, Y, Z)
            return flat || facing[(k & 1) ? 1 : 0] || facing[(k & 2) ? 3 : 2] || facing[(k & 4) ? 5 : 4];
        };
        const std::size_t first = out.size();
        const auto add = [&](const Vec3d& p, inference::Kind kind, bool hidden) {
            // O cutie turtită (o placă, o formă plană) are colțuri și mijloace care coincid: fiecare punct o dată.
            for (std::size_t i = first; i < out.size(); ++i) {
                if (glm::length(out[i].point - p) < tol::kLength) {
                    out[i].hidden = out[i].hidden && hidden;
                    return;
                }
            }
            out.push_back({ p, kind, false, hidden });
        };
        for (std::size_t k = 0; k < 8; ++k) {
            add(c[k], inference::Kind::Endpoint, !visible(k));
        }
        for (const auto& [a, b] : kEdges) {
            // Mijlocul unei muchii e vizibil dacă una dintre cele două fețe ale muchiei e văzută.
            const auto ka = std::size_t(a);
            const auto kb = std::size_t(b);
            bool seen = flat;
            for (int axis = 0; axis < 3; ++axis) {
                const std::size_t bit = std::size_t(1) << axis;
                if ((ka & bit) == (kb & bit)) { // muchia stă pe fața de pe această axă
                    seen = seen || facing[std::size_t(axis * 2) + ((ka & bit) ? 1 : 0)];
                }
            }
            add((c[ka] + c[kb]) * 0.5, inference::Kind::Midpoint, !seen);
        }
    }
    return out;
}

// Indexul punctului de prindere cel mai apropiat de cursor (în pixeli), sau -1.
int nearestGrip(const ToolContext& ctx, const std::vector<Grip>& grips, Vec2d screen, double radiusPx, bool hidden);

// Cel mai apropiat punct vizibil; doar dacă nu e niciunul, unul din spatele obiectului.
int nearestGrip(const ToolContext& ctx, const std::vector<Grip>& grips, Vec2d screen, double radiusPx)
{
    const int visible = nearestGrip(ctx, grips, screen, radiusPx, false);
    return visible >= 0 ? visible : nearestGrip(ctx, grips, screen, radiusPx, true);
}

int nearestGrip(const ToolContext& ctx, const std::vector<Grip>& grips, Vec2d screen, double radiusPx, bool hidden)
{
    int best = -1;
    double bestDistance = radiusPx;
    for (std::size_t i = 0; i < grips.size(); ++i) {
        if (grips[i].hidden != hidden) {
            continue;
        }
        if (const auto s = ctx.camera.project(grips[i].point)) {
            const double d = glm::length(*s - screen);
            // La egalitate (puncte suprapuse pe ecran) câștigă colțul.
            if (d < bestDistance - 0.5 || (d < bestDistance + 0.5 && best >= 0 && grips[i].kind == inference::Kind::Endpoint
                    && grips[std::size_t(best)].kind != inference::Kind::Endpoint)) {
                bestDistance = std::min(d, bestDistance);
                best = static_cast<int>(i);
            }
        }
    }
    return best;
}

void appendPreview(Overlay& o, const std::vector<PreviewEdge>& edges, const Transform& t)
{
    for (const PreviewEdge& e : edges) {
        o.lines.push_back({ e.movesA ? t.applyToPoint(e.a) : e.a, e.movesB ? t.applyToPoint(e.b) : e.b, false });
    }
}

// Un număr simplu („45”, „1.5”, „-2”), fără unități.
std::optional<double> parseNumber(std::string_view text)
{
    for (char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'' || c == '"') {
            return std::nullopt;
        }
    }
    return units::parseLength(text);
}

// „x3”, „3x”, „*3” = încă copii la aceeași distanță; „/3”, „3/” = copii împărțite pe distanța dată.
struct ArrayRequest {
    bool divide = false;
    int count = 0;
};

std::optional<ArrayRequest> parseArray(std::string_view text)
{
    while (!text.empty() && text.front() == ' ') {
        text.remove_prefix(1);
    }
    while (!text.empty() && text.back() == ' ') {
        text.remove_suffix(1);
    }
    if (text.size() < 2) {
        return std::nullopt;
    }
    ArrayRequest r;
    std::string_view digits;
    const char first = text.front();
    const char last = text.back();
    if (first == 'x' || first == 'X' || first == '*' || first == '/') {
        r.divide = first == '/';
        digits = text.substr(1);
    } else if (last == 'x' || last == 'X' || last == '*' || last == '/') {
        r.divide = last == '/';
        digits = text.substr(0, text.size() - 1);
    } else {
        return std::nullopt;
    }
    for (char c : digits) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        r.count = r.count * 10 + (c - '0');
        if (r.count > 1000) {
            return std::nullopt;
        }
    }
    if (r.count < 1) {
        return std::nullopt;
    }
    return r;
}

std::string formatAngle(double radians)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.1f", radians / kDegree);
    std::string s = buffer;
    if (s.size() > 2 && s.substr(s.size() - 2) == ".0") {
        s.resize(s.size() - 2);
    }
    if (s == "-0") {
        s = "0";
    }
    return s + "°";
}

std::string formatFactor(double factor)
{
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "%.3f", factor);
    std::string s = buffer;
    while (!s.empty() && s.back() == '0') {
        s.pop_back();
    }
    if (!s.empty() && s.back() == '.') {
        s.pop_back();
    }
    return s;
}

// Mărimea unui pixel (în mm) la adâncimea punctului dat.
double worldPerPixelAt(const view::Camera& camera, const Vec3d& p)
{
    const auto a = camera.project(p);
    const auto b = camera.project(p + camera.right() * 1000.0);
    if (!a || !b || glm::length(*b - *a) < 1e-6) {
        return camera.worldPerPixelAtTarget();
    }
    return 1000.0 / glm::length(*b - *a);
}

// Aplică transformarea pe țintă: vârfurile geometriei și instanțele (într-o singură comandă).
void transformTarget(ToolContext& ctx, const std::string& name, const Target& target, const Transform& t)
{
    if (ctx.model == nullptr) {
        ctx.stack.run(name, [&](geom::Mesh& mesh) { geom::transformVertices(mesh, target.vertices, t); });
        return;
    }
    ctx.stack.run(name, [&](model::Model& model) {
        geom::transformVertices(model.mesh(ctx.context), target.vertices, t);
        model::transformInstances(model, target.selection.instances, t);
    });
}

// Copiile țintei, câte una pentru fiecare transformare (array). Instanțele unei componente rămân legate.
geom::Selection copyTarget(ToolContext& ctx, const geom::Selection& source, const std::vector<Transform>& transforms)
{
    geom::Selection copies;
    const auto copyGeometry = [&](geom::Mesh& mesh) {
        for (const Transform& t : transforms) {
            copies.add(geom::copySelection(mesh, { source.edges, source.faces, {} }, t));
        }
    };
    if (ctx.model == nullptr) {
        ctx.stack.run("Copy", copyGeometry);
        return copies;
    }
    ctx.stack.run("Copy", [&](model::Model& model) {
        copyGeometry(model.mesh(ctx.context));
        for (const Transform& t : transforms) {
            for (model::InstanceId i : source.instances) {
                if (model.instances().contains(i)) {
                    copies.instances.insert(model::copyInstance(model, i, t));
                }
            }
        }
    });
    return copies;
}

// Ultima copie, pentru array: entitățile copiate și transformarea unei copii, generată din pas.
struct LastCopy {
    geom::Selection source;
    std::function<Transform(double)> step; // step(k) = transformarea copiei k (k poate fi fracționar)
};

// Aplică un „x3” / „/3” după o copiere. Întoarce false dacă nu a existat o copiere.
bool applyArray(ToolContext& ctx, std::optional<LastCopy>& last, const ArrayRequest& request)
{
    if (!last || ctx.stack.undoName() != "Copy") {
        return false;
    }
    std::vector<Transform> steps;
    if (request.divide) {
        // Copia existentă se înlocuiește cu `count` copii împărțite pe aceeași distanță.
        ctx.stack.undo();
        for (int k = 1; k <= request.count; ++k) {
            steps.push_back(last->step(double(k) / request.count));
        }
        ctx.selection = copyTarget(ctx, last->source, steps);
    } else {
        for (int k = 2; k <= request.count; ++k) {
            steps.push_back(last->step(double(k)));
        }
        geom::Selection copies = ctx.selection;
        copies.add(copyTarget(ctx, last->source, steps));
        ctx.selection = copies;
    }
    ctx.selection.prune(ctx.mesh);
    last.reset();
    return true;
}

// --- Move -----------------------------------------------------------------------------------

class MoveTool final : public Tool {
public:
    std::string_view id() const override { return "move"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        m_buttonDown = true;
        m_press = e.screen;
        track(ctx, e);
        if (!m_base) {
            m_target = targetAt(ctx, e.screen);
            // Un punct de prindere al obiectului de sub cursor (fără selecție): obiectul lui e ținta, chiar dacă
            // punctul e puțin în afara conturului.
            if (m_hotGrip >= 0 && ctx.selection.empty() && m_gripOwner) {
                m_target = Target {};
                m_target.selection.instances.insert(m_gripOwner);
                if (ctx.model != nullptr && ctx.model->instance(m_gripOwner).locked) {
                    m_target.selection.instances.clear();
                }
            }
            if (m_target.empty() || (!m_inference.valid() && m_hotGrip < 0)) {
                return;
            }
            m_base = m_hotGrip >= 0 ? m_grips[std::size_t(m_hotGrip)].point : m_inference.point;
            m_current = *m_base;
            m_preview = previewEdges(ctx, m_target);
            m_engine.restartFrom(*m_base);
            m_dragging = false;
            return;
        }
        commit(ctx);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        if (m_buttonDown && glm::length(e.screen - m_press) > kDragThresholdPx) {
            m_dragging = true;
        }
        track(ctx, e);
    }

    void pointerUp(ToolContext& ctx, const PointerEvent& /*e*/) override
    {
        m_buttonDown = false;
        if (m_base && m_dragging) {
            commit(ctx);
        }
    }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (handleArrow(m_engine, e.key)) {
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_base.reset();
        m_dragging = false;
        m_engine.reset();
        m_shiftLock = false;
        m_grips.clear();
        m_hotGrip = -1;
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_base) {
            appendPreview(o, m_preview, Transform::translation(m_current - *m_base));
            o.lines.push_back({ *m_base, m_current, true });
            o.points.push_back(*m_base);
            const double d = glm::length(m_current - *m_base);
            if (d > tol::kLength) {
                o.dimensions.push_back({ *m_base, m_current, units::formatLength(d) });
            }
        }
        appendInference(o, m_inference);
        o.grips = m_grips;
        if (m_hotGrip >= 0) {
            const Grip& g = m_grips[std::size_t(m_hotGrip)];
            o.marker = InferenceMarker { g.point, g.kind, inference::Tint::Neutral,
                g.kind == inference::Kind::Endpoint ? "Colț" : "Mijlocul muchiei", false };
        }
        return o;
    }

    view::PickResult hover() const override { return m_base ? view::PickResult {} : m_hover; }

    std::string status() const override
    {
        if (m_base) {
            return "Alege punctul de destinație sau tastează distanța · Option: copiază · săgeți/Shift: blochează · Esc";
        }
        return m_lastCopy ? "Tastează x3 sau /3 pentru mai multe copii · sau alege ce muți"
                          : "Alege punctul de pornire: un colț sau mijlocul unei muchii al obiectului, sau orice punct";
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Distanță", m_base ? units::formatLength(glm::length(m_current - *m_base)) : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        if (!m_base) {
            const auto array = parseArray(text);
            return array && applyArray(ctx, m_lastCopy, *array);
        }
        const auto length = units::parseLength(text);
        const Vec3d d = m_current - *m_base;
        if (!length || std::abs(*length) <= tol::kLength || glm::length(d) <= tol::kLength) {
            return false;
        }
        m_current = *m_base + glm::normalize(d) * *length;
        commit(ctx);
        return true;
    }

private:
    void track(ToolContext& ctx, const PointerEvent& e)
    {
        m_copy = e.modifiers.alt;
        updateShiftLock(m_engine, e.modifiers.shift, m_shiftLock);
        m_inference = m_engine.withGuides(ctx.guides).withGridSnap(ctx.gridStep).infer(ctx.mesh, ctx.camera, e.screen, m_base);
        if (m_base && m_inference.valid()) {
            m_current = m_inference.point;
        }
        m_hover = m_base ? view::PickResult {} : targetHover(ctx, e.screen);
        updateGrips(ctx, e.screen);
    }

    // Punctele de prindere: înainte de pornire, ale obiectelor selectate (sau al celui de sub cursor); în timpul
    // mutării, ale obiectului de sub cursor (destinația), fără cele mutate. Cel apropiat de cursor e „fierbinte”.
    void updateGrips(ToolContext& ctx, Vec2d screen)
    {
        std::set<model::InstanceId> ids;
        m_gripOwner = {};
        if (!m_base && !ctx.selection.empty()) {
            ids = ctx.selection.instances;
        } else {
            const view::PickResult hit = pickWithInstances(ctx, screen);
            if (hit.kind == view::PickResult::Kind::Instance && !(m_base && m_target.selection.instances.contains(hit.instance))) {
                ids.insert(hit.instance);
                m_gripOwner = hit.instance;
            } else if (ctx.model != nullptr) {
                // Cursorul e lângă un obiect, dar nu pe el (un colț văzut din afară): cel mai apropiat punct al
                // obiectelor vizibile din context.
                double best = kGripRadiusPx;
                for (std::size_t i = 0; i < ctx.model->instances().size(); ++i) {
                    const model::InstanceId id = ctx.model->instances().keys()[i];
                    const model::Instance& inst = ctx.model->instances().values()[i];
                    if (inst.parent != ctx.context || inst.hidden || (m_base && m_target.selection.instances.contains(id))) {
                        continue;
                    }
                    const std::vector<Grip> grips = instanceGrips(ctx, { id });
                    const int k = nearestGrip(ctx, grips, screen, best);
                    if (k >= 0) {
                        if (const auto sp = ctx.camera.project(grips[std::size_t(k)].point)) {
                            best = glm::length(*sp - screen);
                        }
                        ids = { id };
                        m_gripOwner = id;
                    }
                }
            }
        }
        m_grips = instanceGrips(ctx, ids);
        m_hotGrip = nearestGrip(ctx, m_grips, screen, kGripRadiusPx);
        if (m_hotGrip >= 0) {
            m_grips[std::size_t(m_hotGrip)].hot = true;
            if (m_base && !m_shiftLock) {
                m_current = m_grips[std::size_t(m_hotGrip)].point;
            }
        }
    }

    void commit(ToolContext& ctx)
    {
        const Vec3d delta = m_current - *m_base;
        if (glm::length(delta) > tol::kLength) {
            const Transform t = Transform::translation(delta);
            if (m_copy) {
                const geom::Selection source = m_target.selection;
                ctx.selection = copyTarget(ctx, source, { t });
                m_lastCopy = LastCopy { source, [delta](double k) { return Transform::translation(delta * k); } };
            } else {
                transformTarget(ctx, "Move", m_target, t);
                m_lastCopy.reset();
            }
            ctx.selection.prune(ctx.mesh);
        }
        m_base.reset();
        m_dragging = false;
        m_engine.resetLock();
        m_shiftLock = false;
    }

    static constexpr double kGripRadiusPx = 14.0;

    Target m_target;
    std::vector<Grip> m_grips;
    int m_hotGrip = -1;
    model::InstanceId m_gripOwner;
    std::optional<Vec3d> m_base;
    Vec3d m_current { 0.0 };
    std::vector<PreviewEdge> m_preview;
    bool m_copy = false;
    bool m_buttonDown = false;
    bool m_dragging = false;
    Vec2d m_press { 0.0 };
    std::optional<LastCopy> m_lastCopy;
    inference::Engine m_engine;
    inference::Inference m_inference;
    bool m_shiftLock = false;
    view::PickResult m_hover;
};

// --- Rotate ---------------------------------------------------------------------------------

class RotateTool final : public Tool {
public:
    std::string_view id() const override { return "rotate"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        switch (m_phase) {
        case Phase::Center: {
            m_target = targetAt(ctx, e.screen);
            if (m_target.empty() || !m_inference.valid()) {
                return;
            }
            m_center = m_inference.point;
            m_normal = m_axisLock ? inference::axisDirection(*m_axisLock) : m_hoverNormal;
            m_preview = previewEdges(ctx, m_target);
            m_reference = m_center;
            m_phase = Phase::Reference;
            return;
        }
        case Phase::Reference:
            if (glm::length(m_reference - m_center) > tol::kLength) {
                m_phase = Phase::Angle;
                m_angle = 0.0;
            }
            return;
        case Phase::Angle:
            commit(ctx);
            return;
        }
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        // Săgețile aleg axa de rotație (planul raportorului) înainte de a fixa centrul.
        const auto axis = e.key == Key::Right ? inference::Axis::X
            : e.key == Key::Left              ? inference::Axis::Y
            : e.key == Key::Up                ? inference::Axis::Z
                                              : inference::Axis::None;
        if (axis != inference::Axis::None && m_phase == Phase::Center) {
            m_axisLock = m_axisLock == axis ? std::nullopt : std::optional(axis);
            m_hoverNormal = m_axisLock ? inference::axisDirection(*m_axisLock) : Vec3d { 0, 0, 1 };
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_phase = Phase::Center;
        m_engine.reset();
    }

    Overlay overlay() const override
    {
        Overlay o;
        const Vec3d center = m_phase == Phase::Center ? m_inference.point : m_center;
        const Vec3d normal = m_phase == Phase::Center ? m_hoverNormal : m_normal;
        if (m_phase == Phase::Center && !m_inference.valid()) {
            return o;
        }
        // Raportorul: un cerc în planul de rotație, în culoarea axei (dacă e paralel cu una).
        const double radius = m_protractorRadius;
        const Vec3d u = anyPerpendicular(normal);
        const Vec3d v = glm::cross(normal, u);
        const inference::Tint tint = axisTint(normal);
        constexpr int kSegments = 48;
        for (int i = 0; i < kSegments; ++i) {
            const double a0 = 2.0 * std::numbers::pi * i / kSegments;
            const double a1 = 2.0 * std::numbers::pi * (i + 1) / kSegments;
            o.lines.push_back({ center + (u * std::cos(a0) + v * std::sin(a0)) * radius,
                center + (u * std::cos(a1) + v * std::sin(a1)) * radius, false, tint, true });
        }
        if (m_phase != Phase::Center) {
            o.lines.push_back({ m_center, m_reference, true, inference::Tint::Neutral, true });
            o.points.push_back(m_center);
        }
        if (m_phase == Phase::Angle) {
            const Transform t = rotation();
            appendPreview(o, m_preview, t);
            const Vec3d end = t.applyToPoint(m_reference);
            o.lines.push_back({ m_center, end, false, inference::Tint::Neutral, true });
            o.dimensions.push_back({ m_center, end, formatAngle(m_angle) });
        }
        if (m_phase != Phase::Angle) {
            appendInference(o, m_inference);
        }
        return o;
    }

    view::PickResult hover() const override { return m_phase == Phase::Center ? m_hover : view::PickResult {}; }

    std::string status() const override
    {
        switch (m_phase) {
        case Phase::Center:
            return m_lastCopy ? "Tastează x4 sau /4 pentru copii radiale · sau alege centrul rotației"
                              : "Alege centrul rotației (pe o față: planul ei) · săgeți: axa X/Y/Z";
        case Phase::Reference:
            return "Alege direcția de referință (unghiul 0)";
        case Phase::Angle:
            return "Alege unghiul sau tastează-l în grade · Option: copiază · Esc anulează";
        }
        return {};
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Unghi", m_phase == Phase::Angle ? formatAngle(m_angle) : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        if (m_phase == Phase::Center) {
            const auto array = parseArray(text);
            return array && applyArray(ctx, m_lastCopy, *array);
        }
        if (m_phase != Phase::Angle) {
            return false;
        }
        const auto degrees = parseNumber(text);
        if (!degrees || std::abs(*degrees) < 1e-9) {
            return false;
        }
        m_angle = (m_angle < 0.0 ? -*degrees : *degrees) * kDegree;
        commit(ctx);
        return true;
    }

private:
    enum class Phase { Center, Reference, Angle };

    static Vec3d anyPerpendicular(const Vec3d& n)
    {
        const Vec3d a = std::abs(n.z) < 0.9 ? Vec3d { 0, 0, 1 } : Vec3d { 1, 0, 0 };
        return glm::normalize(glm::cross(a, n));
    }

    static inference::Tint axisTint(const Vec3d& n)
    {
        for (auto axis : { inference::Axis::X, inference::Axis::Y, inference::Axis::Z }) {
            if (tol::parallel(n, inference::axisDirection(axis), 1e-6)) {
                return inference::axisTint(axis);
            }
        }
        return inference::Tint::Neutral;
    }

    [[nodiscard]] Transform rotation() const { return Transform::rotation(m_center, m_normal, m_angle); }

    // Punctul de sub cursor, adus în planul de rotație.
    std::optional<Vec3d> onPlane(const ToolContext& ctx, Vec2d screen) const
    {
        const math::Plane plane = math::Plane::fromPointNormal(m_center, m_normal);
        if (m_inference.isPoint() || m_inference.kind == inference::Kind::OnEdge) {
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

    void track(ToolContext& ctx, const PointerEvent& e)
    {
        m_copy = e.modifiers.alt;
        const std::optional<Vec3d> from = m_phase == Phase::Center ? std::nullopt : std::optional(m_center);
        m_inference = m_engine.withGuides(ctx.guides).withGridSnap(ctx.gridStep).infer(ctx.mesh, ctx.camera, e.screen, from);
        switch (m_phase) {
        case Phase::Center:
            m_hover = targetHover(ctx, e.screen);
            if (!m_axisLock) {
                const geom::FaceId face = m_inference.pick.face;
                m_hoverNormal = face && ctx.mesh.faces().contains(face) ? ctx.mesh.face(face).plane.normal
                                                                        : Vec3d { 0, 0, 1 };
            }
            if (m_inference.valid()) {
                m_protractorRadius = worldPerPixelAt(ctx.camera, m_inference.point) * 60.0;
            }
            return;
        case Phase::Reference:
            if (const auto p = onPlane(ctx, e.screen)) {
                m_reference = *p;
            }
            return;
        case Phase::Angle:
            if (const auto p = onPlane(ctx, e.screen)) {
                const Vec3d r = m_reference - m_center;
                const Vec3d c = *p - m_center;
                if (glm::length(c) > tol::kLength) {
                    double angle = std::atan2(glm::dot(glm::cross(r, c), m_normal), glm::dot(r, c));
                    const double snapped = std::round(angle / kAngleSnapStep) * kAngleSnapStep;
                    if (!m_inference.isPoint() && std::abs(angle - snapped) <= kAngleSnapTolerance) {
                        angle = snapped;
                    }
                    m_angle = angle;
                }
            }
            return;
        }
    }

    void commit(ToolContext& ctx)
    {
        if (std::abs(m_angle) > 1e-9) {
            const Transform t = rotation();
            if (m_copy) {
                const geom::Selection source = m_target.selection;
                ctx.selection = copyTarget(ctx, source, { t });
                const Vec3d center = m_center;
                const Vec3d normal = m_normal;
                const double angle = m_angle;
                m_lastCopy = LastCopy { source,
                    [center, normal, angle](double k) { return Transform::rotation(center, normal, angle * k); } };
            } else {
                transformTarget(ctx, "Rotate", m_target, t);
                m_lastCopy.reset();
            }
            ctx.selection.prune(ctx.mesh);
        }
        m_phase = Phase::Center;
        m_engine.resetLock();
    }

    Phase m_phase = Phase::Center;
    Target m_target;
    Vec3d m_center { 0.0 };
    Vec3d m_normal { 0, 0, 1 };
    Vec3d m_hoverNormal { 0, 0, 1 };
    std::optional<inference::Axis> m_axisLock;
    Vec3d m_reference { 0.0 };
    double m_angle = 0.0;
    double m_protractorRadius = 300.0;
    std::vector<PreviewEdge> m_preview;
    bool m_copy = false;
    std::optional<LastCopy> m_lastCopy;
    inference::Engine m_engine;
    inference::Inference m_inference;
    view::PickResult m_hover;
};

// --- Scale ----------------------------------------------------------------------------------

// Scalare uniformă: punctul fix, un punct de referință, apoi factorul (raportul distanțelor față de punctul fix).
// Option: punctul fix devine centrul entităților scalate.
// Scale (S), ca în SketchUp: în jurul selecției apare cutia ei cu mânere — colțurile scalează uniform, mijloacele
// muchiilor pe două axe, centrele fețelor pe o singură axă. Click pe un mâner, mișcă, click din nou (sau tastează
// factorul: „2”, ori „2,1,1.5” pe axele X, Y, Z). Punctul fix e mânerul opus; cu Option, centrul cutiei.
// Fără selecție, primul click alege entitatea de sub cursor. Scările negative oglindesc.
class ScaleTool final : public Tool {
public:
    std::string_view id() const override { return "scale"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (m_dragging) {
            commit(ctx);
            return;
        }
        if (m_hoverHandle >= 0) {
            startDrag(ctx, e);
            return;
        }
        // Fără mâner sub cursor: alege entitatea (dacă nu e nimic selectat), ca să-i apară mânerele.
        if (ctx.selection.empty()) {
            const Target t = targetAt(ctx, e.screen);
            if (!t.selection.empty()) {
                ctx.selection = t.selection;
                refreshBox(ctx);
            }
        }
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_dragging = false;
        m_factors = Vec3d { 1.0 };
    }

    Overlay overlay() const override
    {
        Overlay o;
        if (!m_hasBox) {
            return o;
        }
        const Transform t = m_dragging ? scaling() : Transform {};
        // Cutia (scalată în timpul tragerii) și mânerele ei.
        std::array<Vec3d, 8> c {};
        for (std::size_t k = 0; k < 8; ++k) {
            c[k] = t.applyToPoint({ (k & 1) ? m_hi.x : m_lo.x, (k & 2) ? m_hi.y : m_lo.y, (k & 4) ? m_hi.z : m_lo.z });
        }
        constexpr std::array<std::pair<int, int>, 12> kEdges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 },
            { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
        for (const auto& [a, b] : kEdges) {
            o.lines.push_back({ c[std::size_t(a)], c[std::size_t(b)], true, inference::Tint::Neutral, true });
        }
        for (int i = 0; i < 27; ++i) {
            if (i != 13) {
                o.points.push_back(t.applyToPoint(handlePoint(i)));
            }
        }
        if (m_dragging) {
            appendPreview(o, m_preview, t);
            o.points.push_back(pivot());
            const Vec3d handle = t.applyToPoint(handlePoint(m_handle));
            o.dimensions.push_back({ pivot(), handle, describe() });
        }
        const int marked = m_dragging ? m_handle : m_hoverHandle;
        if (marked >= 0) {
            InferenceMarker marker;
            marker.point = t.applyToPoint(handlePoint(marked));
            marker.kind = inference::Kind::Endpoint;
            marker.label = handleName(marked);
            o.marker = marker;
        }
        return o;
    }

    view::PickResult hover() const override { return m_hasBox ? view::PickResult {} : m_hover; }

    std::string status() const override
    {
        if (m_dragging) {
            return "Mișcă pentru mărimea nouă, click ca să confirmi, sau tastează factorul (2 · 2,1,1.5) · "
                   "Option: din centru · Esc anulează";
        }
        if (m_hasBox) {
            return "Alege un mâner: colțurile scalează uniform, muchiile pe două axe, fețele pe una";
        }
        return "Selectează ce vrei să scalezi (sau click pe o entitate)";
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Scară", m_dragging ? describe() : "" };
    }

    bool applyMeasurement(ToolContext& ctx, std::string_view text) override
    {
        if (!m_dragging) {
            return false;
        }
        // „2” = factorul pe axele mânerului; „2,1,1.5” (sau cu ;) = factorii pe X, Y, Z.
        std::vector<double> values;
        std::string token;
        const std::string all(text);
        for (std::size_t i = 0; i <= all.size(); ++i) {
            if (i == all.size() || all[i] == ',' || all[i] == ';') {
                const auto v = parseNumber(token);
                if (!v || std::abs(*v) < 1e-6) {
                    return false;
                }
                values.push_back(*v);
                token.clear();
            } else {
                token += all[i];
            }
        }
        if (values.size() == 1) {
            m_factors = factorsFor(values[0]);
        } else if (values.size() == 3) {
            m_factors = { values[0], values[1], values[2] };
        } else {
            return false;
        }
        commit(ctx);
        return true;
    }

private:
    // Mânerul i: coordonatele (ix, iy, iz) ∈ {0, 1, 2}³ (min, mijloc, max); 13 = centrul (nu e mâner).
    static std::array<int, 3> handleIndex(int i) { return { i % 3, (i / 3) % 3, i / 9 }; }

    [[nodiscard]] Vec3d handlePoint(int i) const
    {
        const auto idx = handleIndex(i);
        Vec3d p;
        for (int a = 0; a < 3; ++a) {
            p[a] = idx[std::size_t(a)] == 0 ? m_lo[a] : idx[std::size_t(a)] == 2 ? m_hi[a] : (m_lo[a] + m_hi[a]) * 0.5;
        }
        return p;
    }

    [[nodiscard]] static std::string handleName(int i)
    {
        const auto idx = handleIndex(i);
        const int active = (idx[0] != 1) + (idx[1] != 1) + (idx[2] != 1);
        return active == 3 ? "Colț: uniform" : active == 2 ? "Muchie: două axe" : "Față: o axă";
    }

    // Axele pe care lucrează mânerul tras.
    [[nodiscard]] std::array<bool, 3> activeAxes() const
    {
        const auto idx = handleIndex(m_handle);
        return { idx[0] != 1, idx[1] != 1, idx[2] != 1 };
    }

    [[nodiscard]] Vec3d factorsFor(double f) const
    {
        const auto axes = activeAxes();
        return { axes[0] ? f : 1.0, axes[1] ? f : 1.0, axes[2] ? f : 1.0 };
    }

    [[nodiscard]] Vec3d pivot() const
    {
        if (m_fromCenter) {
            return (m_lo + m_hi) * 0.5;
        }
        const auto idx = handleIndex(m_handle);
        return handlePoint(int(2 - idx[0]) + 3 * int(2 - idx[1]) + 9 * int(2 - idx[2]));
    }

    [[nodiscard]] Transform scaling() const { return Transform::scaling(pivot(), m_factors); }

    [[nodiscard]] std::string describe() const
    {
        const auto axes = activeAxes();
        if (axes[0] + axes[1] + axes[2] > 1 && m_factors.x == m_factors.y && m_factors.y == m_factors.z) {
            return formatFactor(m_factors.x);
        }
        std::string out;
        for (int a = 0; a < 3; ++a) {
            out += (a > 0 ? ", " : "") + formatFactor(m_factors[a]);
        }
        return out;
    }

    void refreshBox(const ToolContext& ctx)
    {
        m_hasBox = false;
        if (ctx.selection.empty()) {
            return;
        }
        Target t;
        t.selection = ctx.selection;
        t.vertices = geom::verticesOf(ctx.mesh, t.selection);
        Vec3d lo { 1e300 };
        Vec3d hi { -1e300 };
        for (geom::VertexId v : t.vertices) {
            lo = glm::min(lo, ctx.mesh.position(v));
            hi = glm::max(hi, ctx.mesh.position(v));
        }
        for (const PreviewEdge& e : previewEdges(ctx, Target { t.selection, {} })) {
            lo = glm::min(lo, glm::min(e.a, e.b));
            hi = glm::max(hi, glm::max(e.a, e.b));
        }
        if (lo.x > hi.x) {
            return;
        }
        // O cutie turtită (o față plană) primește o grosime mică, ca mânerele să nu se suprapună.
        for (int a = 0; a < 3; ++a) {
            if (hi[a] - lo[a] < tol::kLength) {
                lo[a] -= 0.5;
                hi[a] += 0.5;
            }
        }
        m_lo = lo;
        m_hi = hi;
        m_hasBox = true;
    }

    void startDrag(ToolContext& ctx, const PointerEvent& e)
    {
        m_target = targetAt(ctx, e.screen);
        if (m_target.empty()) {
            return;
        }
        m_handle = m_hoverHandle;
        m_preview = previewEdges(ctx, m_target);
        m_factors = Vec3d { 1.0 };
        m_dragging = true;
    }

    void track(ToolContext& ctx, const PointerEvent& e)
    {
        m_fromCenter = e.modifiers.alt;
        if (!m_dragging) {
            refreshBox(ctx);
            m_hover = targetHover(ctx, e.screen);
            m_hoverHandle = -1;
            if (!m_hasBox) {
                return;
            }
            double best = 12.0; // px
            for (int i = 0; i < 27; ++i) {
                if (i == 13) {
                    continue;
                }
                if (const auto s = ctx.camera.project(handlePoint(i))) {
                    const double d = glm::length(*s - e.screen);
                    if (d < best) {
                        best = d;
                        m_hoverHandle = i;
                    }
                }
            }
            return;
        }
        // Factorul: proiecția cursorului pe dreapta pivot → mâner (pe axele mânerului).
        const Vec3d from = pivot();
        Vec3d d = handlePoint(m_handle) - from;
        const auto axes = activeAxes();
        for (int a = 0; a < 3; ++a) {
            d[a] = axes[std::size_t(a)] ? d[a] : 0.0;
        }
        const double len2 = glm::dot(d, d);
        if (len2 < 1e-12) {
            return;
        }
        if (const auto p = inference::pointOnLineUnderCursor(ctx.camera, e.screen, from, glm::normalize(d))) {
            double f = glm::dot(*p - from, d) / len2;
            if (std::abs(f) < 1e-3) {
                f = f < 0.0 ? -1e-3 : 1e-3;
            }
            // Pași de 0,05 când cursorul e aproape de un multiplu (prinderi plăcute, ca la unghiuri).
            const double snapped = std::round(f * 20.0) / 20.0;
            if (std::abs(f - snapped) < 0.01) {
                f = snapped;
            }
            m_factors = factorsFor(f);
        }
    }

    void commit(ToolContext& ctx)
    {
        if (glm::length(m_factors - Vec3d { 1.0 }) > 1e-9) {
            transformTarget(ctx, "Scale", m_target, scaling());
            ctx.selection.prune(ctx.mesh);
        }
        m_dragging = false;
        m_factors = Vec3d { 1.0 };
        refreshBox(ctx);
    }

    bool m_hasBox = false;
    Vec3d m_lo { 0.0 };
    Vec3d m_hi { 0.0 };
    int m_hoverHandle = -1;
    int m_handle = -1;
    bool m_dragging = false;
    bool m_fromCenter = false;
    Vec3d m_factors { 1.0 };
    Target m_target;
    std::vector<PreviewEdge> m_preview;
    view::PickResult m_hover;
};

} // namespace

std::unique_ptr<Tool> makeMoveTool()
{
    return std::make_unique<MoveTool>();
}

std::unique_ptr<Tool> makeRotateTool()
{
    return std::make_unique<RotateTool>();
}

std::unique_ptr<Tool> makeScaleTool()
{
    return std::make_unique<ScaleTool>();
}

} // namespace aion::tools
