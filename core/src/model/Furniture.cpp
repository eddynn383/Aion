#include "aion/core/model/Furniture.h"

#include "aion/core/geom/Primitives.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/MaterialLibrary.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace aion::model {

std::span<const BoardType> boardTypes()
{
    static const std::array<BoardType, 5> kTypes { {
        { "pal", "PAL", { 16.0, 18.0, 25.0 }, 18.0, 2800.0, 2070.0, true, true, Unit::SquareMeters },
        { "mdf", "MDF", { 16.0, 18.0, 19.0, 22.0 }, 18.0, 2800.0, 2070.0, false, true, Unit::SquareMeters },
        { "hdf", "HDF", { 3.0, 4.0 }, 3.0, 2800.0, 2070.0, false, true, Unit::SquareMeters },
        { "plywood", "Placaj", { 12.0, 15.0, 18.0 }, 18.0, 2500.0, 1250.0, true, true, Unit::SquareMeters },
        { "solid", "Lemn masiv", { 18.0, 20.0, 26.0, 40.0 }, 20.0, 0.0, 0.0, true, false, Unit::LinearMeters },
    } };
    return kTypes;
}

std::span<const BandType> bandTypes()
{
    static const std::array<BandType, 4> kTypes { {
        { "abs.0.4", "ABS 0,4 mm", 0.4, { "pal", "mdf", "plywood" }, false },
        { "abs.1", "ABS 1 mm", 1.0, { "pal", "mdf", "plywood" }, true },
        { "abs.2", "ABS 2 mm", 2.0, { "pal", "mdf", "plywood" }, true },
        { "melamine.0.4", "Melamină 0,4 mm", 0.4, { "pal" }, false },
    } };
    return kTypes;
}

std::span<const HardwareType> hardwareTypes()
{
    static const std::vector<HardwareType> kTypes = [] {
        std::vector<HardwareType> types;
        const std::array<std::pair<const char*, const char*>, 3> hinges { {
            { "overlay", "aplicată" },
            { "half", "semi-aplicată" },
            { "inset", "încastrată" },
        } };
        for (const auto& [kind, label] : hinges) {
            for (const int angle : { 110, 165 }) {
                types.push_back({ std::string("hinge.") + kind + "." + std::to_string(angle),
                    std::string("Balama ") + label + " " + std::to_string(angle) + "°", Unit::Piece, { 50.0, 35.0, 12.0 } });
            }
        }
        for (int length = 300; length <= 600; length += 50) {
            types.push_back({ "slide." + std::to_string(length), "Glisieră " + std::to_string(length) + " mm", Unit::Pair,
                { static_cast<double>(length), 12.0, 45.0 } });
        }
        types.push_back({ "handle.128", "Mâner 128 mm", Unit::Piece, { 20.0, 30.0, 140.0 } });
        return types;
    }();
    return kTypes;
}

namespace {

template <class T>
const T* findByKey(std::span<const T> items, std::string_view key)
{
    const auto it = std::find_if(items.begin(), items.end(), [&](const T& t) { return t.key == key; });
    return it == items.end() ? nullptr : &*it;
}

} // namespace

const BoardType* findBoardType(std::string_view key)
{
    return findByKey(boardTypes(), key);
}

const BandType* findBandType(std::string_view key)
{
    return findByKey(bandTypes(), key);
}

const HardwareType* findHardwareType(std::string_view key)
{
    return findByKey(hardwareTypes(), key);
}

bool bandFitsBoard(std::string_view bandKey, std::string_view boardTypeKey)
{
    const BandType* band = findBandType(bandKey);
    return band != nullptr
        && std::find(band->boardTypes.begin(), band->boardTypes.end(), boardTypeKey) != band->boardTypes.end();
}

double nearestThickness(std::string_view boardTypeKey, double value)
{
    const BoardType* type = findBoardType(boardTypeKey);
    if (type == nullptr || type->thicknesses.empty()) {
        return value;
    }
    double best = type->thicknesses.front();
    for (const double t : type->thicknesses) {
        const double d = std::abs(t - value);
        const double bestD = std::abs(best - value);
        if (d < bestD - 1e-9 || (std::abs(d - bestD) <= 1e-9 && t > best)) {
            best = t;
        }
    }
    return best;
}

