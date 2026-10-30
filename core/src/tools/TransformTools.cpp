#include "ToolHelpers.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/math/Intersect.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Operations.h"
#include "aion/core/units/Length.h"

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
// Cât de aproape (în pixeli) trebuie să fie cursorul de un punct de prindere al obiectului.
constexpr double kGripRadiusPx = 14.0;

// Ce transformă unealta: entitățile (pentru copiere) și vârfurile lor (pentru mutare).
struct Target {
    geom::Selection selection;
    std::set<geom::VertexId> vertices;
    std::optional<model::SceneObjectRef> object; // o cameră sau o lumină (selectată, fără altă selecție)

    [[nodiscard]] bool empty() const { return vertices.empty() && selection.instances.empty() && !object; }
};

Target targetAt(const ToolContext& ctx, Vec2d screen)
{
    Target t;
    if (ctx.selection.empty() && ctx.object && ctx.model != nullptr) {
        t.object = ctx.object;
        return t;
    }
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
        std::erase_if(
            t.selection.instances, [&](model::InstanceId i) { return ctx.model->instance(i).locked; });
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
    // O cameră / lumină: o cruce în poziția ei și, la cameră, linia spre țintă.
    if (target.object && ctx.model != nullptr) {
        const std::vector<Vec3d> points = model::sceneObjectPoints(*ctx.model, *target.object);
        if (!points.empty()) {
            const double s = std::max(ctx.camera.worldPerPixelAtTarget(), 1e-6) * 18.0;
            for (const Vec3d& axis : { Vec3d { 1, 0, 0 }, Vec3d { 0, 1, 0 }, Vec3d { 0, 0, 1 } }) {
                out.push_back({ points[0] - axis * s, points[0] + axis * s, true, true });
            }
            if (points.size() > 1) {
                out.push_back({ points[0], points[1], true, true });
            }
        }
    }
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
            constexpr std::array<std::pair<int, int>, 12> kEdges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },
                { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
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

// Punctele de prindere ale obiectelor (Move): colțurile și mijloacele muchiilor cutiei fiecăruia, orientate
// după obiect (o cutie rotită are punctele pe muchiile ei, nu pe ale cutiei aliniate cu axele).
std::vector<Grip> instanceGrips(const ToolContext& ctx, const std::set<model::InstanceId>& ids)
{
    std::vector<Grip> out;
    if (ctx.model == nullptr) {
        return out;
    }
    constexpr std::array<std::pair<int, int>, 12> kEdges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 }, { 0, 2 },
        { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
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
            c[k] = inst.transform.applyToPoint(
                { (k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z });
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
                const Vec3d toEye = ctx.camera.projection() == view::Projection::Orthographic
                    ? -ctx.camera.forward()
                    : ctx.camera.eye() - p;
                facing[std::size_t(a * 2 + side)] = glm::dot(outward, toEye) > 0.0;
            }
        }
        const auto visible = [&](std::size_t k) { // colțul k (biții = partea pe X, Y, Z)
            return flat || facing[(k & 1) ? 1 : 0] || facing[(k & 2) ? 3 : 2] || facing[(k & 4) ? 5 : 4];
        };
        const std::size_t first = out.size();
        const auto add = [&](const Vec3d& p, inference::Kind kind, bool hidden) {
            // O cutie turtită (o placă, o formă plană) are colțuri și mijloace care coincid: fiecare punct o
            // dată.
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
int nearestGrip(
    const ToolContext& ctx, const std::vector<Grip>& grips, Vec2d screen, double radiusPx, bool hidden);

// Cel mai apropiat punct vizibil; doar dacă nu e niciunul, unul din spatele obiectului.
int nearestGrip(const ToolContext& ctx, const std::vector<Grip>& grips, Vec2d screen, double radiusPx)
{
    const int visible = nearestGrip(ctx, grips, screen, radiusPx, false);
    return visible >= 0 ? visible : nearestGrip(ctx, grips, screen, radiusPx, true);
}

int nearestGrip(
    const ToolContext& ctx, const std::vector<Grip>& grips, Vec2d screen, double radiusPx, bool hidden)
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
            if (d < bestDistance - 0.5
                || (d < bestDistance + 0.5 && best >= 0 && grips[i].kind == inference::Kind::Endpoint
                    && grips[std::size_t(best)].kind != inference::Kind::Endpoint)) {
                bestDistance = std::min(d, bestDistance);
                best = static_cast<int>(i);
            }
        }
    }
    return best;
}

