#include "aion/core/model/Stretch.h"

#include "aion/core/geom/Operations.h"
#include "aion/core/model/Carcass.h"
#include "aion/core/model/Copying.h"
#include "aion/core/model/Furniture.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Parametric.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace aion::model {

namespace {

constexpr double kEps = 1e-6;

struct Plane {
    double at = 0.0;
    double distance = 0.0;
};

// Ce face „Întinde” cu o piesă (în coordonatele corpului: `toBody` = părintele piesei → corp).
struct Op {
    enum class Kind { Move, Grow, MoveVertices };
    Kind kind = Kind::Move;
    InstanceId instance; // Move, Grow
    DefinitionId definition; // MoveVertices (geometria liberă a definiției)
    math::Transform toBody; // părintele piesei → corp
    double shift = 0.0; // mm pe axă
    const char* key = nullptr; // Grow: "length" / "width"
    double growth = 0.0;
};

struct Plan {
    std::vector<Op> ops;
    StretchResult refusal { true, StretchRefusal::None, {} };
    std::vector<InstanceId> needUnique; // piese de schimbat a căror definiție e folosită și în afara corpului
};

// Suma distanțelor planelor de sub `x` (piesa e de partea lor +) și a celor din (lo, hi) (piesa e tăiată).
double shiftBelow(const std::vector<Plane>& planes, double x)
{
    double s = 0.0;
    for (const Plane& p : planes) {
        if (p.at <= x + kEps) {
            s += p.distance;
        }
    }
    return s;
}

double growthInside(const std::vector<Plane>& planes, double lo, double hi)
{
    double g = 0.0;
    for (const Plane& p : planes) {
        if (p.at > lo + kEps && p.at < hi - kEps) {
            g += p.distance;
        }
    }
    return g;
}

// Intervalul pe axă al cutiei definiției, cu transformarea `t` (definiție → corp).
std::pair<double, double> extent(
    const Model& model, DefinitionId definition, const math::Transform& t, int axis)
{
    const auto [lo, hi] = bounds(model, definition);
    double a = std::numeric_limits<double>::infinity();
    double b = -a;
    if (lo.x > hi.x) {
        return { 0.0, -1.0 };
    }
    for (int k = 0; k < 8; ++k) {
        const math::Vec3d c { (k & 1) ? hi.x : lo.x, (k & 2) ? hi.y : lo.y, (k & 4) ? hi.z : lo.z };
        const double v = t.applyToPoint(c)[axis];
        a = std::min(a, v);
        b = std::max(b, v);
    }
    return { a, b };
}

class Planner {
public:
    Planner(const Model& model, DefinitionId body, int axis, const std::vector<Plane>& planes)
        : m_model(model)
        , m_axis(axis)
        , m_planes(planes)
    {
        collect(body);
    }

    Plan plan(DefinitionId body)
    {
        Plan out;
        visit(body, math::Transform {}, out);
        return out;
    }

private:
    // Definițiile din corp (corpul și tot ce conține).
    void collect(DefinitionId d)
    {
        if (!m_inside.insert(d).second) {
            return;
        }
        for (InstanceId c : m_model.childrenOf(d)) {
            collect(m_model.instance(c).definition);
        }
    }

    bool usedOutside(DefinitionId d) const
    {
        const std::vector<InstanceId> users = m_model.instancesOf(d);
        return std::any_of(users.begin(), users.end(),
            [&](InstanceId id) { return !m_inside.contains(m_model.instance(id).parent); });
    }

    void refuse(Plan& out, StretchRefusal reason, InstanceId piece)
    {
        if (out.refusal.ok) {
            out.refusal = { false, reason, piece };
        }
    }

