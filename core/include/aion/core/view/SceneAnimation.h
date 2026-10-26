#pragma once

#include "aion/core/view/Camera.h"

#include <vector>

// Animația scenelor (ca „View → Animation” din SketchUp): camera stă `delay` secunde pe fiecare scenă, apoi trece
// lin în `transition` secunde la următoarea. Cu `loop`, după ultima scenă revine la prima.
// Aceeași cronologie servește redării în timp real și exportului cadru cu cadru.
namespace aion::view {

class SceneAnimation {
public:
    SceneAnimation(std::vector<CameraPose> poses, double transitionSeconds, double delaySeconds, bool loop);

    // Durata unei treceri complete (cu `loop`: până înapoi la prima scenă).
    [[nodiscard]] double duration() const;
    [[nodiscard]] CameraPose poseAt(double seconds) const;
    // Scena pe care stă sau spre care merge camera la momentul dat.
    [[nodiscard]] std::size_t sceneAt(double seconds) const;
    [[nodiscard]] bool empty() const { return m_poses.empty(); }

private:
    struct Position {
        std::size_t from = 0;
        std::size_t to = 0;
        double blend = 0.0; // 0 = pe `from`, 1 = pe `to`
    };
    [[nodiscard]] Position positionAt(double seconds) const;

    std::vector<CameraPose> m_poses;
    double m_transition;
    double m_delay;
    bool m_loop;
};

} // namespace aion::view
