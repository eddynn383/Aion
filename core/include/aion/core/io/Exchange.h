#pragma once

#include "aion/core/model/Model.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// Schimbul de modele cu alte programe: OBJ (+ MTL) și glTF / GLB (ADR 0009).
// Aion lucrează în mm cu Z în sus; OBJ și glTF folosesc de obicei metri cu Y în sus.
namespace aion::io {

struct ExportOptions {
    double scale = 0.001; // mm → m
    bool yUp = true; // (x, y, z) → (x, z, −y)
    bool includeHidden = false;
};

// OBJ: fiecare grup sau obiect devine un `o`; fețele sunt triangulate, cu câte o normală pe față.
[[nodiscard]] std::string exportObj(const model::Model& model, const ExportOptions& options = {},
    std::string_view materialLibrary = "model.mtl");
// Materialele folosite (`newmtl` după numele din model, cu `map_Kd` pentru texturi) și fișierele imaginilor de
// scris lângă .mtl: (nume, octeți).
[[nodiscard]] std::string exportMtl(const model::Model& model);
[[nodiscard]] std::vector<std::pair<std::string, std::vector<std::uint8_t>>> exportTextureFiles(const model::Model& model);

// glTF 2.0 (`binary` = .glb). Definițiile devin mesh-uri partajate de instanțe (copiile unui obiect rămân copii).
bool exportGltf(const model::Model& model, const std::filesystem::path& path, bool binary,
    const ExportOptions& options = {}, std::string* error = nullptr);

struct ImportOptions {
    double scale = 1000.0; // m → mm
    bool yUp = true; // (x, y, z) → (x, −z, y)
};

struct ImportResult {
    model::InstanceId instance; // grupul creat în `parent` (nul dacă importul a eșuat)
    std::size_t faces = 0;
    std::string error;
};

// Importă un OBJ ca grup în definiția `parent` (fețele coplanare vecine se unesc, curburile line devin netede).
ImportResult importObj(model::Model& model, model::DefinitionId parent, std::string_view text, std::string name,
    const ImportOptions& options = {});

// Importă un glTF sau GLB: nodurile devin grupuri, mesh-urile folosite de mai multe noduri devin obiecte.
ImportResult importGltf(model::Model& model, model::DefinitionId parent, const std::filesystem::path& path,
    const ImportOptions& options = {});

} // namespace aion::io
