#include "aion/core/model/MaterialLibrary.h"

#include <array>

namespace aion::model {

namespace {

// Un material inclus, cu categoria și cheia lui (ADR 0044). Cheia = numele fără diacritice, cu „-”.
Material entry(const char* name, Color color, float roughness, float metallic, const char* category, const char* key,
    math::Vec2d textureSize = { 1000.0, 1000.0 })
{
    Material m;
    m.name = name;
    m.color = color;
    m.roughness = roughness;
    m.metallic = metallic;
    m.textureSize = textureSize;
    m.category = category;
    m.libraryKey = key;
    return m;
}

} // namespace

std::span<const Material> builtinMaterials()
{
    // Ordinea nu se schimbă: indicii sunt folosiți de scripturi (`material:front,24`) și de fișierele vechi; cele noi
    // se adaugă la sfârșit.
    static const std::array<Material, 37> kMaterials { {
        entry("Alb mat", { 242, 241, 236, 255 }, 0.9f, 0.0f, "", "alb-mat"),
        entry("Gips", { 228, 225, 216, 255 }, 0.95f, 0.0f, "", "gips"),
        entry("Gri deschis", { 190, 192, 196, 255 }, 0.85f, 0.0f, "", "gri-deschis"),
        entry("Grafit", { 64, 66, 72, 255 }, 0.7f, 0.0f, "", "grafit"),
        entry("Negru", { 28, 28, 30, 255 }, 0.6f, 0.0f, "", "negru"),
        entry("Beton", { 160, 158, 150, 255 }, 0.95f, 0.0f, "stone", "beton"),
        entry("Beton închis", { 112, 110, 104, 255 }, 0.95f, 0.0f, "stone", "beton-inchis"),
        entry("Cărămidă", { 168, 82, 58, 255 }, 0.9f, 0.0f, "", "caramida"),
        entry("Teracotă", { 196, 112, 72, 255 }, 0.85f, 0.0f, "", "teracota"),
        entry("Piatră", { 182, 172, 152, 255 }, 0.9f, 0.0f, "stone", "piatra"),
        entry("Lemn de stejar", { 176, 132, 86, 255 }, 0.7f, 0.0f, "board-decor", "lemn-de-stejar"),
        entry("Lemn de nuc", { 104, 70, 46, 255 }, 0.65f, 0.0f, "board-decor", "lemn-de-nuc"),
        entry("Lemn de pin", { 214, 178, 128, 255 }, 0.75f, 0.0f, "board-decor", "lemn-de-pin"),
        entry("Bambus", { 198, 170, 110, 255 }, 0.7f, 0.0f, "board-decor", "bambus"),
        entry("Sticlă", { 170, 205, 225, 90 }, 0.05f, 0.0f, "glass", "sticla"),
        entry("Sticlă fumurie", { 90, 100, 108, 120 }, 0.05f, 0.0f, "glass", "sticla-fumurie"),
        entry("Oțel periat", { 170, 172, 176, 255 }, 0.35f, 1.0f, "metal", "otel-periat"),
        entry("Aluminiu", { 200, 202, 206, 255 }, 0.3f, 1.0f, "metal", "aluminiu"),
        entry("Cupru", { 186, 110, 72, 255 }, 0.35f, 1.0f, "metal", "cupru"),
        entry("Alamă", { 196, 160, 84, 255 }, 0.3f, 1.0f, "metal", "alama"),
        entry("Iarbă", { 96, 140, 70, 255 }, 0.95f, 0.0f, "", "iarba"),
        entry("Apă", { 60, 120, 160, 160 }, 0.05f, 0.0f, "", "apa"),
        entry("Albastru", { 59, 130, 246, 255 }, 0.6f, 0.0f, "", "albastru"),
        entry("Verde salvie", { 140, 160, 130, 255 }, 0.8f, 0.0f, "", "verde-salvie"),
        // Cu textură (culoarea albă = textura neschimbată; mărimea = o repetiție, în mm).
        entry("Cărămidă aparentă", { 255, 255, 255, 255 }, 0.9f, 0.0f, "", "caramida-aparenta", { 1000.0, 1000.0 }),
        entry("Parchet de stejar", { 255, 255, 255, 255 }, 0.55f, 0.0f, "", "parchet-de-stejar", { 1200.0, 1200.0 }),
        entry("Lemn (fibră)", { 255, 255, 255, 255 }, 0.6f, 0.0f, "board-decor", "lemn-fibra", { 800.0, 800.0 }),
        entry("Beton turnat", { 255, 255, 255, 255 }, 0.95f, 0.0f, "stone", "beton-turnat", { 2000.0, 2000.0 }),
        entry("Gresie 30 × 30", { 255, 255, 255, 255 }, 0.4f, 0.0f, "stone", "gresie-30-x-30", { 1200.0, 1200.0 }),
        // Adăugate după cele texturate (indicii de mai sus rămân aceiași în fișierele și scripturile existente).
        entry("Verde brad", { 46, 96, 58, 255 }, 0.9f, 0.0f, "", "verde-brad"),
        // Decoruri de plăci și lemn masiv (ADR 0044).
        entry("Alb (decor)", { 242, 241, 236, 255 }, 0.7f, 0.0f, "board-decor", "alb-decor"),
        entry("Gri (decor)", { 150, 152, 156, 255 }, 0.7f, 0.0f, "board-decor", "gri-decor"),
        entry("Nuc (decor)", { 150, 108, 78, 255 }, 0.65f, 0.0f, "board-decor", "nuc-decor", { 800.0, 800.0 }),
        entry("Stejar (decor)", { 236, 206, 160, 255 }, 0.65f, 0.0f, "board-decor", "stejar-decor", { 800.0, 800.0 }),
        entry("Wenge (decor)", { 96, 74, 60, 255 }, 0.6f, 0.0f, "board-decor", "wenge-decor", { 800.0, 800.0 }),
        entry("Nuc masiv", { 150, 108, 78, 255 }, 0.6f, 0.0f, "wood", "nuc-masiv", { 800.0, 800.0 }),
        entry("Stejar masiv", { 236, 206, 160, 255 }, 0.6f, 0.0f, "wood", "stejar-masiv", { 800.0, 800.0 }),
    } };
    return kMaterials;
}

std::string_view builtinTexture(std::size_t index)
{
    // Pe index (culoarea materialului nuanțează textura).
    switch (index) {
    case 24:
        return "brick";
    case 25:
        return "parquet";
    case 26:
    case 32:
    case 33:
    case 34:
    case 35:
    case 36:
        return "wood";
    case 27:
        return "concrete";
    case 28:
        return "tiles";
    default:
        return {};
    }
}

std::optional<std::size_t> libraryMaterialByKey(std::string_view key)
{
    const auto library = builtinMaterials();
    for (std::size_t i = 0; i < library.size(); ++i) {
        if (!key.empty() && library[i].libraryKey == key) {
            return i;
        }
    }
    return std::nullopt;
}

} // namespace aion::model