// Obiectul ale cărui puncte de prindere contează la cursor: cel de sub el sau, dacă nu e niciunul, cel cu un
// punct cel mai aproape de cursor (un colț văzut din afară). Fără cele din `exclude`; gol = niciunul.
model::InstanceId gripObjectNear(
    const ToolContext& ctx, Vec2d screen, const std::set<model::InstanceId>& exclude)
{
    const view::PickResult hit = pickWithInstances(ctx, screen);
    if (hit.kind == view::PickResult::Kind::Instance && !exclude.contains(hit.instance)) {
        return hit.instance;
    }
    model::InstanceId nearest;
    if (ctx.model == nullptr) {
        return nearest;
    }
    double best = kGripRadiusPx;
    for (std::size_t i = 0; i < ctx.model->instances().size(); ++i) {
        const model::InstanceId id = ctx.model->instances().keys()[i];
        const model::Instance& inst = ctx.model->instances().values()[i];
        if (inst.parent != ctx.context || inst.hidden || exclude.contains(id)) {
            continue;
        }
        const std::vector<Grip> grips = instanceGrips(ctx, { id });
        const int k = nearestGrip(ctx, grips, screen, best);
        if (k >= 0) {
            if (const auto sp = ctx.camera.project(grips[std::size_t(k)].point)) {
                best = glm::length(*sp - screen);
            }
            nearest = id;
        }
    }
    return nearest;
}

void appendPreview(Overlay& o, const std::vector<PreviewEdge>& edges, const Transform& t)
{
    for (const PreviewEdge& e : edges) {
        o.lines.push_back(
            { e.movesA ? t.applyToPoint(e.a) : e.a, e.movesB ? t.applyToPoint(e.b) : e.b, false });
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
        if (target.object) {
            model::transformSceneObject(model, *target.object, t);
        }
    });
}

// Punctele de prindere ale camerei / luminii selectate (poziția, ținta).
std::vector<Grip> objectGrips(const ToolContext& ctx)
{
    std::vector<Grip> out;
    if (ctx.model != nullptr && ctx.selection.empty() && ctx.object) {
        for (const Vec3d& p : model::sceneObjectPoints(*ctx.model, *ctx.object)) {
            out.push_back({ p, inference::Kind::Endpoint, false, false });
        }
    }
    return out;
}