InstanceId createHardware(Model& model, DefinitionId context, std::string_view typeKey, const math::Transform& placement)
{
    const HardwareType* type = findHardwareType(typeKey);
    if (type == nullptr) {
        return {};
    }
    Definition def { type->label, false };
    def.shape = "box";
    PartSpec part;
    part.kind = PartSpec::Kind::Hardware;
    part.typeKey = type->key;
    def.part = part;
    const DefinitionId d = model.addDefinition(def);
    geom::buildPrimitive(model.mesh(d), geom::Primitive::Box, type->size, 4);
    Instance inst;
    inst.parent = context;
    inst.definition = d;
    inst.transform = placement;
    inst.nameKey = "hardware." + type->key;
    if (const auto steel = libraryMaterialByKey("otel-periat")) {
        // Feroneria se vede metalică; materialul intră în model o singură dată (unirea după cheie).
        MaterialId material;
        for (std::size_t i = 0; i < model.materials().size(); ++i) {
            if (model.materials().values()[i].libraryKey == "otel-periat") {
                material = model.materials().keys()[i];
            }
        }
        inst.material = material ? material : model.addMaterial(builtinMaterials()[*steel]);
    }
    return model.addInstance(inst);
}

namespace {

struct LocalBox {
    math::Vec3d lo { std::numeric_limits<double>::infinity() };
    math::Vec3d hi { -std::numeric_limits<double>::infinity() };
    [[nodiscard]] bool empty() const { return lo.x > hi.x; }
};

LocalBox localBox(const geom::Mesh& mesh)
{
    LocalBox box;
    for (const geom::Vertex& v : mesh.vertices().values()) {
        box.lo = glm::min(box.lo, v.position);
        box.hi = glm::max(box.hi, v.position);
    }
    return box;
}

bool onBoundary(const LocalBox& box, const math::Vec3d& p)
{
    for (int a = 0; a < 3; ++a) {
        if (tol::equal(p[a], box.lo[a]) || tol::equal(p[a], box.hi[a])) {
            return true;
        }
    }
    return false;
}

// Regula din ADR 0017 pentru piesele fără date: fața cea mai mare dă normala (grosimea), cea mai lungă muchie a ei
// dă direcția lungimii; lungimea e latura mai lungă.
PartMeasure measureByLargestFace(const geom::Mesh& mesh, const math::Transform& t)
{
    PartMeasure m;
    const geom::Face* main = nullptr;
    double mainArea = -1.0;
    for (const geom::Face& face : mesh.faces().values()) {
        std::vector<math::Vec3d> ring;
        for (geom::VertexId v : mesh.loopVertices(face.outer)) {
            ring.push_back(t.applyToPoint(mesh.position(v)));
        }
        math::Vec3d n { 0.0 };
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const math::Vec3d& a = ring[i];
            const math::Vec3d& b = ring[(i + 1) % ring.size()];
            n += glm::cross(a, b);
        }
        const double area = glm::length(n) * 0.5;
        if (area > mainArea) {
            mainArea = area;
            main = &face;
        }
    }
    if (main == nullptr) {
        return m;
    }
    std::vector<math::Vec3d> ring;
    for (geom::VertexId v : mesh.loopVertices(main->outer)) {
        ring.push_back(t.applyToPoint(mesh.position(v)));
    }
    const math::Vec3d n
        = glm::normalize(glm::transpose(glm::inverse(glm::dmat3(t.matrix()))) * main->plane.normal);
    math::Vec3d u { 1.0, 0.0, 0.0 };
    double longest = -1.0;
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const math::Vec3d d = ring[(i + 1) % ring.size()] - ring[i];
        if (glm::length(d) > longest) {
            longest = glm::length(d);
            u = glm::normalize(d);
        }
    }
    const math::Vec3d v = glm::normalize(glm::cross(n, u));
    double minU = std::numeric_limits<double>::infinity(), maxU = -minU;
    double minV = minU, maxV = -minU, minN = minU, maxN = -minU;
    for (const geom::Vertex& vertex : mesh.vertices().values()) {
        const math::Vec3d p = t.applyToPoint(vertex.position);
        minU = std::min(minU, glm::dot(p, u));
        maxU = std::max(maxU, glm::dot(p, u));
        minV = std::min(minV, glm::dot(p, v));
        maxV = std::max(maxV, glm::dot(p, v));
        minN = std::min(minN, glm::dot(p, n));
        maxN = std::max(maxN, glm::dot(p, n));
    }
    m.length = std::max(maxU - minU, maxV - minV);
    m.width = std::min(maxU - minU, maxV - minV);
    m.thickness = maxN - minN;
    return m;
}

} // namespace

