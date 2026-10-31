#include "aion/core/model/Furniture.h"

#include "aion/core/geom/Bezier.h"
#include "aion/core/geom/Operations.h"
#include "aion/core/geom/Primitives.h"
#include "aion/core/math/Tolerance.h"
#include "aion/core/model/MaterialLibrary.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>

namespace aion::model {

std::span<const BoardType> boardTypes()
{
    static const std::array<BoardType, 5> kTypes { {
        { "pal", "PAL", { 16.0, 18.0, 25.0 }, 18.0, 2800.0, 2070.0, true, true, Unit::SquareMeters },
        { "mdf", "MDF", { 16.0, 18.0, 19.0, 22.0 }, 18.0, 2800.0, 2070.0, false, true, Unit::SquareMeters },
        { "hdf", "HDF", { 3.0, 4.0 }, 3.0, 2800.0, 2070.0, false, true, Unit::SquareMeters },
        { "plywood", "Placaj", { 12.0, 15.0, 18.0 }, 18.0, 2500.0, 1250.0, true, true, Unit::SquareMeters },
        { "solid", "Lemn masiv", { 18.0, 20.0, 26.0, 40.0 }, 20.0, 0.0, 0.0, true, false,
            Unit::LinearMeters },
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
                    std::string("Balama ") + label + " " + std::to_string(angle) + "°", Unit::Piece,
                    { 50.0, 12.0, 35.0 } }); // pe spatele ușii: lată 50, groasă 12, înaltă 35 (cupa)
            }
        }
        for (int length = 300; length <= 600; length += 50) {
            types.push_back({ "slide." + std::to_string(length), "Glisieră " + std::to_string(length) + " mm",
                Unit::Pair, { static_cast<double>(length), 12.0, 45.0 } });
        }
        types.push_back({ "handle.128", "Mâner 128 mm", Unit::Piece, { 20.0, 30.0, 140.0 } });
        return types;
    }();
    return kTypes;
}

