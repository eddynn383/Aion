#include "ToolHelpers.h"

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

} // namespace aion::tools
