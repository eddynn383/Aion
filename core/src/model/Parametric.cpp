#include "aion/core/model/Parametric.h"

#include "aion/core/geom/Primitives.h"
#include "aion/core/model/Carcass.h"
#include "aion/core/model/Furniture.h"
#include "aion/core/model/MaterialLibrary.h"

#include <glm/common.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace aion::model {

namespace {

// Câmpurile unui corp (docs/plans/mobilier-pe-corp.md, §4): dimensiunile, ușile, polițele pe spațiu,
// separatoarele, grosimea corpului (PAL) și a spatelui (HDF).
std::vector<ParameterSpec> carcass(double w, double wMin, double wMax, double h, double hMin, double hMax,
    double d, double dMin, double dMax, double doors, double doorsMax, double shelves, double shelvesMax,
    double dividers, double dividersMax)
{
    std::vector<ParameterSpec> specs { { "width", "Lățime", "mm", w, wMin, wMax, 1 },
        { "height", "Înălțime", "mm", h, hMin, hMax, 1 }, { "depth", "Adâncime", "mm", d, dMin, dMax, 1 } };
    if (doorsMax > 0) {
        specs.push_back({ "doors", "Uși", "buc.", doors, 0, doorsMax, 1 });
    }
    specs.push_back({ "shelves", "Polițe (pe spațiu)", "buc.", shelves, 0, shelvesMax, 1 });
    if (dividersMax > 0) {
        specs.push_back({ "dividers", "Separatoare", "buc.", dividers, 0, dividersMax, 1 });
    }
    specs.push_back({ "board", "Grosimea corpului", "mm", 18, 16, 25, 1 });
    specs.push_back({ "back", "Grosimea spatelui", "mm", 3, 3, 4, 1 });
    return specs;
}

std::vector<ParameterSpec> appliance()
{
    std::vector<ParameterSpec> specs
        = carcass(600, 450, 900, 2100, 1200, 2600, 560, 450, 700, 0, 0, 1, 4, 0, 0);
    specs.insert(specs.end() - 2, { "nicheBottom", "Nișa de la", "mm", 850, 0, 1500, 1 });
    specs.insert(specs.end() - 2, { "nicheHeight", "Înălțimea nișei", "mm", 600, 300, 900, 1 });
    return specs;
}

const std::vector<GeneratorInfo>& registry()
{
    static const std::vector<GeneratorInfo> kGenerators {
        { "door", "Ușă", "Uși",
            { { "width", "Lățime", "mm", 900, 500, 2000, 10 },
                { "height", "Înălțime", "mm", 2100, 1500, 3000, 10 },
                { "wall", "Grosimea peretelui", "mm", 120, 60, 500, 10 },
                { "frame", "Lățimea tocului", "mm", 60, 20, 150, 5 },
                { "leaf", "Grosimea foii", "mm", 40, 20, 80, 2 } } },
        { "window", "Fereastră", "Ferestre",
            { { "width", "Lățime", "mm", 1200, 300, 4000, 10 },
                { "height", "Înălțime", "mm", 1400, 300, 3000, 10 },
                { "depth", "Adâncime", "mm", 80, 40, 300, 5 },
                { "frame", "Lățimea ramei", "mm", 60, 20, 150, 5 },
                { "columns", "Canate", "buc.", 2, 1, 6, 1 } } },
        { "table", "Masă", "Mobilier",
            { { "width", "Lungime", "mm", 1600, 400, 4000, 10 },
                { "depth", "Lățime", "mm", 900, 400, 2000, 10 },
                { "height", "Înălțime", "mm", 750, 300, 1200, 10 },
                { "top", "Grosimea blatului", "mm", 30, 10, 100, 2 },
                { "leg", "Latura piciorului", "mm", 60, 20, 200, 5 } } },
        { "shelf", "Bibliotecă", "Mobilier",
            { { "width", "Lățime", "mm", 900, 300, 3000, 10 },
                { "height", "Înălțime", "mm", 1800, 300, 3000, 10 },
                { "depth", "Adâncime", "mm", 300, 150, 800, 10 },
                { "shelves", "Polițe", "buc.", 5, 1, 12, 1 },
                { "board", "Grosimea plăcii", "mm", 18, 10, 50, 1 } },
            true },
        { "cabinet", "Dulap", "Mobilier",
            { { "width", "Lățime", "mm", 1000, 400, 3000, 10 },
                { "height", "Înălțime", "mm", 2000, 500, 3000, 10 },
                { "depth", "Adâncime", "mm", 600, 300, 900, 10 }, { "doors", "Uși", "buc.", 2, 1, 6, 1 },
                { "board", "Grosimea plăcii", "mm", 18, 10, 50, 1 } },
            true },
        { "base", "Corp la sol", "Corpuri",
            carcass(600, 150, 1200, 720, 300, 1000, 560, 300, 700, 1, 4, 1, 6, 0, 4) },
        { "wall", "Corp suspendat", "Corpuri",
            carcass(600, 150, 1200, 720, 300, 1000, 320, 200, 450, 1, 4, 1, 6, 0, 4) },
        { "tall", "Soldat", "Corpuri",
            carcass(600, 300, 900, 2100, 1200, 2600, 560, 300, 700, 1, 2, 4, 10, 0, 2) },
        { "appliance", "Corp pentru electrocasnice", "Corpuri", appliance() },
        { "wardrobe", "Dulap cu uși", "Corpuri",
            carcass(1000, 400, 3000, 2000, 500, 2700, 600, 300, 700, 2, 6, 0, 8, 0, 5) },
        { "rack", "Raft", "Corpuri",
            carcass(800, 300, 2000, 1800, 300, 2700, 300, 150, 600, 0, 4, 4, 12, 0, 5) },
        { "stairs", "Scară dreaptă", "Scări",
            { { "width", "Lățime", "mm", 1000, 600, 3000, 10 },
                { "height", "Înălțime totală", "mm", 2800, 300, 6000, 10 },
                { "run", "Lungime totală", "mm", 3600, 300, 8000, 10 },
                { "steps", "Trepte", "buc.", 16, 2, 40, 1 } } },
        { "tree", "Copac", "Plante",
            { { "height", "Înălțime", "mm", 5000, 1000, 20000, 100 },
                { "crown", "Diametrul coroanei", "mm", 3000, 500, 12000, 100 },
                { "trunk", "Diametrul trunchiului", "mm", 300, 50, 1500, 10 } } },
        { "conifer", "Brad", "Plante",
            { { "height", "Înălțime", "mm", 6000, 1000, 30000, 100 },
                { "width", "Lățime la bază", "mm", 2500, 500, 10000, 100 },
                { "tiers", "Etaje", "buc.", 3, 1, 6, 1 } } },
        { "bush", "Tufă", "Plante",
            { { "width", "Lățime", "mm", 1200, 200, 5000, 50 },
                { "height", "Înălțime", "mm", 900, 200, 4000, 50 } } },
        { "person", "Om", "Oameni", { { "height", "Înălțime", "mm", 1750, 900, 2200, 10 } } },
    };
    return kGenerators;
}

// Un material al bibliotecii (după nume), adus în model dacă lipsește; cel din model se recunoaște după cheie
// (ADR 0044), iar pentru fișierele vechi, fără cheie, după nume.
MaterialId libraryMaterial(Model& model, std::string_view name)
{
    const auto library = builtinMaterials();
    const auto lib
        = std::find_if(library.begin(), library.end(), [&](const Material& m) { return m.name == name; });
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        const Material& mine = model.materials().values()[i];
        if (lib != library.end()
                ? mine.libraryKey == lib->libraryKey || (mine.libraryKey.empty() && mine.name == name)
                : mine.name == name) {
            return model.materials().keys()[i];
        }
    }
    return lib != library.end() ? model.addMaterial(*lib) : MaterialId {};
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

