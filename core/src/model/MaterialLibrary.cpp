#include "aion/core/model/MaterialLibrary.h"

#include <array>

namespace aion::model {

std::span<const Material> builtinMaterials()
{
    static const std::array<Material, 30> kMaterials { {
        { "Alb mat", { 242, 241, 236, 255 }, 0.9f, 0.0f },
        { "Gips", { 228, 225, 216, 255 }, 0.95f, 0.0f },
        { "Gri deschis", { 190, 192, 196, 255 }, 0.85f, 0.0f },
        { "Grafit", { 64, 66, 72, 255 }, 0.7f, 0.0f },
        { "Negru", { 28, 28, 30, 255 }, 0.6f, 0.0f },
        { "Beton", { 160, 158, 150, 255 }, 0.95f, 0.0f },
        { "Beton închis", { 112, 110, 104, 255 }, 0.95f, 0.0f },
        { "Cărămidă", { 168, 82, 58, 255 }, 0.9f, 0.0f },
        { "Teracotă", { 196, 112, 72, 255 }, 0.85f, 0.0f },
        { "Piatră", { 182, 172, 152, 255 }, 0.9f, 0.0f },
        { "Lemn de stejar", { 176, 132, 86, 255 }, 0.7f, 0.0f },
        { "Lemn de nuc", { 104, 70, 46, 255 }, 0.65f, 0.0f },
        { "Lemn de pin", { 214, 178, 128, 255 }, 0.75f, 0.0f },
        { "Bambus", { 198, 170, 110, 255 }, 0.7f, 0.0f },
        { "Sticlă", { 170, 205, 225, 90 }, 0.05f, 0.0f },
        { "Sticlă fumurie", { 90, 100, 108, 120 }, 0.05f, 0.0f },
        { "Oțel periat", { 170, 172, 176, 255 }, 0.35f, 1.0f },
        { "Aluminiu", { 200, 202, 206, 255 }, 0.3f, 1.0f },
        { "Cupru", { 186, 110, 72, 255 }, 0.35f, 1.0f },
        { "Alamă", { 196, 160, 84, 255 }, 0.3f, 1.0f },
        { "Iarbă", { 96, 140, 70, 255 }, 0.95f, 0.0f },
        { "Apă", { 60, 120, 160, 160 }, 0.05f, 0.0f },
        { "Albastru", { 59, 130, 246, 255 }, 0.6f, 0.0f },
        { "Verde salvie", { 140, 160, 130, 255 }, 0.8f, 0.0f },
        // Cu textură (culoarea albă = textura neschimbată; mărimea = o repetiție, în mm).
        { "Cărămidă aparentă", { 255, 255, 255, 255 }, 0.9f, 0.0f, {}, { 1000.0, 1000.0 } },
        { "Parchet de stejar", { 255, 255, 255, 255 }, 0.55f, 0.0f, {}, { 1200.0, 1200.0 } },
        { "Lemn (fibră)", { 255, 255, 255, 255 }, 0.6f, 0.0f, {}, { 800.0, 800.0 } },
        { "Beton turnat", { 255, 255, 255, 255 }, 0.95f, 0.0f, {}, { 2000.0, 2000.0 } },
        { "Gresie 30 × 30", { 255, 255, 255, 255 }, 0.4f, 0.0f, {}, { 1200.0, 1200.0 } },
        // Adăugate după cele texturate (indicii de mai sus rămân aceiași în fișierele și scripturile existente).
        { "Verde brad", { 46, 96, 58, 255 }, 0.9f, 0.0f },
    } };
    return kMaterials;
}

std::string_view builtinTexture(std::size_t index)
{
    static constexpr std::array<std::string_view, 5> kTextures { "brick", "parquet", "wood", "concrete", "tiles" };
    constexpr std::size_t kFirstTextured = 24;
    return index >= kFirstTextured && index - kFirstTextured < kTextures.size() ? kTextures[index - kFirstTextured]
                                                                               : std::string_view {};
}

} // namespace aion::model