bool isSimpleBox(const geom::Mesh& mesh)
{
    if (mesh.faces().size() != 6 || mesh.vertices().size() != 8) {
        return false;
    }
    const LocalBox box = localBox(mesh);
    for (const geom::Vertex& v : mesh.vertices().values()) {
        for (int a = 0; a < 3; ++a) {
            if (!tol::equal(v.position[a], box.lo[a]) && !tol::equal(v.position[a], box.hi[a])) {
                return false;
            }
        }
    }
    return true;
}

PartMeasure measurePart(const Model& model, DefinitionId definition, const math::Transform& toWorld)
{
    const geom::Mesh& mesh = model.mesh(definition);
    const Definition& def = model.definition(definition);
    if (!def.part || def.part->kind != PartSpec::Kind::Board) {
        PartMeasure m = measureByLargestFace(mesh, toWorld);
        m.box = isSimpleBox(mesh);
        return m;
    }
    PartMeasure m;
    const LocalBox box = localBox(mesh);
    if (box.empty()) {
        return m;
    }
    math::Vec3d extent = box.hi - box.lo;
    for (int a = 0; a < 3; ++a) {
        extent[a] *= glm::length(math::Vec3d(toWorld.matrix()[a]));
    }
    const int thick = std::clamp<int>(def.part->thicknessAxis, 0, 2);
    int grain = def.part->grainAxis;
    const int first = thick == 0 ? 1 : 0;
    const int second = thick == 2 ? 1 : 2;
    if (grain == PartSpec::kNoGrain || grain == thick || grain > 2) {
        grain = extent[first] >= extent[second] ? first : second;
    }
    const int other = grain == first ? second : first;
    m.thickness = extent[thick];
    m.length = extent[grain];
    m.width = extent[other];
    m.lengthAxis = grain;
    m.widthAxis = other;
    m.box = isSimpleBox(mesh);
    for (const geom::Vertex& v : mesh.vertices().values()) {
        if (!onBoundary(box, v.position)) {
            m.extraShells = true;
            break;
        }
    }
    return m;
}

std::array<int, 4> bandSlots(const PartSpec& part, const PartMeasure& measure)
{
    const int thick = std::clamp<int>(part.thicknessAxis, 0, 2);
    const int u = thick == 0 ? 1 : 0;
    if (measure.lengthAxis == u) {
        return { 0, 1, 2, 3 }; // laturile lungi sunt la −V / +V
    }
    return { 2, 3, 0, 1 };
}

bool makeBoard(Model& model, InstanceId instance, std::string_view boardTypeKey)
{
    const DefinitionId d = model.instance(instance).definition;
    const geom::Mesh& mesh = model.mesh(d);
    if (mesh.faces().empty() || !model.childrenOf(d).empty() || findBoardType(boardTypeKey) == nullptr) {
        return false;
    }
    const LocalBox box = localBox(mesh);
    const math::Vec3d extent = box.hi - box.lo;
    int thick = 0;
    for (int a = 1; a < 3; ++a) {
        if (extent[a] < extent[thick]) {
            thick = a;
        }
    }
    const int first = thick == 0 ? 1 : 0;
    const int second = thick == 2 ? 1 : 2;
    const int grain = extent[first] >= extent[second] ? first : second;
    Definition def = model.definition(d);
    PartSpec part = def.part && def.part->kind == PartSpec::Kind::Board ? *def.part : PartSpec {};
    part.kind = PartSpec::Kind::Board;
    part.typeKey = std::string(boardTypeKey);
    part.nominalThickness = nearestThickness(boardTypeKey, extent[thick]);
    part.thicknessAxis = static_cast<std::uint8_t>(thick);
    part.grainAxis = findBoardType(boardTypeKey)->grain ? static_cast<std::uint8_t>(grain) : PartSpec::kNoGrain;
    def.part = part;
    model.setDefinition(d, def);
    return true;
}

void backfillMaterials(Model& model)
{
    const auto library = builtinMaterials();
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        Material m = model.materials().values()[i];
        if (!m.category.empty() || !m.libraryKey.empty()) {
            continue;
        }
        const auto it = std::find_if(library.begin(), library.end(), [&](const Material& l) { return l.name == m.name; });
        if (it != library.end()) {
            m.category = it->category;
            m.libraryKey = it->libraryKey;
            model.setMaterial(model.materials().keys()[i], m);
        }
    }
}

} // namespace aion::model