    // Un cilindru, con sau sferă cu cutia începând în `corner` (diametrul `diameter`, înălțimea `height`),
    // opțional turtit / alungit pe Z (scara Z), ca subgrup.
    void round(const std::string& name, geom::Primitive kind, math::Vec3d corner, double diameter,
        double height, std::string_view material)
    {
        if (diameter < 0.5 || height < 0.5) {
            return;
        }
        Definition def { name, true };
        def.shape = kind == geom::Primitive::Sphere ? "sphere"
            : kind == geom::Primitive::Cone         ? "cone"
                                                    : "cylinder";
        const DefinitionId d = model.addDefinition(def);
        // Sfera are diametrul X pe toate axele: o scalăm pe Z până la înălțimea cerută.
        const double built = kind == geom::Primitive::Sphere ? diameter : height;
        geom::buildPrimitive(model.mesh(d), kind, { diameter, diameter, built }, 24);
        Instance inst;
        inst.parent = parent;
        inst.definition = d;
        inst.transform
            = math::Transform::translation(corner + math::Vec3d { diameter / 2, diameter / 2, 0.0 })
            * math::Transform::scaling(math::Vec3d { 0.0 }, { 1.0, 1.0, height / built });
        inst.name = name;
        inst.material = material.empty() ? MaterialId {} : libraryMaterial(model, material);
        model.addInstance(inst);
    }
};

void buildDoor(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), h = v.at("height"), wall = v.at("wall"), f = v.at("frame"),
                 leaf = v.at("leaf");
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
        p.box("Geam " + std::to_string(c + 1), { x0, d / 2 - 3, f }, { x0 + inner, d / 2 + 3, h - f },
            "Sticlă");
    }
}

void buildTable(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), d = v.at("depth"), h = v.at("height"), t = v.at("top"), l = v.at("leg");
    p.box("Blat", { 0, 0, h - t }, { w, d, h }, "Lemn de stejar");
    const double inset = 40;
    const std::array<math::Vec2d, 4> corners { { { inset, inset }, { w - inset - l, inset },
        { inset, d - inset - l }, { w - inset - l, d - inset - l } } };
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
        p.box("Mâner " + std::to_string(i + 1), { hx, -25, h / 2 - 100 }, { hx + 20, 0, h / 2 + 100 },
            "Oțel periat");
    }
}