// Copiile țintei, câte una pentru fiecare transformare (și seriile ×N): copii unice, în adâncime (doar ⌥D și
// Array leagă copiile).
geom::Selection copyTarget(
    ToolContext& ctx, const geom::Selection& source, const std::vector<Transform>& transforms)
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
        std::vector<model::InstanceId> ids;
        for (model::InstanceId i : source.instances) {
            if (model.instances().contains(i)) {
                ids.push_back(i);
            }
        }
        for (const Transform& t : transforms) {
            const model::CopyResult copy = model::copyDeep(model, ctx.context, model, ids, t);
            model::copyAnimations(model, model, copy, t, model::CopyKind::Copy);
            copies.instances.insert(copy.roots.begin(), copy.roots.end());
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
            // Un punct de prindere al obiectului de sub cursor (fără selecție): obiectul lui e ținta, chiar
            // dacă punctul e puțin în afara conturului.
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

    std::set<model::InstanceId> snapExclude() const override
    {
        return m_base ? m_target.selection.instances : std::set<model::InstanceId> {};
    }

    bool ownsArrowKeys() const override { return m_base.has_value(); }

    std::string status() const override
    {
        if (m_base) {
            return "Alege punctul de destinație sau tastează distanța · Option: copiază · săgeți/Shift: "
                   "blochează · Esc";
        }
        return m_lastCopy
            ? "Tastează x3 sau /3 pentru mai multe copii · sau alege ce muți"
            : "Alege punctul de pornire: un colț sau mijlocul unei muchii al obiectului, sau orice punct";
    }

    std::optional<Measurement> measurement() const override
    {
        return Measurement { "Distanță",
            m_base ? units::formatLength(glm::length(m_current - *m_base)) : "" };
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
        m_inference = inferAt(m_engine, ctx, e.screen, m_base);
        if (m_base && m_inference.valid()) {
            m_current = m_inference.point;
        }
        m_hover = m_base ? view::PickResult {} : targetHover(ctx, e.screen);
        updateGrips(ctx, e.screen);
    }

    // Punctele de prindere: înainte de pornire, ale obiectelor selectate (sau al celui de sub cursor); în
    // timpul mutării, ale obiectului de sub cursor (destinația), fără cele mutate. Cel apropiat de cursor e
    // „fierbinte”.
    void updateGrips(ToolContext& ctx, Vec2d screen)
    {
        std::set<model::InstanceId> ids;
        m_gripOwner = {};
        if (!m_base && !ctx.selection.empty()) {
            ids = ctx.selection.instances;
        } else {
            m_gripOwner = gripObjectNear(
                ctx, screen, m_base ? m_target.selection.instances : std::set<model::InstanceId> {});
            if (m_gripOwner) {
                ids.insert(m_gripOwner);
            }
        }
        m_grips = instanceGrips(ctx, ids);
        if (!m_base) {
            const std::vector<Grip> own = objectGrips(ctx);
            m_grips.insert(m_grips.begin(), own.begin(), own.end());
        }
        m_hotGrip = nearestGrip(ctx, m_grips, screen, kGripRadiusPx);
        // Un punct real al unui obiect (vârf, mijloc de muchie, centru) bate punctul cutiei când nu e mai
        // departe de cursor decât acesta cu mai mult de 0,5 px; punctele cutiei rămân ținte doar unde nu e
        // niciun punct real.
        if (m_hotGrip >= 0 && m_inference.origin == inference::Origin::Object
            && objectPoint(m_inference.kind)) {
            const auto gs = ctx.camera.project(m_grips[std::size_t(m_hotGrip)].point);
            const auto is = ctx.camera.project(m_inference.point);
            if (gs && is && glm::length(*is - screen) <= glm::length(*gs - screen) + 0.5) {
                // În același loc (colțul unei cutii), punctul rămâne aprins; marcajul e al inferenței.
                if (tol::samePoint(m_grips[std::size_t(m_hotGrip)].point, m_inference.point)) {
                    m_grips[std::size_t(m_hotGrip)].hot = true;
                }
                m_hotGrip = -1;
            }
        }
        if (m_hotGrip >= 0) {
            m_grips[std::size_t(m_hotGrip)].hot = true;
            if (m_base && !m_shiftLock) {
                m_current = m_grips[std::size_t(m_hotGrip)].point;
            }
        }
    }

    static bool objectPoint(inference::Kind k)
    {
        return k == inference::Kind::Endpoint || k == inference::Kind::Midpoint
            || k == inference::Kind::Center;
    }

    void commit(ToolContext& ctx)
    {
        const Vec3d delta = m_current - *m_base;
        if (glm::length(delta) > tol::kLength) {
            const Transform t = Transform::translation(delta);
            if (m_copy && !m_target.object) { // camerele și luminile nu se copiază cu Option
                const geom::Selection source = m_target.selection;
                ctx.selection = copyTarget(ctx, source, { t });
                m_lastCopy
                    = LastCopy { source, [delta](double k) { return Transform::translation(delta * k); } };
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

// --- Place (lipirea) ------------------------------------------------------------------------

// Copia lipită (⌘V) urmează cursorul cu colțul ei de prindere, prinsă ca Move: de colțurile și mijloacele
// muchiilor obiectului de sub cursor, de inferență și de grid. Click o așază, Esc renunță; Document scrie
// copia în model.
class PlaceTool final : public Tool {
public:
    std::string_view id() const override { return "place"; }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override { track(ctx, e); }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        track(ctx, e);
        if (m_current && ctx.requests != nullptr) {
            ctx.requests->placeAt = *m_current;
        }
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Escape) {
            if (ctx.requests != nullptr) {
                ctx.requests->placeCancel = true;
            }
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override
    {
        m_current.reset();
        m_grips.clear();
        m_hotGrip = -1;
    }

    // Copiile nu sunt încă în model (doar conturul lor, desenat de unealtă), deci nu sunt surse; originalele
    // rămân ținte (lipirea colț pe colț pe original).
    std::set<model::InstanceId> snapExclude() const override { return {}; }

    Overlay overlay() const override
    {
        Overlay o;
        if (m_current) {
            const Vec3d d = *m_current - m_source.base;
            for (const auto& [a, b] : m_source.edges) {
                o.lines.push_back({ a + d, b + d, false });
            }
            o.points.push_back(*m_current);
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

    std::string status() const override
    {
        return "Alege unde pui copia: un colț sau mijlocul unei muchii al altui obiect, grid-ul sau orice "
               "punct · Esc: "
               "renunță";
    }

private:
    void track(ToolContext& ctx, const PointerEvent& e)
    {
        m_source = ctx.place.value_or(PlaceSource {});
        m_inference = inferAt(m_engine, ctx, e.screen, std::nullopt);
        if (m_inference.valid()) {
            m_current = m_inference.point;
        }
        const model::InstanceId owner = gripObjectNear(ctx, e.screen, {});
        m_grips = owner ? instanceGrips(ctx, { owner }) : std::vector<Grip> {};
        m_hotGrip = nearestGrip(ctx, m_grips, e.screen, kGripRadiusPx);
        if (m_hotGrip >= 0) {
            m_grips[std::size_t(m_hotGrip)].hot = true;
            m_current = m_grips[std::size_t(m_hotGrip)].point;
        }
    }

    PlaceSource m_source;
    std::optional<Vec3d> m_current;
    std::vector<Grip> m_grips;
    int m_hotGrip = -1;
    inference::Engine m_engine;
    inference::Inference m_inference;
};

// --- „Adaugă separator” (mobilier, pasul 6) ------------------------------------------------------

class DividerTool final : public Tool {
public:
    std::string_view id() const override { return "divider"; }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        m_inference = inferAt(m_engine, ctx, e.screen, std::nullopt);
    }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        m_inference = inferAt(m_engine, ctx, e.screen, std::nullopt);
        if (m_inference.valid() && ctx.requests != nullptr) {
            ctx.requests->dividerAt = m_inference.point;
            ctx.requests->dividerAtClick = e.modifiers.alt;
        }
    }

    void pointerUp(ToolContext& /*ctx*/, const PointerEvent& /*e*/) override { }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Escape) {
            if (ctx.requests != nullptr) {
                ctx.requests->dividerCancel = true;
            }
            return true;
        }
        return Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override { m_inference = {}; }

    Overlay overlay() const override
    {
        Overlay o;
        appendInference(o, m_inference);
        return o;
    }

    std::string status() const override
    {
        return "Clic în spațiul corpului: separatorul se pune la mijlocul lui · ⌥: la locul clicului · Esc: "
               "renunță";
    }

private:
    inference::Engine m_engine;
    inference::Inference m_inference;
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
            if (m_target.empty()) {
                return;
            }
            m_normal = m_axisLock ? inference::axisDirection(*m_axisLock) : m_hoverNormal;
            m_preview = previewEdges(ctx, m_target);
            if (m_fromPivot) {
                // Centrul e pivotul selecției; click-ul a ales deja referința (unghiul 0).
                m_center = *ctx.pivot;
                m_angle = 0.0;
                m_phase
                    = glm::length(m_reference - m_center) > tol::kLength ? Phase::Angle : Phase::Reference;
                m_grips.clear();
                m_hotGrip = -1;
                return;
            }
            if (m_hotGrip < 0 && !m_inference.valid()) {
                return;
            }
            m_center = m_hotGrip >= 0 ? m_grips[std::size_t(m_hotGrip)].point : m_inference.point;
            m_reference = m_center;
            m_phase = Phase::Reference;
            m_grips.clear();
            m_hotGrip = -1;
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
        m_grips.clear();
        m_hotGrip = -1;
    }

    std::set<model::InstanceId> snapExclude() const override
    {
        return m_phase != Phase::Center ? m_target.selection.instances : std::set<model::InstanceId> {};
    }

    Overlay overlay() const override
    {
        Overlay o;
        o.grips = m_grips;
        if (m_hotGrip >= 0) {
            const Grip& g = m_grips[std::size_t(m_hotGrip)];
            o.marker = InferenceMarker { g.point, g.kind, inference::Tint::Neutral, gripLabel(g), false };
        }
        const bool choosingCenter = m_phase == Phase::Center && !m_fromPivot;
        const Vec3d center = !choosingCenter ? m_center
            : m_hotGrip >= 0                 ? m_grips[std::size_t(m_hotGrip)].point
                                             : m_inference.point;
        const Vec3d normal = m_phase == Phase::Center ? m_hoverNormal : m_normal;
        if (choosingCenter && !m_inference.valid() && m_hotGrip < 0) {
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
        if (!choosingCenter) {
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
        if (m_phase != Phase::Angle && m_hotGrip < 0) {
            appendInference(o, m_inference);
        }
        return o;
    }

    view::PickResult hover() const override
    {
        return m_phase == Phase::Center ? m_hover : view::PickResult {};
    }

    // Înainte de centru săgețile aleg axa; după, rotația e în curs.
    bool ownsArrowKeys() const override { return true; }

    std::string status() const override
    {
        switch (m_phase) {
        case Phase::Center:
            if (m_lastCopy) {
                return "Tastează x4 sau /4 pentru copii radiale · sau alege centrul rotației";
            }
            return m_fromPivot
                ? "Centrul e pivotul: alege direcția de referință (unghiul 0) · săgeți: axa X/Y/Z · "
                  "Option: alt centru"
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

    [[nodiscard]] std::string gripLabel(const Grip& g) const
    {
        if (m_pivot && glm::length(g.point - *m_pivot) < tol::kLength) {
            return "Pivot";
        }
        return g.kind == inference::Kind::Endpoint ? "Colț" : "Mijlocul muchiei";
    }

    // Punctele de prindere pentru centru (sau, când centrul e pivotul, pentru referință): colțurile și
    // mijloacele obiectelor selectate (sau al celui de sub cursor) și pivotul selecției.
    void updateGrips(const ToolContext& ctx, Vec2d screen)
    {
        std::set<model::InstanceId> ids = ctx.selection.instances;
        if (ids.empty()) {
            const view::PickResult hit = pickWithInstances(ctx, screen);
            if (hit.kind == view::PickResult::Kind::Instance) {
                ids.insert(hit.instance);
            }
        }
        m_grips = instanceGrips(ctx, ids);
        const std::vector<Grip> own = objectGrips(ctx);
        m_grips.insert(m_grips.end(), own.begin(), own.end());
        if (m_pivot && !m_fromPivot) {
            const auto same = [&](const Grip& g) { return glm::length(g.point - *m_pivot) < tol::kLength; };
            if (std::none_of(m_grips.begin(), m_grips.end(), same)) {
                m_grips.push_back({ *m_pivot, inference::Kind::Endpoint, false, false });
            }
        }
        m_hotGrip = nearestGrip(ctx, m_grips, screen, kGripRadiusPx);
        if (m_hotGrip >= 0) {
            m_grips[std::size_t(m_hotGrip)].hot = true;
        }
    }

    // Punctul de sub cursor, adus în planul de rotație.
    std::optional<Vec3d> onPlane(const ToolContext& ctx, Vec2d screen) const
    {
        const math::Plane plane = math::Plane::fromPointNormal(m_center, m_normal);
        // Un punct prins (capăt, mijloc, muchie, ghidaj) sau o direcție inferată din centru (pe axă, paralel,
        // perpendicular): punctul ei exact — altfel referința și linia de final ies puțin înclinate față de
        // axa arătată.
        if (m_inference.isPoint() || m_inference.kind == inference::Kind::OnEdge
            || m_inference.kind == inference::Kind::GuidePoint || m_inference.direction.has_value()) {
            const Vec3d p = plane.project(m_inference.point);
            if (glm::length(p - m_center) > tol::kLength) { // o axă perpendiculară pe plan cade în centru
                return p;
            }
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
        if (m_phase == Phase::Center) {
            // Cu o selecție, centrul e pivotul ei (marcajul din viewport); Option = alege alt centru.
            m_pivot = ctx.selection.empty() && !ctx.object ? std::nullopt : ctx.pivot;
            m_fromPivot = m_pivot.has_value() && !e.modifiers.alt;
            if (m_fromPivot) {
                m_center = *m_pivot;
            }
        }
        const std::optional<Vec3d> from
            = m_phase == Phase::Center && !m_fromPivot ? std::nullopt : std::optional(m_center);
        m_inference = inferAt(m_engine, ctx, e.screen, from);
        switch (m_phase) {
        case Phase::Center:
            m_hover = targetHover(ctx, e.screen);
            if (!m_axisLock) {
                m_hoverNormal = inferenceNormal(ctx, m_inference, Vec3d { 0, 0, 1 });
            }
            updateGrips(ctx, e.screen);
            if (m_fromPivot) {
                m_normal = m_hoverNormal;
                const math::Plane plane = math::Plane::fromPointNormal(m_center, m_normal);
                if (m_hotGrip >= 0) {
                    m_reference = plane.project(m_grips[std::size_t(m_hotGrip)].point);
                } else if (const auto p = onPlane(ctx, e.screen)) {
                    m_reference = *p;
                }
                m_protractorRadius = worldPerPixelAt(ctx.camera, m_center) * 60.0;
            } else if (m_hotGrip >= 0) {
                m_protractorRadius
                    = worldPerPixelAt(ctx.camera, m_grips[std::size_t(m_hotGrip)].point) * 60.0;
            } else if (m_inference.valid()) {
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
            if (m_copy && !m_target.object) {
                const geom::Selection source = m_target.selection;
                ctx.selection = copyTarget(ctx, source, { t });
                const Vec3d center = m_center;
                const Vec3d normal = m_normal;
                const double angle = m_angle;
                m_lastCopy = LastCopy { source, [center, normal, angle](double k) {
                                           return Transform::rotation(center, normal, angle * k);
                                       } };
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
    std::optional<Vec3d> m_pivot; // pivotul selecției
    bool m_fromPivot = false; // centrul e pivotul (fără Option)
    std::vector<Grip> m_grips;
    int m_hotGrip = -1;
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

// Scalare uniformă: punctul fix, un punct de referință, apoi factorul (raportul distanțelor față de punctul
// fix). Option: punctul fix devine pivotul selecției (implicit centrul entităților scalate). Scale (S), ca în
// SketchUp: în jurul selecției apare cutia ei cu mânere — colțurile scalează uniform, mijloacele muchiilor pe
// două axe, centrele fețelor pe o singură axă. Click pe un mâner, mișcă, click din nou (sau tastează
// factorul: „2”, ori „2,1,1.5” pe axele X, Y, Z). Punctul fix e mânerul opus; cu Option, pivotul selecției
// (implicit centrul cutiei). Fără selecție, primul click alege entitatea de sub cursor. Scările negative
// oglindesc.
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
        if (ctx.selection.empty() && !ctx.object) {
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

    std::set<model::InstanceId> snapExclude() const override
    {
        return m_dragging ? m_target.selection.instances : std::set<model::InstanceId> {};
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
            c[k] = t.applyToPoint(
                { (k & 1) ? m_hi.x : m_lo.x, (k & 2) ? m_hi.y : m_lo.y, (k & 4) ? m_hi.z : m_lo.z });
        }
        constexpr std::array<std::pair<int, int>, 12> kEdges { { { 0, 1 }, { 2, 3 }, { 4, 5 }, { 6, 7 },
            { 0, 2 }, { 1, 3 }, { 4, 6 }, { 5, 7 }, { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 } } };
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

    bool ownsArrowKeys() const override { return m_dragging; }

    std::string status() const override
    {
        if (m_dragging) {
            return "Mișcă pentru mărimea nouă, click ca să confirmi, sau tastează factorul (2 · 2,1,1.5) · "
                   "Option: din pivot · Esc anulează";
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
            p[a] = idx[std::size_t(a)] == 0 ? m_lo[a]
                : idx[std::size_t(a)] == 2  ? m_hi[a]
                                            : (m_lo[a] + m_hi[a]) * 0.5;
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
            return m_selectionPivot.value_or((m_lo + m_hi) * 0.5);
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
        if (ctx.selection.empty() && ctx.object && ctx.model != nullptr) {
            // O cameră / lumină: o cutie mică în jurul poziției (mânerele dau factorii).
            const std::vector<Vec3d> points = model::sceneObjectPoints(*ctx.model, *ctx.object);
            if (!points.empty()) {
                const double h = worldPerPixelAt(ctx.camera, points[0]) * 40.0;
                m_lo = points[0] - Vec3d { h };
                m_hi = points[0] + Vec3d { h };
                m_hasBox = true;
            }
            return;
        }
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
        for (const PreviewEdge& e : previewEdges(ctx, Target { t.selection, {}, std::nullopt })) {
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
            m_selectionPivot = ctx.selection.empty() ? std::nullopt : ctx.pivot;
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
            if (m_target.object && ctx.model != nullptr) {
                const model::SceneObjectRef object = *m_target.object;
                const Vec3d factors = m_factors;
                ctx.stack.run(
                    "Scale", [&](model::Model& model) { model::scaleSceneObject(model, object, factors); });
            } else {
                transformTarget(ctx, "Scale", m_target, scaling());
            }
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
    bool m_fromCenter = false; // Option: punctul fix e pivotul selecției (implicit centrul cutiei)
    std::optional<Vec3d> m_selectionPivot;
    Vec3d m_factors { 1.0 };
    Target m_target;
    std::vector<PreviewEdge> m_preview;
    view::PickResult m_hover;
};

// --- Pivot ----------------------------------------------------------------------------------

// Tragerea marcajului pivotului (cu Select): punctul nou se prinde ca la Move — capete, mijloace, muchii,
// fețe, axele din pivotul de pornire, ghidaje, grid, colțurile și mijloacele obiectelor — plus centrele
// fețelor și centrul cutiei selecției și originea. Unealta nu schimbă modelul: trimite punctul prin
// ToolRequests (previzualizare, apoi cel ales la eliberare), iar Document mută pivotul (un pas de undo).
class PivotTool final : public Tool {
public:
    std::string_view id() const override { return "pivot"; }

    void pointerDown(ToolContext& ctx, const PointerEvent& e) override
    {
        if (!ctx.pivot) {
            finish(ctx, false);
            return;
        }
        m_start = *ctx.pivot;
        m_current = *m_start;
        m_press = e.screen;
        m_moved = false;
        m_engine.reset();
        m_engine.restartFrom(*m_start);
        track(ctx, e);
    }

    void pointerMove(ToolContext& ctx, const PointerEvent& e) override
    {
        if (!m_start) {
            return;
        }
        m_moved = m_moved || glm::length(e.screen - m_press) > kDragThresholdPx;
        track(ctx, e);
    }

    void pointerUp(ToolContext& ctx, const PointerEvent& /*e*/) override
    {
        finish(ctx, m_start && m_moved && glm::length(m_current - *m_start) > tol::kLength);
    }

    bool key(ToolContext& ctx, const KeyEvent& e) override
    {
        if (e.key == Key::Escape) {
            finish(ctx, false);
            return true;
        }
        return handleArrow(m_engine, e.key) || Tool::key(ctx, e);
    }

    void cancel(ToolContext& /*ctx*/) override { reset(); }

    Overlay overlay() const override
    {
        Overlay o;
        if (!m_start) {
            return o;
        }
        o.lines.push_back({ *m_start, m_current, true, inference::Tint::Neutral, true });
        appendInference(o, m_inference);
        o.grips = m_grips;
        if (m_hotGrip >= 0) {
            const Grip& g = m_grips[std::size_t(m_hotGrip)];
            o.marker = InferenceMarker { g.point, g.kind, inference::Tint::Neutral,
                m_labels[std::size_t(m_hotGrip)], false };
        }
        return o;
    }

    bool ownsArrowKeys() const override { return m_start.has_value(); }

    std::string status() const override
    {
        return "Trage pivotul: se prinde de colțuri, mijloace, muchii, fețe, axe, ghidaje și grid · "
               "săgeți/Shift: "
               "blochează · Esc anulează";
    }

private:
    void track(ToolContext& ctx, const PointerEvent& e)
    {
        updateShiftLock(m_engine, e.modifiers.shift, m_shiftLock);
        m_inference = inferAt(m_engine, ctx, e.screen, m_start);
        if (m_inference.valid()) {
            m_current = m_inference.point;
        }
        // Punctele obiectului selectat și ale celui de sub cursor (sau ale celui mai apropiat), apoi centrele
        // cutiei selecției și originea.
        m_grips.clear();
        m_labels.clear();
        std::set<model::InstanceId> ids = ctx.selection.instances;
        if (const model::InstanceId near = gripObjectNear(ctx, e.screen, {})) {
            ids.insert(near);
        }
        for (const Grip& g : instanceGrips(ctx, ids)) {
            m_grips.push_back(g);
            m_labels.emplace_back(g.kind == inference::Kind::Endpoint ? "Colț" : "Mijlocul muchiei");
        }
        appendSelectionCenters(ctx);
        m_grips.push_back(
            { ctx.toWorld.inverse().applyToPoint(Vec3d { 0.0 }), inference::Kind::Endpoint, false, false });
        m_labels.emplace_back("Originea");
        m_hotGrip = nearestGrip(ctx, m_grips, e.screen, kGripRadiusPx);
        if (m_hotGrip >= 0) {
            m_grips[std::size_t(m_hotGrip)].hot = true;
            if (!m_shiftLock) {
                m_current = m_grips[std::size_t(m_hotGrip)].point;
            }
        }
        if (ctx.requests != nullptr) {
            ctx.requests->pivot = m_current;
        }
    }

    // Centrele fețelor și centrul cutiei selecției (a obiectului, orientată după el, sau a geometriei libere,
    // cu tot cu colțurile și mijloacele ei). Centrul unei fețe văzute e un punct obișnuit; restul sunt
    // estompate: se prind doar când nu e alt punct vizibil aproape.
    void appendSelectionCenters(const ToolContext& ctx)
    {
        Vec3d lo { 1e300 };
        Vec3d hi { -1e300 };
        Transform t;
        if (ctx.selection.instances.size() == 1 && ctx.model != nullptr
            && ctx.model->instances().contains(*ctx.selection.instances.begin())) {
            const model::Instance& inst = ctx.model->instance(*ctx.selection.instances.begin());
            std::tie(lo, hi) = model::bounds(*ctx.model, inst.definition);
            t = inst.transform;
        } else if (ctx.selection.instances.empty()) {
            for (geom::VertexId v : geom::verticesOf(ctx.mesh, ctx.selection)) {
                lo = glm::min(lo, ctx.mesh.position(v));
                hi = glm::max(hi, ctx.mesh.position(v));
            }
        }
        if (!(lo.x <= hi.x)) {
            return;
        }
        // Pe fiecare axă: minimul, mijlocul sau maximul. Obiectele au deja colțurile și mijloacele muchiilor.
        const bool loose = ctx.selection.instances.empty();
        for (int i = 0; i < 27; ++i) {
            const int k[3] = { i % 3, (i / 3) % 3, i / 9 };
            const int middles = int(k[0] == 1) + int(k[1] == 1) + int(k[2] == 1);
            if (!loose && middles < 2) {
                continue;
            }
            Vec3d p;
            for (int a = 0; a < 3; ++a) {
                p[a] = k[a] == 0 ? lo[a] : k[a] == 1 ? (lo[a] + hi[a]) * 0.5 : hi[a];
            }
            static constexpr const char* kLabels[]
                = { "Colț", "Mijlocul muchiei", "Centrul feței", "Centru" };
            const Vec3d w = t.applyToPoint(p);
            bool hidden = true;
            if (middles == 2) {
                const Vec3d toEye = ctx.camera.projection() == view::Projection::Orthographic
                    ? -ctx.camera.forward()
                    : ctx.camera.eye() - w;
                hidden = glm::dot(w - t.applyToPoint((lo + hi) * 0.5), toEye) <= 0.0;
            }
            m_grips.push_back(
                { w, middles == 0 ? inference::Kind::Endpoint : inference::Kind::Midpoint, false, hidden });
            m_labels.emplace_back(kLabels[middles]);
        }
    }

    void finish(ToolContext& ctx, bool commit)
    {
        if (ctx.requests != nullptr) {
            ctx.requests->pivotDone = true;
            if (commit) {
                ctx.requests->pivot = m_current;
            } else {
                ctx.requests->pivot.reset();
            }
        }
        reset();
    }

    void reset()
    {
        m_start.reset();
        m_moved = false;
        m_engine.reset();
        m_shiftLock = false;
        m_grips.clear();
        m_labels.clear();
        m_hotGrip = -1;
        m_inference = {};
    }

    std::optional<Vec3d> m_start;
    Vec3d m_current { 0.0 };
    Vec2d m_press { 0.0 };
    bool m_moved = false;
    inference::Engine m_engine;
    inference::Inference m_inference;
    bool m_shiftLock = false;
    std::vector<Grip> m_grips;
    std::vector<std::string> m_labels;
    int m_hotGrip = -1;
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

std::unique_ptr<Tool> makePivotTool()
{
    return std::make_unique<PivotTool>();
}

std::unique_ptr<Tool> makePlaceTool()
{
    return std::make_unique<PlaceTool>();
}

std::unique_ptr<Tool> makeDividerTool()
{
    return std::make_unique<DividerTool>();
}

} // namespace aion::tools
