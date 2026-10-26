#pragma once

#include "aion/core/view/Camera.h"

// Mișcările camerei care se întind pe mai multe cadre: tranziții animate între vederi,
// inerția după orbit și zoom-ul lin. Viewport-ul apelează update() la fiecare cadru
// cât timp isActive() e adevărat.
namespace aion::view {

// Interpolarea între două poziții: ținta liniar, unghiurile pe drumul cel mai scurt, distanța geometric.
[[nodiscard]] CameraPose interpolatePose(const CameraPose& a, const CameraPose& b, double t);
[[nodiscard]] double easeInOutCubic(double t);

class CameraMotion {
public:
    static constexpr double kDefaultTransitionSeconds = 0.35;

    // Tranziție animată spre o altă poziție (vederi standard, Zoom Extents).
    void animateTo(const Camera& camera, const CameraPose& target,
        double durationSeconds = kDefaultTransitionSeconds);

    // Viteza unghiulară (rad/s) cu care orbita continuă după ce utilizatorul dă drumul.
    void setOrbitVelocity(Vec2d radiansPerSecond);

    // Zoom distribuit pe câteva cadre. factor < 1 = apropiere.
    void zoomSmoothly(double factor, const Vec3d& anchor);

    // Utilizatorul a preluat controlul: oprește tot ce era în desfășurare.
    void stop();

    // Fără animații și inerție (setarea „Reduce Motion” din sistem).
    void setReducedMotion(bool reduced) { m_reducedMotion = reduced; }

    [[nodiscard]] bool isActive() const;

    // Avansează cu dt secunde. Întoarce true dacă a modificat camera.
    bool update(Camera& camera, double dtSeconds);

private:
    struct Transition {
        CameraPose from;
        CameraPose to;
        double elapsed = 0.0;
        double duration = 0.0;
    };

    std::optional<Transition> m_transition;
    Vec2d m_orbitVelocity { 0.0, 0.0 };
    double m_pendingZoomLog = 0.0;
    Vec3d m_zoomAnchor { 0.0, 0.0, 0.0 };
    bool m_reducedMotion = false;
};

} // namespace aion::view