void buildStairs(Parts& p, const std::map<std::string, double>& v)
{
    const double w = v.at("width"), h = v.at("height"), run = v.at("run");
    const int steps = static_cast<int>(std::round(v.at("steps")));
    const double rise = h / steps;
    const double tread = run / steps;
    for (int i = 0; i < steps; ++i) {
        p.box("Treaptă " + std::to_string(i + 1), { 0, i * tread, 0 }, { w, (i + 1) * tread, (i + 1) * rise },
            "Beton");
    }
}

void buildTree(Parts& p, const std::map<std::string, double>& v)
{
    const double h = v.at("height"), crown = v.at("crown"), trunk = std::min(v.at("trunk"), crown * 0.5);
    const double crownHeight = std::min(crown, h * 0.7);
    const double c = crown / 2;
    p.round("Trunchi", geom::Primitive::Cylinder, { c - trunk / 2, c - trunk / 2, 0 }, trunk,
        h - crownHeight * 0.6, "Lemn de nuc");
    p.round("Coroană", geom::Primitive::Sphere, { 0, 0, h - crownHeight }, crown, crownHeight, "Iarbă");
}

void buildConifer(Parts& p, const std::map<std::string, double>& v)
{
    const double h = v.at("height"), w = v.at("width");
    const int tiers = static_cast<int>(std::round(v.at("tiers")));
    const double trunk = std::max(80.0, w * 0.08);
    const double c = w / 2;
    const double trunkHeight = h * 0.2;
    p.round("Trunchi", geom::Primitive::Cylinder, { c - trunk / 2, c - trunk / 2, 0 }, trunk,
        trunkHeight + 100, "Lemn de nuc");
    // Etajele: conuri tot mai mici, suprapuse.
    const double tierHeight = (h - trunkHeight) / (1.0 + 0.6 * (tiers - 1));
    for (int i = 0; i < tiers; ++i) {
        const double d = w * (1.0 - 0.25 * i);
        const double z = trunkHeight + i * tierHeight * 0.6;
        p.round("Etaj " + std::to_string(i + 1), geom::Primitive::Cone, { c - d / 2, c - d / 2, z }, d,
            tierHeight, "Verde brad");
    }
}

void buildBush(Parts& p, const std::map<std::string, double>& v)
{
    p.round("Tufă", geom::Primitive::Sphere, { 0, 0, 0 }, v.at("width"), v.at("height"), "Iarbă");
}

// Un om stilizat (proporții medii): picioare, trunchi, brațe, cap.
void buildPerson(Parts& p, const std::map<std::string, double>& v)
{
    const double k = v.at("height") / 1750.0;
    const auto s = [k](double mm) { return mm * k; };
    p.box("Picior stâng", { s(80), s(60), 0 }, { s(200), s(200), s(850) }, "Grafit");
    p.box("Picior drept", { s(240), s(60), 0 }, { s(360), s(200), s(850) }, "Grafit");
    p.box("Trunchi", { s(60), s(40), s(850) }, { s(380), s(220), s(1450) }, "Albastru");
    p.box("Braț stâng", { s(-10), s(80), s(820) }, { s(60), s(180), s(1430) }, "Albastru");
    p.box("Braț drept", { s(380), s(80), s(820) }, { s(450), s(180), s(1430) }, "Albastru");
    p.round("Cap", geom::Primitive::Sphere, { s(130), s(40), s(1480) }, s(180), s(230), "Teracotă");
}

using Builder = void (*)(Parts&, const std::map<std::string, double>&);

Builder builderFor(std::string_view id)
{
    if (id == "door")
        return buildDoor;
    if (id == "window")
        return buildWindow;
    if (id == "table")
        return buildTable;
    if (id == "shelf")
        return buildShelf;
    if (id == "cabinet")
        return buildCabinet;
    if (id == "stairs")
        return buildStairs;
    if (id == "tree")
        return buildTree;
    if (id == "conifer")
        return buildConifer;
    if (id == "bush")
        return buildBush;
    if (id == "person")
        return buildPerson;
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

std::map<std::string, double> resolvedParameters(
    const GeneratorInfo& generator, const std::map<std::string, double>& given)
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
    if (def.generator == kBoardGenerator) {
        return setBoardValue(model, definition, key, value); // placa ține pivotul pe loc (ADR 0044)
    }
    const GeneratorInfo* info = findGenerator(def.generator);
    if (info == nullptr) {
        return false;
    }
    if (isCarcassGenerator(def.generator)) {
        refreshDividerPositions(model, definition); // o mutare de mână a separatoarelor se păstrează
        def = model.definition(definition);
        if (key == "board" || key == "back") {
            value = nearestThickness(key == "board" ? "pal" : "hdf", value);
        }
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
    if (def.generator == kBoardGenerator) {
        rebuildBoard(model, definition); // geometria plăcii e în mesh-ul ei, nu în subgrupuri
        return;
    }
    if (isCarcassGenerator(def.generator)) {
        reconcileCarcass(model, definition); // piesele se recunosc după rol (ADR 0043)
        return;
    }
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