namespace {

template <class T> const T* findByKey(std::span<const T> items, std::string_view key)
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
        && std::find(band->boardTypes.begin(), band->boardTypes.end(), boardTypeKey)
        != band->boardTypes.end();
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

InstanceId createHardware(
    Model& model, DefinitionId context, std::string_view typeKey, const math::Transform& placement)
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

bool replaceHardwareVariant(Model& model, DefinitionId definition, std::string_view typeKey)
{
    const HardwareType* type = findHardwareType(typeKey);
    Definition def = model.definition(definition);
    if (type == nullptr || !def.part || def.part->kind != PartSpec::Kind::Hardware) {
        return false;
    }
    if (def.part->typeKey == type->key) {
        return true;
    }
    def.part->typeKey = type->key;
    def.name = type->label;
    model.setDefinition(definition, def);
    geom::Mesh& mesh = model.mesh(definition);
    for (geom::FaceId f : std::vector(mesh.faces().keys())) {
        mesh.removeFace(f);
    }
    for (geom::EdgeId e : std::vector(mesh.edges().keys())) {
        mesh.removeEdge(e);
    }
    for (geom::VertexId v : std::vector(mesh.vertices().keys())) {
        mesh.removeVertex(v);
    }
    geom::buildPrimitive(mesh, geom::Primitive::Box, type->size, 4);
    for (InstanceId id : model.instancesOf(definition)) {
        Instance inst = model.instance(id);
        if (inst.nameKey.starts_with("hardware.")) {
            inst.nameKey = "hardware." + type->key;
            model.setInstance(id, inst);
        }
    }
    return true;
}

std::string_view hardwareFamily(std::string_view typeKey)
{
    return typeKey.substr(0, typeKey.find('.'));
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

// Regula din ADR 0017 pentru piesele fără date: fața cea mai mare dă normala (grosimea), cea mai lungă muchie
// a ei dă direcția lungimii; lungimea e latura mai lungă.
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

std::string_view bandSideName(int side)
{
    static constexpr std::array<std::string_view, 4> kNames { "L1", "L2", "W1", "W2" };
    return side >= 0 && side < 4 ? kNames[static_cast<std::size_t>(side)] : std::string_view {};
}

namespace {

// Indicele din `PartSpec::bands` al laturii L1…W2 a plăcii (măsurată pe geometria ei, fără transformare).
int bandSlot(const Model& model, DefinitionId definition, int side)
{
    const PartSpec& part = *model.definition(definition).part;
    return bandSlots(
        part, measurePart(model, definition, math::Transform {}))[static_cast<std::size_t>(side)];
}

bool isBoardPart(const Model& model, DefinitionId definition)
{
    const Definition& def = model.definition(definition);
    return def.part && def.part->kind == PartSpec::Kind::Board;
}

} // namespace

bool setBand(Model& model, DefinitionId definition, int side, std::string_view bandKey,
    std::string_view decor, bool rounded)
{
    if (side < 0 || side > 3 || !isBoardPart(model, definition)) {
        return false;
    }
    Definition def = model.definition(definition);
    if (!bandKey.empty() && !bandFitsBoard(bandKey, def.part->typeKey)) {
        return false;
    }
    const int slot = bandSlot(model, definition, side);
    BandSide& target = def.part->bands[static_cast<std::size_t>(slot)];
    const BandSide next { std::string(bandKey), bandKey.empty() ? std::string() : std::string(decor),
        !bandKey.empty() && rounded };
    if (target == next) {
        return true;
    }
    target = next;
    model.setDefinition(definition, def);
    return true;
}

std::array<BandSide, 4> boardBands(const Model& model, DefinitionId definition)
{
    std::array<BandSide, 4> out {};
    if (!isBoardPart(model, definition)) {
        return out;
    }
    const PartSpec& part = *model.definition(definition).part;
    for (int k = 0; k < 4; ++k) {
        out[static_cast<std::size_t>(k)]
            = part.bands[static_cast<std::size_t>(bandSlot(model, definition, k))];
    }
    return out;
}

std::span<const BandPreset> builtinBandPresets()
{
    static const std::array<BandPreset, 3> kPresets { {
        { "Fără", {} },
        { "Toate 2 mm",
            { { { "abs.2", "", false }, { "abs.2", "", false }, { "abs.2", "", false },
                { "abs.2", "", false } } } },
        { "L1 2 mm, restul 0,4 mm",
            { { { "abs.2", "", false }, { "abs.0.4", "", false }, { "abs.0.4", "", false },
                { "abs.0.4", "", false } } } },
    } };
    return kPresets;
}

int applyBandPreset(Model& model, DefinitionId definition, const std::array<BandSide, 4>& sides)
{
    if (!isBoardPart(model, definition)) {
        return 0;
    }
    int skipped = 0;
    for (int k = 0; k < 4; ++k) {
        const BandSide& side = sides[static_cast<std::size_t>(k)];
        if (!setBand(model, definition, k, side.bandKey, side.decor, side.rounded)) {
            setBand(model, definition, k, {}, {});
            ++skipped;
        }
    }
    return skipped;
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
    part.grainAxis
        = findBoardType(boardTypeKey)->grain ? static_cast<std::uint8_t>(grain) : PartSpec::kNoGrain;
    def.part = part;
    model.setDefinition(d, def);
    return true;
}

namespace {

void clearMesh(geom::Mesh& mesh)
{
    for (geom::FaceId f : std::vector(mesh.faces().keys())) {
        mesh.removeFace(f);
    }
    for (geom::EdgeId e : std::vector(mesh.edges().keys())) {
        mesh.removeEdge(e);
    }
    for (geom::VertexId v : std::vector(mesh.vertices().keys())) {
        mesh.removeVertex(v);
    }
}

// Cutia L × W × T cu colțul minim în origine.
void buildBox(geom::Mesh& mesh, const math::Vec3d& size)
{
    geom::buildPrimitive(mesh, geom::Primitive::Box, size, 4);
    std::set<geom::VertexId> all(mesh.vertices().keys().begin(), mesh.vertices().keys().end());
    geom::transformVertices(mesh, all, math::Transform::translation({ size.x / 2, size.y / 2, 0.0 }));
}

bool isBoard(const Definition& def)
{
    return def.generator == kBoardGenerator && def.part && def.part->kind == PartSpec::Kind::Board;
}

math::Vec3d boardSize(const Definition& def)
{
    const auto get = [&](const char* key, double fallback) {
        const auto it = def.parameters.find(key);
        return it != def.parameters.end() ? it->second : fallback;
    };
    return { get("length", 600.0), get("width", 400.0), def.part ? def.part->nominalThickness : 18.0 };
}

// Pivotul local al unei instanțe: cel explicit sau centrul cutiei `size`.
math::Vec3d localPivot(const Instance& inst, const math::Vec3d& size)
{
    return inst.pivot ? *inst.pivot : size * 0.5;
}

math::Vec3d column(const math::Transform& t, int i)
{
    return math::Vec3d(t.matrix()[i]);
}

} // namespace

InstanceId createBoard(Model& model, DefinitionId context, std::string_view typeKey, double thickness,
    double length, double width, const math::Transform& placement)
{
    const BoardType* type = findBoardType(typeKey);
    if (type == nullptr) {
        return {};
    }
    Definition def { type->label, false };
    def.shape = "box";
    def.generator = kBoardGenerator;
    def.parameters = { { "length", std::max(1.0, length) }, { "width", std::max(1.0, width) } };
    PartSpec part;
    part.typeKey = type->key;
    part.nominalThickness = nearestThickness(type->key, thickness);
    part.thicknessAxis = 2;
    part.grainAxis = 0;
    def.part = part;
    const DefinitionId d = model.addDefinition(def);
    rebuildBoard(model, d);
    Instance inst;
    inst.parent = context;
    inst.definition = d;
    inst.transform = placement;
    inst.nameKey = "part.board";
    return model.addInstance(inst);
}

void rebuildBoard(Model& model, DefinitionId definition)
{
    const Definition& def = model.definition(definition);
    if (!isBoard(def)) {
        return;
    }
    geom::Mesh& mesh = model.mesh(definition);
    clearMesh(mesh);
    buildBox(mesh, boardSize(def));
}

BoardStatus boardState(const Model& model, InstanceId instance, const math::Transform& toWorld)
{
    const Definition& def = model.definition(model.instance(instance).definition);
    BoardStatus status;
    if (!def.part || def.part->kind != PartSpec::Kind::Board) {
        return status;
    }
    if (def.generator != kBoardGenerator) {
        status.state = BoardState::Detached;
        return status;
    }
    status.state = BoardState::Modified;
    const DefinitionId d = model.instance(instance).definition;
    if (!model.childrenOf(d).empty()) {
        status.reason = BoardReason::Children;
        return status;
    }
    const geom::Mesh& mesh = model.mesh(d);
    const math::Vec3d size = boardSize(def);
    constexpr double kTol = 0.01;
    if (!isSimpleBox(mesh)) {
        status.reason = BoardReason::Geometry;
        return status;
    }
    const LocalBox box = localBox(mesh);
    const math::Vec3d extent = box.hi - box.lo;
    if (std::abs(extent.z - size.z) > kTol) {
        status.reason = BoardReason::Thickness;
        return status;
    }
    if (glm::length(box.lo) > kTol || std::abs(extent.x - size.x) > kTol
        || std::abs(extent.y - size.y) > kTol) {
        status.reason = BoardReason::Geometry;
        return status;
    }
    const auto orthonormal = [](const math::Transform& t) {
        for (int i = 0; i < 3; ++i) {
            if (std::abs(glm::length(column(t, i)) - 1.0) > 1e-6) {
                return false;
            }
            for (int j = i + 1; j < 3; ++j) {
                if (std::abs(glm::dot(column(t, i), column(t, j))) > 1e-6) {
                    return false;
                }
            }
        }
        return true;
    };
    if (!orthonormal(model.instance(instance).transform)) {
        status.reason = BoardReason::Scaled;
        return status;
    }
    if (!orthonormal(toWorld)) {
        status.reason = BoardReason::ScaledParent;
        return status;
    }
    status.state = BoardState::Matches;
    return status;
}

void shiftBoardOrigin(Model& model, DefinitionId definition, const math::Vec3d& d)
{
    if (glm::length(d) <= tol::kLength) {
        return;
    }
    geom::Mesh& mesh = model.mesh(definition);
    std::set<geom::VertexId> all(mesh.vertices().keys().begin(), mesh.vertices().keys().end());
    geom::transformVertices(mesh, all, math::Transform::translation(-d));
    const math::Transform toOld = math::Transform::translation(d); // coordonatele noi → cele vechi
    const std::vector<InstanceId> users = model.instancesOf(definition);
    for (InstanceId id : users) {
        Instance inst = model.instance(id);
        inst.transform = inst.transform * toOld;
        if (inst.pivot) {
            inst.pivot = *inst.pivot - d; // scris direct: cheile de animație rămân valabile
        }
        model.setInstance(id, inst);
    }
    const std::set<InstanceId> userSet(users.begin(), users.end());
    for (std::size_t i = 0; i < model.scenes().size(); ++i) {
        Scene scene = model.scenes().values()[i];
        bool changed = false;
        for (auto& [id, t] : scene.transforms) {
            if (userSet.contains(id)) {
                t = t * toOld;
                changed = true;
            }
        }
        if (changed) {
            model.setScene(model.scenes().keys()[i], scene);
        }
    }
    for (std::size_t i = 0; i < model.motionPaths().size(); ++i) {
        MotionPath path = model.motionPaths().values()[i];
        bool changed = false;
        for (PathAnchor& a : path.anchors) {
            if (userSet.contains(a.instance)) {
                a.before = a.before * toOld;
                changed = true;
            }
        }
        if (changed) {
            model.setMotionPath(model.motionPaths().keys()[i], path);
        }
    }
    const math::Transform back = math::Transform::translation(-d);
    for (std::size_t i = 0; i < model.annotations().size(); ++i) {
        Annotation a = model.annotations().values()[i];
        if (a.context == definition) {
            a.a -= d;
            a.b -= d;
            model.setAnnotation(model.annotations().keys()[i], a);
        }
    }
    for (std::size_t i = 0; i < model.penCurves().size(); ++i) {
        PenCurve c = model.penCurves().values()[i];
        if (c.definition == definition) {
            c.path = geom::transformed(c.path, back);
            model.setPenCurve(model.penCurves().keys()[i], c);
        }
    }
    std::vector<std::pair<GuideId, Guide>> guides;
    for (std::size_t i = 0; i < model.guides().size(); ++i) {
        if (model.guides().values()[i].context == definition) {
            guides.emplace_back(model.guides().keys()[i], model.guides().values()[i]);
        }
    }
    for (auto& [id, g] : guides) {
        model.removeGuide(id);
        g.point -= d;
        model.addGuide(g);
    }
}

bool setBoardValue(Model& model, DefinitionId definition, std::string_view key, double value)
{
    Definition def = model.definition(definition);
    if (!isBoard(def)) {
        return false;
    }
    const math::Vec3d before = boardSize(def);
    if (key == "length" || key == "width") {
        def.parameters[std::string(key)] = std::max(1.0, value);
    } else if (key == "thickness") {
        def.part->nominalThickness = nearestThickness(def.part->typeKey, value);
    } else {
        return false;
    }
    const math::Vec3d after = boardSize(def);
    if (glm::length(after - before) <= tol::kLength) {
        return false;
    }
    model.setDefinition(definition, def);
    rebuildBoard(model, definition);
    // Pivotul fiecărei instanțe rămâne pe loc în lume; unul explicit își păstrează poziția relativă în cutie.
    for (InstanceId id : model.instancesOf(definition)) {
        Instance inst = model.instance(id);
        const math::Vec3d oldPivot = localPivot(inst, before);
        const math::Vec3d newPivot = inst.pivot ? oldPivot * (after / before) : after * 0.5;
        if (inst.pivot) {
            inst.pivot = newPivot;
        }
        const math::Vec3d world = inst.transform.applyToPoint(oldPivot);
        const math::Vec3d moved = inst.transform.applyToPoint(newPivot);
        inst.transform = math::Transform::translation(world - moved) * inst.transform;
        model.setInstance(id, inst);
    }
    return true;
}

bool setBoardType(Model& model, DefinitionId definition, std::string_view typeKey)
{
    Definition def = model.definition(definition);
    const BoardType* type = findBoardType(typeKey);
    if (!isBoard(def) || type == nullptr || def.part->typeKey == typeKey) {
        return false;
    }
    const double thickness = nearestThickness(typeKey, def.part->nominalThickness);
    def.part->typeKey = type->key;
    for (BandSide& side : def.part->bands) {
        if (!side.bandKey.empty() && !bandFitsBoard(side.bandKey, typeKey)) {
            side = {};
        }
    }
    def.name = type->label;
    model.setDefinition(definition, def);
    if (thickness != def.part->nominalThickness) {
        setBoardValue(model, definition, "thickness", thickness);
    }
    return true;
}

bool adoptBoardGeometry(Model& model, DefinitionId definition)
{
    const Definition& def = model.definition(definition);
    if (!isBoard(def) || !model.childrenOf(definition).empty()) {
        return false;
    }
    const geom::Mesh& mesh = model.mesh(definition);
    if (!isSimpleBox(mesh)) {
        return false;
    }
    const LocalBox box = localBox(mesh);
    const math::Vec3d extent = box.hi - box.lo;
    const math::Vec3d size = boardSize(def);
    if (std::abs(extent.z - size.z) > 0.01) {
        return false; // grosimea nu mai e cea nominală: placa rămâne „modificată manual”
    }
    if (std::abs(extent.x - size.x) <= tol::kLength && std::abs(extent.y - size.y) <= tol::kLength
        && glm::length(box.lo) <= tol::kLength) {
        return false; // deja în acord
    }
    Definition updated = def;
    updated.parameters["length"] = extent.x;
    updated.parameters["width"] = extent.y;
    model.setDefinition(definition, updated);
    shiftBoardOrigin(model, definition, box.lo);
    return true;
}

void finalizeBoards(Model& model, std::span<const DefinitionId> changed)
{
    for (DefinitionId d : changed) {
        if (model.definitions().contains(d) && model.definition(d).generator == kBoardGenerator) {
            adoptBoardGeometry(model, d);
        }
    }
}

void restoreBoard(Model& model, InstanceId instance)
{
    const DefinitionId d = model.instance(instance).definition;
    const Definition& def = model.definition(d);
    if (!isBoard(def)) {
        return;
    }
    Instance inst = model.instance(instance);
    const LocalBox box = localBox(model.mesh(d));
    const math::Vec3d oldSize = box.empty() ? boardSize(def) : box.hi - box.lo;
    const math::Vec3d oldPivot
        = inst.pivot ? *inst.pivot : (box.empty() ? oldSize * 0.5 : (box.lo + box.hi) * 0.5);
    const math::Vec3d world = inst.transform.applyToPoint(oldPivot);
    rebuildBoard(model, d);
    // |scara| → 1, cu semnul păstrat (o oglindire rămâne).
    math::Mat4d m = inst.transform.matrix();
    for (int i = 0; i < 3; ++i) {
        const double len = glm::length(math::Vec3d(m[i]));
        if (len > 1e-12) {
            m[i] = math::Mat4d::col_type(math::Vec3d(m[i]) / len, 0.0);
        }
    }
    inst.transform = math::Transform(m);
    const math::Vec3d newPivot = inst.pivot ? *inst.pivot : boardSize(def) * 0.5;
    inst.transform
        = math::Transform::translation(world - inst.transform.applyToPoint(newPivot)) * inst.transform;
    model.setInstance(instance, inst);
}

void detachBoard(Model& model, DefinitionId definition)
{
    Definition def = model.definition(definition);
    if (def.generator != kBoardGenerator) {
        return;
    }
    def.generator.clear();
    def.parameters.clear();
    model.setDefinition(definition, def);
}

void backfillMaterials(Model& model)
{
    const auto library = builtinMaterials();
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        Material m = model.materials().values()[i];
        if (!m.category.empty() || !m.libraryKey.empty()) {
            continue;
        }
        const auto it = std::find_if(
            library.begin(), library.end(), [&](const Material& l) { return l.name == m.name; });
        if (it != library.end()) {
            m.category = it->category;
            m.libraryKey = it->libraryKey;
            model.setMaterial(model.materials().keys()[i], m);
        }
    }
}

} // namespace aion::model
