#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <vector>

// Datele pe care Viewport (firul GUI) le predă renderer-ului (firul de randare) la fiecare cadru.
namespace aion::render {

struct LineInstance {
    glm::vec3 a;
    glm::vec3 b;
    glm::vec4 color;
    glm::vec2 style; // x = grosime în px logici, y = lungimea liniuței (0 = linie continuă)
};
static_assert(sizeof(LineInstance) == 48);

struct PointInstance {
    glm::vec3 position;
    glm::vec4 color;
    float sizePx;
};
static_assert(sizeof(PointInstance) == 32);

struct MeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct ViewportColors {
    glm::vec4 backgroundTop;
    glm::vec4 backgroundBottom;
    glm::vec4 gridMinor;
    glm::vec4 gridMajor;
    glm::vec4 faceFront;
    glm::vec4 faceBack;
};

struct ViewportFrame {
    glm::dmat4 view { 1.0 };
    glm::dmat4 projection { 1.0 };
    glm::dvec3 eye { 0.0 };
    glm::dvec3 forward { 0.0, 1.0, 0.0 };
    double targetDistance = 1.0;
    bool orthographic = false;
    bool gridVisible = true;
    float devicePixelRatio = 1.0f;

    ViewportColors colors {};
    std::vector<LineInstance> lines;
    std::vector<PointInstance> points;
    std::vector<MeshVertex> triangles;
    // Crește când `triangles` se schimbă, ca renderer-ul să reîncarce buffer-ul numai atunci.
    unsigned meshRevision = 0;
};

} // namespace aion::render
