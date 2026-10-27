#include "aion/core/model/Parametric.h"

#include "aion/core/geom/Primitives.h"
#include "aion/core/model/MaterialLibrary.h"

#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace aion::model {

namespace {

const std::vector<GeneratorInfo>& registry()
{
    static const std::vector<GeneratorInfo> kGenerators {
        { "door", "Ușă", "Uși",
            { { "width", "Lățime", "mm", 900, 500, 2000, 10 }, { "height", "Înălțime", "mm", 2100, 1500, 3000, 10 },
                { "wall", "Grosimea peretelui", "mm", 120, 60, 500, 10 }, { "frame", "Lățimea tocului", "mm", 60, 20, 150, 5 },
                { "leaf", "Grosimea foii", "mm", 40, 20, 80, 2 } } },
        { "window", "Fereastră", "Ferestre",
            { { "width", "Lățime", "mm", 1200, 300, 4000, 10 }, { "height", "Înălțime", "mm", 1400, 300, 3000, 10 },
                { "depth", "Adâncime", "mm", 80, 40, 300, 5 }, { "frame", "Lățimea ramei", "mm", 60, 20, 150, 5 },
                { "columns", "Canate", "buc.", 2, 1, 6, 1 } } },
        { "table", "Masă", "Mobilier",
            { { "width", "Lungime", "mm", 1600, 400, 4000, 10 }, { "depth", "Lățime", "mm", 900, 400, 2000, 10 },
                { "height", "Înălțime", "mm", 750, 300, 1200, 10 }, { "top", "Grosimea blatului", "mm", 30, 10, 100, 2 },
                { "leg", "Latura piciorului", "mm", 60, 20, 200, 5 } } },
        { "shelf", "Bibliotecă", "Mobilier",
            { { "width", "Lățime", "mm", 900, 300, 3000, 10 }, { "height", "Înălțime", "mm", 1800, 300, 3000, 10 },
                { "depth", "Adâncime", "mm", 300, 150, 800, 10 }, { "shelves", "Polițe", "buc.", 5, 1, 12, 1 },
                { "board", "Grosimea plăcii", "mm", 18, 10, 50, 1 } } },
        { "cabinet", "Dulap", "Mobilier",
            { { "width", "Lățime", "mm", 1000, 400, 3000, 10 }, { "height", "Înălțime", "mm", 2000, 500, 3000, 10 },
                { "depth", "Adâncime", "mm", 600, 300, 900, 10 }, { "doors", "Uși", "buc.", 2, 1, 6, 1 },
                { "board", "Grosimea plăcii", "mm", 18, 10, 50, 1 } } },
        { "stairs", "Scară dreaptă", "Scări",
            { { "width", "Lățime", "mm", 1000, 600, 3000, 10 }, { "height", "Înălțime totală", "mm", 2800, 300, 6000, 10 },
                { "run", "Lungime totală", "mm", 3600, 300, 8000, 10 }, { "steps", "Trepte", "buc.", 16, 2, 40, 1 } } },
    };
    return kGenerators;
}

// Un material al bibliotecii (după nume), adus în model dacă lipsește.
MaterialId libraryMaterial(Model& model, std::string_view name)
{
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        if (model.materials().values()[i].name == name) {
            return model.materials().keys()[i];
        }
    }
    for (const Material& m : builtinMaterials()) {
        if (m.name == name) {
            return model.addMaterial(m);
        }
    }
    return {};
}

// Constructorul părților: fiecare parte e un subgrup cu o cutie (colțurile `lo`, `hi`) și un material.
struct Parts {
    Model& model;
    DefinitionId parent;

    void box(const std::string& name, math::Vec3d lo, math::Vec3d hi, std::string_view material)
    {
        const math::Vec3d size = glm::abs(hi - lo);
        if (size.x < 0.5 || size.y < 0.5 || size.z < 0.5) {
            return;
        }
        Definition def { name, true };
        def.shape = "box";
        const DefinitionId d = model.addDefinition(def);
        geom::buildPrimitive(model.mesh(d), geom::Primitive::Box, size, 4);
        // buildPrimitive centrează baza în origine: o mutăm în colțul `lo`.
        const math::Vec3d corner = glm::min(lo, hi);
        Instance inst;
        inst.parent = parent;
        inst.definition = d;
        inst.transform = math::Transform::translation(corner + math::Vec3d { size.x / 2, size.y / 2, 0.0 });
        inst.name = name;
        inst.material = material.empty() ? MaterialId {} : libraryMaterial(model, material);
        model.addInstance(inst);
    }
};

