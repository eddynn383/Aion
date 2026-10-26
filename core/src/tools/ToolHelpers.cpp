#include "ToolHelpers.h"

#include "aion/core/view/ModelPicking.h"

#include <glm/geometric.hpp>

namespace aion::tools {

void appendInference(Overlay& o, const inference::Inference& inf)
{
    for (const inference::Guide& g : inf.guides) {
        o.lines.push_back({ g.a, g.b, g.dashed, g.tint, true });
    }
    if (inf.valid()) {
        const inference::Tint tint = inf.guides.empty() ? inference::Tint::Neutral : inf.guides.front().tint;
        o.marker = InferenceMarker { inf.point, inf.kind, tint, std::string(inference::label(inf)), inf.locked };
    }
}

view::PickResult inferenceHover(const inference::Inference& inf)
{
    view::PickResult h;
    switch (inf.kind) {
    case inference::Kind::OnEdge:
    case inference::Kind::Midpoint:
    case inference::Kind::Intersection:
        if (inf.edge) {
            h.kind = view::PickResult::Kind::Edge;
            h.edge = inf.edge;
            h.point = inf.point;
        }
        break;
    case inference::Kind::OnFace:
        h.kind = view::PickResult::Kind::Face;
        h.face = inf.face;
        h.point = inf.point;
        break;
    default:
        break;
    }
    return h;
}

bool handleArrow(inference::Engine& engine, Key key)
{
    switch (key) {
    case Key::Right:
        engine.toggleAxisLock(inference::Axis::X);
        return true;
    case Key::Left:
        engine.toggleAxisLock(inference::Axis::Y);
        return true;
    case Key::Up:
        engine.toggleAxisLock(inference::Axis::Z);
        return true;
    case Key::Down:
        engine.toggleReferenceLock();
        return true;
    default:
        return false;
    }
}

void updateShiftLock(inference::Engine& engine, bool shift, bool& shiftLock)
{
    if (shift && !shiftLock && !engine.locked()) {
        engine.lockCurrent();
        shiftLock = engine.locked();
    } else if (!shift && shiftLock) {
        engine.unlock();
        shiftLock = false;
    }
}

view::PickResult pickWithInstances(const ToolContext& ctx, Vec2d screen, view::PickOptions options)
{
    const view::PickResult geometry = view::pick(ctx.mesh, ctx.camera, screen, options);
    if (ctx.model == nullptr) {
        return geometry;
    }
    const auto hit = view::pickInstance(*ctx.model, ctx.context, ctx.camera, screen);
    if (!hit) {
        return geometry;
    }
    if (geometry.hit()) {
        const math::Ray ray = ctx.camera.rayAt(screen);
        const double t = glm::dot(geometry.point - ray.origin, ray.direction);
        // Muchiile și vârfurile de pe suprafața unei instanțe rămân alegibile (toleranță ca la picking).
        if (t <= hit->t + 1.0 + hit->t * 1e-4) {
            return geometry;
        }
    }
    view::PickResult r;
    r.kind = view::PickResult::Kind::Instance;
    r.instance = hit->instance;
    r.point = hit->point;
    return r;
}

} // namespace aion::tools
