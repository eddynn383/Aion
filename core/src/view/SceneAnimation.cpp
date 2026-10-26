#include "aion/core/view/SceneAnimation.h"

#include "aion/core/view/CameraMotion.h"

#include <algorithm>
#include <cmath>

namespace aion::view {

SceneAnimation::SceneAnimation(std::vector<CameraPose> poses, double transitionSeconds, double delaySeconds, bool loop)
    : m_poses(std::move(poses))
    , m_transition(std::max(transitionSeconds, 0.0))
    , m_delay(std::max(delaySeconds, 0.0))
    , m_loop(loop && m_poses.size() > 1)
{
}

double SceneAnimation::duration() const
{
    if (m_poses.empty()) {
        return 0.0;
    }
    const auto n = static_cast<double>(m_poses.size());
    const double transitions = m_loop ? n : n - 1.0;
    return n * m_delay + transitions * m_transition;
}

SceneAnimation::Position SceneAnimation::positionAt(double seconds) const
{
    if (m_poses.size() < 2) {
        return {};
    }
    double t = std::clamp(seconds, 0.0, duration());
    const double step = m_delay + m_transition;
    const std::size_t segments = m_loop ? m_poses.size() : m_poses.size() - 1;
    for (std::size_t i = 0; i < segments; ++i) {
        const std::size_t next = (i + 1) % m_poses.size();
        if (t < m_delay || (step <= 0.0)) {
            return { i, next, 0.0 };
        }
        if (t < step) {
            return { i, next, m_transition > 0.0 ? (t - m_delay) / m_transition : 1.0 };
        }
        t -= step;
    }
    // Ultima scenă (fără loop): rămâne pe ea până la sfârșit.
    const std::size_t last = m_loop ? 0 : m_poses.size() - 1;
    return { last, last, 0.0 };
}

CameraPose SceneAnimation::poseAt(double seconds) const
{
    if (m_poses.empty()) {
        return {};
    }
    const Position p = positionAt(seconds);
    return p.blend <= 0.0 ? m_poses[p.from] : interpolatePose(m_poses[p.from], m_poses[p.to], easeInOutCubic(p.blend));
}

std::size_t SceneAnimation::sceneAt(double seconds) const
{
    const Position p = positionAt(seconds);
    return p.blend > 0.0 ? p.to : p.from;
}

} // namespace aion::view