void buildDoor(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), h = v.at("height"), wall = v.at("wall"), f = v.at("frame"), leaf = v.at("leaf");
    p.box("Toc stânga", { 0, 0, 0 }, { f, wall, h }, "Alb mat");
    p.box("Toc dreapta", { w - f, 0, 0 }, { w, wall, h }, "Alb mat");
    p.box("Toc sus", { f, 0, h - f }, { w - f, wall, h }, "Alb mat");
    const double y0 = (wall - leaf) / 2;
    p.box("Foaie", { f + 2, y0, 5 }, { w - f - 2, y0 + leaf, h - f - 2 }, "Lemn de stejar");
    // Clanța, pe ambele fețe, la 1 m.
    const double hx = w - f - 90;
    p.box("Clanță", { hx, y0 - 50, 990 }, { hx + 120, y0, 1010 }, "Oțel periat");
    p.box("Clanță (spate)", { hx, y0 + leaf, 990 }, { hx + 120, y0 + leaf + 50, 1010 }, "Oțel periat");
}

void buildWindow(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), h = v.at("height"), d = v.at("depth"), f = v.at("frame");
    const int columns = static_cast<int>(std::round(v.at("columns")));
    p.box("Ramă jos", { 0, 0, 0 }, { w, d, f }, "Alb mat");
    p.box("Ramă sus", { 0, 0, h - f }, { w, d, h }, "Alb mat");
    p.box("Ramă stânga", { 0, 0, f }, { f, d, h - f }, "Alb mat");
    p.box("Ramă dreapta", { w - f, 0, f }, { w, d, h - f }, "Alb mat");
    const double inner = (w - 2 * f - (columns - 1) * f) / columns;
    for (int c = 0; c < columns; ++c) {
        const double x0 = f + c * (inner + f);
        if (c > 0) {
            p.box("Montant " + std::to_string(c), { x0 - f, 0, f }, { x0, d, h - f }, "Alb mat");
        }
        p.box("Geam " + std::to_string(c + 1), { x0, d / 2 - 3, f }, { x0 + inner, d / 2 + 3, h - f }, "Sticlă");
    }
}

void buildTable(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), d = v.at("depth"), h = v.at("height"), t = v.at("top"), l = v.at("leg");
    p.box("Blat", { 0, 0, h - t }, { w, d, h }, "Lemn de stejar");
    const double inset = 40;
    const std::array<math::Vec2d, 4> corners { { { inset, inset }, { w - inset - l, inset }, { inset, d - inset - l },
        { w - inset - l, d - inset - l } } };
    for (std::size_t i = 0; i < corners.size(); ++i) {
        p.box("Picior " + std::to_string(i + 1), { corners[i].x, corners[i].y, 0 },
            { corners[i].x + l, corners[i].y + l, h - t }, "Lemn de stejar");
    }
}

void buildShelf(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), h = v.at("height"), d = v.at("depth"), b = v.at("board");
    const int shelves = static_cast<int>(std::round(v.at("shelves")));
    p.box("Lateral stânga", { 0, 0, 0 }, { b, d, h }, "Lemn de pin");
    p.box("Lateral dreapta", { w - b, 0, 0 }, { w, d, h }, "Lemn de pin");
    p.box("Bază", { b, 0, 0 }, { w - b, d, b }, "Lemn de pin");
    p.box("Capac", { b, 0, h - b }, { w - b, d, h }, "Lemn de pin");
    const double gap = (h - 2 * b - shelves * b) / (shelves + 1);
    for (int i = 1; i <= shelves; ++i) {
        const double z = b + i * gap + (i - 1) * b;
        p.box("Poliță " + std::to_string(i), { b, 0, z }, { w - b, d - 5, z + b }, "Lemn de pin");
    }
    p.box("Spate", { b, d - 5, b }, { w - b, d, h - b }, "Alb mat");
}