    void visit(DefinitionId parent, const math::Transform& toBody, Plan& out)
    {
        // Geometria liberă: tăiată = refuz; de partea + = se mută.
        const geom::Mesh& mesh = m_model.mesh(parent);
        if (!mesh.vertices().empty()) {
            double lo = std::numeric_limits<double>::infinity();
            double hi = -lo;
            for (const geom::Vertex& v : mesh.vertices().values()) {
                const double x = toBody.applyToPoint(v.position)[m_axis];
                lo = std::min(lo, x);
                hi = std::max(hi, x);
            }
            if (growthInside(m_planes, lo, hi) != 0.0) {
                refuse(out, StretchRefusal::FreeGeometry, {});
            } else if (const double s = shiftBelow(m_planes, lo); s != 0.0) {
                out.ops.push_back({ Op::Kind::MoveVertices, {}, parent, toBody, s });
            }
        }
        for (InstanceId id : m_model.childrenOf(parent)) {
            const Instance& inst = m_model.instance(id);
            const Definition& def = m_model.definition(inst.definition);
            const math::Transform m = toBody * inst.transform;
            const auto [lo, hi] = extent(m_model, inst.definition, m, m_axis);
            if (lo > hi) {
                continue;
            }
            const double shift = shiftBelow(m_planes, lo);
            const double growth = growthInside(m_planes, lo, hi);
            const bool hardware = def.part && def.part->kind == PartSpec::Kind::Hardware;
            if (hardware) {
                // Feroneria nu se întinde: se mută după pivotul ei.
                const double at = m.applyToPoint(pivotLocal(m_model, id))[m_axis];
                if (const double s = shiftBelow(m_planes, at); s != 0.0) {
                    out.ops.push_back({ Op::Kind::Move, id, {}, toBody, s });
                }
                continue;
            }
            if (growth == 0.0) {
                if (shift != 0.0) {
                    out.ops.push_back({ Op::Kind::Move, id, {}, toBody, shift });
                }
                continue;
            }
            if (def.part && def.part->kind == PartSpec::Kind::Board) {
                planBoard(id, inst, def, m, toBody, shift, growth, out);
                continue;
            }
            if (!def.generator.empty() || def.part) {
                refuse(out, StretchRefusal::FreeGeometry, id); // un obiect dinamic tăiat
                continue;
            }
            // Un grup fără date: planele trec prin el, deci coborâm.
            if (usedOutside(inst.definition)) {
                out.needUnique.push_back(id);
            }
            visit(inst.definition, m, out);
        }
    }

    void planBoard(InstanceId id, const Instance& inst, const Definition& def, const math::Transform& m,
        const math::Transform& toBody, double shift, double growth, Plan& out)
    {
        if (def.generator != kBoardGenerator || boardState(m_model, id, m).state != BoardState::Matches) {
            refuse(out, StretchRefusal::ModifiedBoard, id);
            return;
        }
        // Axa locală a plăcii aliniată cu axa întinderii (0 = lungime, 1 = lățime, 2 = grosime).
        int local = -1;
        double sign = 1.0;
        for (int k = 0; k < 3; ++k) {
            const math::Vec3d c = glm::normalize(math::Vec3d(m.matrix()[k]));
            if (std::abs(std::abs(c[m_axis]) - 1.0) < 1e-6) {
                local = k;
                sign = c[m_axis] > 0.0 ? 1.0 : -1.0;
            }
        }
        if (local < 0) {
            refuse(out, StretchRefusal::FreeGeometry, id); // placă rotită oblic față de axă
            return;
        }
        if (local == 2) {
            refuse(out, StretchRefusal::ThroughThickness, id);
            return;
        }
        if (usedOutside(inst.definition)) {
            out.needUnique.push_back(id);
        }
        // Originea plăcii e la capătul ei minim pe axa locală: dacă axa e inversată, capătul acela e partea +
        // și se mută cu tot cu creșterea.
        Op op { Op::Kind::Grow, id, {}, toBody, shift + (sign < 0.0 ? growth : 0.0) };
        op.key = local == 0 ? "length" : "width";
        op.growth = growth;
        out.ops.push_back(op);
    }

