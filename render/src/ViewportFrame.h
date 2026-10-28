#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <QImage>

#include <optional>
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
    glm::vec4 backColor { 0.0f, 0.0f, 0.0f, -1.0f }; // materialul de pe verso (a < 0 = ca fața / culoarea versoului)
    glm::vec4 uv { 0.0f }; // coordonatele texturii pe față (xy) și pe verso (zw)
    glm::vec2 layers { -1.0f, -1.0f }; // stratul texturii feței și al versoului în array (< 0 = fără textură)
};

// O lumină a modelului, gata pentru shader (cel mult kMaxLights).
struct LightData {
    int kind = 0; // 0 punctuală, 1 spot, 2 direcțională, 3 suprafață
    glm::vec3 position { 0.0f };
    glm::vec3 direction { 0.0f, 0.0f, -1.0f }; // încotro bate lumina
    glm::vec3 color { 1.0f }; // liniar, înmulțit cu intensitatea
    float range = 6000.0f;
    float cosOuter = 0.0f; // spot: cosinusul jumătății de con
    float cosInner = 0.0f; // spot: unde începe marginea estompată
    float size = 0.0f; // suprafață: latura medie (înmoaie apropierea)
};
inline constexpr int kMaxLights = 8;

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
    double gridStep = 0.0; // pasul fix al grid-ului (Snap la grid), 0 = automat
    int shadowMapSize = 2048; // calitatea umbrelor (1024, 2048, 4096)
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
    // Texturile materialelor (câte un strat în array-ul din shader, în ordinea din model) și semnătura lor:
    // renderer-ul le reîncarcă doar când semnătura se schimbă.
    std::vector<QImage> textures;
    std::size_t textureSignature = 0;
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
    // Luminile modelului (în afară de soare).
    std::vector<LightData> lights;
    // Mediul: culorile cerului (zenit, orizont) și ale solului (rgb liniar, a = intensitatea); reflexiile (x).
    glm::vec4 envZenith { 0.62f, 0.68f, 0.78f, 0.55f };
    glm::vec4 envHorizon { 0.52f, 0.54f, 0.58f, 0.55f };
    glm::vec4 envGround { 0.42f, 0.40f, 0.38f, 0.55f };
    float envReflection = 0.0f;
    bool ambientOcclusion = false;
    // Antialiasing progresiv: indexul cadrului acumulat (0 = cadru nou, scena s-a schimbat); -1 = oprit.
    int accumulateIndex = -1;
    // Spotul care aruncă umbre (primul cu „Aruncă umbre”): indexul lui în `lights`; -1 = niciunul.
    int shadowSpot = -1;
    // Planul de secțiune activ (n, d cu n·x = d); geometria de partea normalei se ascunde.
    std::optional<glm::dvec4> section;
    // Depth of field (doar prin cameră și la export): lentila camerei.
    struct DepthOfField {
        bool enabled = false;
        float focus = 5000.0f; // mm
        float focal = 35.0f; // mm
        float fStop = 2.8f;
        float pixelsPerMm = 50.0f; // pixeli fizici pe mm de senzor (latura lungă a cadrului)
    } dof;
};

} // namespace aion::render
