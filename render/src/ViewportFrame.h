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
    float state = 0.0f; // 0 = normal, 1 = sub cursor, 2 = selectat, 3 = estompat
    glm::vec4 color { 0.0f, 0.0f, 0.0f, -1.0f }; // culoarea materialului (a = opacitate; a < 0 = fără material)
    glm::vec2 material { 0.85f, 0.0f }; // rugozitate, metalic
};

struct ViewportColors {
    glm::vec4 backgroundTop;
    glm::vec4 backgroundBottom;
    glm::vec4 gridMinor;
    glm::vec4 gridMajor;
    glm::vec4 faceFront;
    glm::vec4 faceBack;
    glm::vec4 accent;
};

struct ViewportFrame {
    glm::dmat4 view { 1.0 };
    glm::dmat4 projection { 1.0 };
    glm::dvec3 eye { 0.0 };
    glm::dvec3 forward { 0.0, 1.0, 0.0 };
    double targetDistance = 1.0;
    bool orthographic = false;
    bool gridVisible = true;
    bool hasSelectedFaces = false; // activează conturul selecției
    float devicePixelRatio = 1.0f;

    ViewportColors colors {};
    std::vector<LineInstance> lines;
    std::vector<PointInstance> points;
    // Desenate după tot restul, fără test de adâncime: ghidajele și marcajele inferenței.
    std::vector<LineInstance> topLines;
    std::vector<PointInstance> topPoints;
    std::vector<MeshVertex> triangles;
    // Fețele transparente (sticlă, x-ray): desenate după cele opace, cu amestec.
    std::vector<MeshVertex> transparent;
    // Crește când `triangles` se schimbă, ca renderer-ul să reîncarce buffer-ul numai atunci.
    unsigned meshRevision = 0;

    // Stilul de afișare: 0 shaded, 1 clay, 2 linii ascunse, 3 wireframe, 4 x-ray.
    int style = 0;
    bool fog = false;
    // Soarele: direcția spre el (normalizată) și intensitatea; umbrele și cutia scenei (pentru harta de umbre).
    glm::dvec3 sunDirection { -0.4, -0.6, 0.7 };
    double sunIntensity = 1.0;
    bool shadows = false;
    glm::dvec3 sceneMin { -5000.0, -5000.0, 0.0 };
    glm::dvec3 sceneMax { 5000.0, 5000.0, 3000.0 };
};

} // namespace aion::render