    const Model& m_model;
    int m_axis;
    std::vector<Plane> m_planes;
    std::set<DefinitionId> m_inside;
};

void moveInstance(Model& model, const Op& op, int axis)
{
    math::Vec3d v { 0.0 };
    v[axis] = op.shift;
    Instance inst = model.instance(op.instance);
    inst.transform = op.toBody.inverse() * math::Transform::translation(v) * op.toBody * inst.transform;
    model.setInstance(op.instance, inst);
}

} // namespace

StretchResult stretch(Model& model, InstanceId body, int axis, std::vector<double> planes, double distance)
{
    if (!model.instances().contains(body) || axis < 0 || axis > 2 || planes.empty()) {
        return { false, StretchRefusal::NotABody, {} };
    }
    const DefinitionId bodyDef = model.instance(body).definition;
    const Definition& def = model.definition(bodyDef);
    // Un corp din bibliotecă: câmpul lui (lățimea, adâncimea, înălțimea); reconcilierea face restul.
    if (isCarcassGenerator(def.generator)) {
        static constexpr std::array<const char*, 3> kFields { "width", "depth", "height" };
        const auto it = def.parameters.find(kFields[static_cast<std::size_t>(axis)]);
        const double current = it != def.parameters.end() ? it->second : 0.0;
        setParameter(model, bodyDef, kFields[static_cast<std::size_t>(axis)], current + distance);
        return { true, StretchRefusal::None, {} };
    }
    if (model.childrenOf(bodyDef).empty()) {
        return { false, StretchRefusal::NotABody, {} };
    }
    std::vector<Plane> list;
    for (const double p : planes) {
        list.push_back({ p, distance / static_cast<double>(planes.size()) });
    }
    Plan plan = Planner(model, bodyDef, axis, list).plan(bodyDef);
    if (!plan.refusal.ok) {
        return plan.refusal;
    }
    // Piesele de schimbat folosite și în afara corpului devin unice, apoi planul se refac pe ele (id-urile
    // din grupurile copiate se schimbă).
    for (int guard = 0; !plan.needUnique.empty() && guard < 64; ++guard) {
        makeUniqueDeep(model, plan.needUnique.front());
        plan = Planner(model, bodyDef, axis, list).plan(bodyDef);
    }
    for (const Op& op : plan.ops) {
        switch (op.kind) {
        case Op::Kind::Move:
            moveInstance(model, op, axis);
            break;
        case Op::Kind::Grow: {
            const DefinitionId d = model.instance(op.instance).definition;
            Definition board = model.definition(d);
            board.parameters[op.key] += op.growth;
            model.setDefinition(d, board);
            rebuildBoard(model, d);
            if (op.shift != 0.0) {
                moveInstance(model, op, axis);
            }
            break;
        }
        case Op::Kind::MoveVertices: {
            math::Vec3d v { 0.0 };
            v[axis] = op.shift;
            const math::Vec3d local = op.toBody.inverse().applyToVector(v);
            geom::Mesh& mesh = model.mesh(op.definition);
            const std::set<geom::VertexId> all(mesh.vertices().keys().begin(), mesh.vertices().keys().end());
            geom::transformVertices(mesh, all, math::Transform::translation(local));
            break;
        }
        }
    }
    return { true, StretchRefusal::None, {} };
}

std::vector<double> equalPlanes(const Model& model, InstanceId body, int axis, int n)
{
    std::vector<double> out;
    if (!model.instances().contains(body) || axis < 0 || axis > 2 || n < 1) {
        return out;
    }
    const auto [lo, hi] = bounds(model, model.instance(body).definition);
    if (lo.x > hi.x) {
        return out;
    }
    const double size = hi[axis] - lo[axis];
    for (int i = 0; i < n; ++i) {
        out.push_back(lo[axis] + (i + 0.5) * size / n);
    }
    return out;
}

} // namespace aion::model
