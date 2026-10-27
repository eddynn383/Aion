#pragma once

#include "aion/core/math/Transform.h"
#include "aion/core/model/Model.h"

#include <map>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Obiecte dinamice (parametrice): uși, ferestre, mese, rafturi, scări, dulapuri. Definiția păstrează generatorul și
// parametrii; părțile (toc, foaie, blat, picioare…) sunt subgrupuri refăcute la fiecare schimbare. Copiile legate se
// schimbă împreună. Originea fiecărui obiect e colțul din stânga-față-jos; X = lățime, Y = adâncime, Z = înălțime.
// Vezi ADR 0025.
namespace aion::model {

struct ParameterSpec {
    std::string key;
    std::string label;
    std::string unit; // „mm”, „buc.”
    double value = 0.0; // implicit
    double min = 0.0;
    double max = 0.0;
    double step = 1.0;
};

struct GeneratorInfo {
    std::string id;
    std::string name;
    std::string category;
    std::vector<ParameterSpec> parameters;
};

[[nodiscard]] std::span<const GeneratorInfo> generators();
[[nodiscard]] const GeneratorInfo* findGenerator(std::string_view id);

// Parametrii impliciți ai generatorului, peste care se pun cei dați (limitați la min / max).
[[nodiscard]] std::map<std::string, double> resolvedParameters(
    const GeneratorInfo& generator, const std::map<std::string, double>& given);

// Un obiect dinamic nou în `context`, cu transformarea dată. Întoarce instanța (nulă dacă generatorul nu există).
InstanceId createParametric(Model& model, DefinitionId context, std::string_view generator,
    const std::map<std::string, double>& parameters, const math::Transform& placement);

// Schimbă un parametru (limitat la min / max) și reface obiectul. Întoarce false dacă definiția nu e parametrică.
bool setParameter(Model& model, DefinitionId definition, const std::string& key, double value);

// Reface părțile definiției din generator și parametri.
void regenerate(Model& model, DefinitionId definition);

} // namespace aion::model