void buildCabinet(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), h = v.at("height"), d = v.at("depth"), b = v.at("board");
    const int doors = static_cast<int>(std::round(v.at("doors")));
    p.box("Lateral stânga", { 0, b, 0 }, { b, d, h }, "Alb mat");
    p.box("Lateral dreapta", { w - b, b, 0 }, { w, d, h }, "Alb mat");
    p.box("Bază", { b, b, 0 }, { w - b, d, b }, "Alb mat");
    p.box("Capac", { b, b, h - b }, { w - b, d, h }, "Alb mat");
    p.box("Spate", { b, d - 5, b }, { w - b, d, h - b }, "Alb mat");
    const double doorWidth = (w - (doors - 1) * 3.0) / doors;
    for (int i = 0; i < doors; ++i) {
        const double x0 = i * (doorWidth + 3.0);
        p.box("Ușă " + std::to_string(i + 1), { x0, 0, 0 }, { x0 + doorWidth, b, h }, "Lemn de nuc");
        const double hx = (i % 2 == 0) ? x0 + doorWidth - 50 : x0 + 30;
        p.box("Mâner " + std::to_string(i + 1), { hx, -25, h / 2 - 100 }, { hx + 20, 0, h / 2 + 100 }, "Oțel periat");
    }
}

void buildStairs(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), h = v.at("height"), run = v.at("run");
    const int steps = static_cast<int>(std::round(v.at("steps")));
    const double rise = h / steps;
    const double tread = run / steps;
    for (int i = 0; i < steps; ++i) {
        p.box("Treaptă " + std::to_string(i + 1), { 0, i * tread, 0 }, { w, (i + 1) * tread, (i + 1) * rise }, "Beton");
    }
}

using Builder = void (*)(Parts&, const std::map<std::string, double>&);

Builder builderFor(std::string_view id)
{
    if (id == "door") return buildDoor;
    if (id == "window") return buildWindow;
    if (id == "table") return buildTable;
    if (id == "shelf") return buildShelf;
    if (id == "cabinet") return buildCabinet;
    if (id == "stairs") return buildStairs;
    return nullptr;
}

// Șterge o instanță cu tot ce conține (și definițiile rămase fără instanțe).
void removeSubtree(Model& model, InstanceId id)
{
    const DefinitionId def = model.instance(id).definition;
    model.removeInstance(id);
    if (model.instancesOf(def).empty()) {
        for (InstanceId child : model.childrenOf(def)) {
            removeSubtree(model, child);
        }
        model.removeDefinition(def);
    }
}

} // namespace

std::span<const GeneratorInfo> generators()
{
    return registry();
}

const GeneratorInfo* findGenerator(std::string_view id)
{
    for (const GeneratorInfo& g : registry()) {
        if (g.id == id) {
            return &g;
        }
    }
    return nullptr;
}

std::map<std::string, double> resolvedParameters(const GeneratorInfo& generator, const std::map<std::string, double>& given)
{
    std::map<std::string, double> out;
    for (const ParameterSpec& spec : generator.parameters) {
        const auto it = given.find(spec.key);
        out[spec.key] = std::clamp(it != given.end() ? it->second : spec.value, spec.min, spec.max);
    }
    return out;
}

InstanceId createParametric(Model& model, DefinitionId context, std::string_view generator,
    const std::map<std::string, double>& parameters, const math::Transform& placement)
{
    const GeneratorInfo* info = findGenerator(generator);
    if (info == nullptr) {
        return {};
    }
    // O componentă (copiile rămân legate), nu un grup.
    Definition def { info->name, false };
    def.generator = info->id;
    def.parameters = resolvedParameters(*info, parameters);
    const DefinitionId d = model.addDefinition(def);
    regenerate(model, d);
    Instance inst;
    inst.parent = context;
    inst.definition = d;
    inst.transform = placement;
    inst.name = info->name;
    return model.addInstance(inst);
}

bool setParameter(Model& model, DefinitionId definition, const std::string& key, double value)
{
    Definition def = model.definition(definition);
    const GeneratorInfo* info = findGenerator(def.generator);
    if (info == nullptr) {
        return false;
    }
    std::map<std::string, double> given = def.parameters;
    given[key] = value;
    def.parameters = resolvedParameters(*info, given);
    model.setDefinition(definition, def);
    regenerate(model, definition);
    return true;
}

void regenerate(Model& model, DefinitionId definition)
{
    const Definition& def = model.definition(definition);
    const Builder build = builderFor(def.generator);
    const GeneratorInfo* info = findGenerator(def.generator);
    if (build == nullptr || info == nullptr) {
        return;
    }
    const std::map<std::string, double> parameters = resolvedParameters(*info, def.parameters);
    for (InstanceId child : model.childrenOf(definition)) {
        removeSubtree(model, child);
    }
    Parts parts { model, definition };
    build(parts, parameters);
}

} // namespace aion::model
