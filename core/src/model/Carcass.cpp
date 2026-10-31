#include "aion/core/model/Carcass.h"

#include "aion/core/model/AnimationEval.h"
#include "aion/core/model/Furniture.h"
#include "aion/core/model/MaterialLibrary.h"
#include "aion/core/model/Operations.h"
#include "aion/core/model/Parametric.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>

namespace aion::model {

namespace {

constexpr double kDoorGap = 3.0; // rostul dintre uși
constexpr double kDoorAngle = 105.0;
constexpr double kHingeInset = 100.0; // prima și ultima balama, de la marginile ușii
constexpr double kHandleInset = 40.0; // mânerul, de la marginea liberă
constexpr double kMinDividerGap = 50.0;
constexpr const char* kDecor = "alb-decor";
constexpr const char* kHinge = "hinge.overlay.110";
constexpr const char* kHandle = "handle.128";
constexpr const char* kClickedDivider = "divider.click.";

double param(const Definition& def, const char* key, double fallback = 0.0)
{
    const auto it = def.parameters.find(key);
    return it != def.parameters.end() ? it->second : fallback;
}

int count(const Definition& def, const char* key)
{
    return std::max(0, static_cast<int>(std::lround(param(def, key))));
}

// Placa din cutia [lo, hi] (coordonatele părintelui), cu grosimea pe axa `thick` și fibra pe axa `grain`.
// Axele plăcii: X = fibra, Y = cealaltă latură, Z = grosimea; rotația e proprie (fără oglindire).
DesiredPiece board(std::string role, std::string nameKey, const math::Vec3d& lo, const math::Vec3d& hi,
    int thick, int grain, std::string_view type, double thickness)
{
    const int across = 3 - thick - grain;
    math::Vec3d ex { 0.0 }, ey { 0.0 };
    ex[grain] = 1.0;
    ey[across] = 1.0;
    const math::Vec3d ez = glm::cross(ex, ey);
    math::Vec3d origin = lo;
    if (ez[thick] < 0.0) {
        origin[thick] = hi[thick];
    }
    math::Mat4d m { 1.0 };
    m[0] = glm::dvec4(ex, 0.0);
    m[1] = glm::dvec4(ey, 0.0);
    m[2] = glm::dvec4(ez, 0.0);
    m[3] = glm::dvec4(origin, 1.0);
    DesiredPiece p;
    p.role = std::move(role);
    p.kind = DesiredPiece::Kind::Board;
    p.typeKey = std::string(type);
    p.thickness = thickness;
    p.length = hi[grain] - lo[grain];
    p.width = hi[across] - lo[across];
    p.transform = math::Transform(m);
    p.nameKey = std::move(nameKey);
    return p;
}

DesiredPiece hardware(std::string role, std::string parentRole, std::string_view type, const math::Vec3d& at)
{
    DesiredPiece p;
    p.role = std::move(role);
    p.parentRole = std::move(parentRole);
    p.kind = DesiredPiece::Kind::Hardware;
    p.typeKey = std::string(type);
    p.transform = math::Transform::translation(at);
    p.nameKey = "hardware." + p.typeKey;
    return p;
}

// Balamalele unei uși după înălțimea ei (§4).
int hingeCount(double height)
{
    return height <= 900.0 ? 2 : height <= 1600.0 ? 3 : 4;
}

// O ușă aplicată: grupul (X = lățimea, Y = grosimea spre spate, Z = înălțimea) pus la `x0`, `z0` în fața
// corpului, cu placa, balamalele pe latura lor și mânerul pe marginea liberă.
void door(std::vector<DesiredPiece>& out, const std::string& role, double x0, double z0, double w, double h,
    double thickness, bool hingesLeft)
{
    if (w < 1.0 || h < 1.0) {
        return;
    }
    DesiredPiece group;
    group.role = role;
    group.kind = DesiredPiece::Kind::Group;
    group.transform = math::Transform::translation({ x0, -thickness, z0 });
    group.nameKey = "part.door";
    // Latura balamalelor și lățimea: pentru pivotul și animația de deschidere (ensureDoorMotion).
    group.behavior.anchor = hingesLeft ? Behavior::Anchor::Left : Behavior::Anchor::Right;
    group.length = w;
    out.push_back(group);

    DesiredPiece panel
        = board(role + ".panel", "part.door", { 0.0, 0.0, 0.0 }, { w, thickness, h }, 1, 2, "pal", thickness);
    panel.parentRole = role;
    out.push_back(panel);

    // Balamalele: pe spatele ușii, în interiorul corpului (dincolo de fața interioară a lateralei, pe care
    // ușa o acoperă), ca să nu treacă prin laterală.
    const int hinges = hingeCount(h);
    const HardwareType* hinge = findHardwareType(kHinge);
    const double inset = thickness + 2.0 + hinge->size.x / 2;
    const double hx = hingesLeft ? inset : w - inset;
    for (int k = 0; k < hinges; ++k) {
        const double zc = hinges == 1 ? h / 2 : kHingeInset + k * (h - 2 * kHingeInset) / (hinges - 1);
        out.push_back(hardware(role + ".hinge." + std::to_string(k + 1), role, kHinge,
            { hx, thickness + hinge->size.y / 2, zc - hinge->size.z / 2 }));
    }
    const HardwareType* handle = findHardwareType(kHandle);
    const double gx = hingesLeft ? w - kHandleInset : kHandleInset;
    out.push_back(
        hardware(role + ".handle", role, kHandle, { gx, -handle->size.y / 2, h / 2 - handle->size.z / 2 }));
}

// Mărimile corpului, cu grosimile aduse la cele permise.
struct Body {
    double width = 0.0, height = 0.0, depth = 0.0;
    double board = 18.0, back = 3.0;
    [[nodiscard]] double innerWidth() const { return width - 2 * board; }
    [[nodiscard]] double innerHeight() const { return height - 2 * board; }
    [[nodiscard]] double shelfDepth() const { return depth - back; }
};

Body bodyOf(const Definition& def)
{
    Body b;
    b.width = param(def, "width");
    b.height = param(def, "height");
    b.depth = param(def, "depth");
    b.board = nearestThickness("pal", param(def, "board", 18.0));
    b.back = nearestThickness("hdf", param(def, "back", 3.0));
    return b;
}

// Un separator: marginea din stânga (coordonatele corpului) și piesa lui.
struct Divider {
    double left = 0.0;
    std::string role;
    std::optional<double> relative;
};

// Separatoarele din câmp (la distanțe egale) și cele puse cu clic (la fracțiile lor), sortate de la stânga.
std::vector<Divider> dividersOf(const Model& model, DefinitionId body, const Definition& def, const Body& b)
{
    std::vector<Divider> out;
    if (def.generator == "appliance") {
        return out;
    }
    const int n = count(def, "dividers");
    const double pitch = (b.innerWidth() - n * b.board) / (n + 1);
    for (int k = 1; k <= n; ++k) {
        out.push_back(
            { b.board + k * pitch + (k - 1) * b.board, "divider." + std::to_string(k), std::nullopt });
    }
    for (InstanceId id : model.childrenOf(body)) {
        const Instance& inst = model.instance(id);
        if (inst.relativePosition && inst.roleKey.starts_with(kClickedDivider)) {
            const double center = b.board + std::clamp(*inst.relativePosition, 0.0, 1.0) * b.innerWidth();
            out.push_back({ center - b.board / 2, inst.roleKey, inst.relativePosition });
        }
    }
    std::sort(out.begin(), out.end(), [](const Divider& a, const Divider& c) { return a.left < c.left; });
    return out;
}

// Polițele egale dintre `z0` și `z1`, pe lățimea [x0, x1].
void shelves(std::vector<DesiredPiece>& out, const std::string& space, int n, double x0, double x1, double z0,
    double z1, const Body& b)
{
    const double gap = (z1 - z0 - n * b.board) / (n + 1);
    if (x1 - x0 < 1.0 || gap < 1.0) {
        return;
    }
    for (int m = 1; m <= n; ++m) {
        const double z = z0 + m * gap + (m - 1) * b.board;
        out.push_back(board(space + ".shelf." + std::to_string(m), "part.shelf", { x0, 0.0, z },
            { x1, b.shelfDepth(), z + b.board }, 2, 0, "pal", b.board));
    }
}

// Rolurile existente (sub-piesele directe ale corpului și ale grupurilor cu rol); la același rol, id-ul cel
// mai mic.
std::map<std::string, InstanceId> existingRoles(
    const Model& model, DefinitionId body, std::vector<InstanceId>* losers)
{
    std::map<std::string, InstanceId> roles;
    const auto collect = [&](DefinitionId parent) {
        for (InstanceId id : model.childrenOf(parent)) {
            const Instance& inst = model.instance(id);
            if (inst.roleKey.empty()) {
                continue;
            }
            const auto [it, inserted] = roles.emplace(inst.roleKey, id);
            if (!inserted) {
                InstanceId loser = id;
                if (id.value < it->second.value) {
                    loser = it->second;
                    it->second = id;
                }
                if (losers != nullptr) {
                    losers->push_back(loser);
                }
            }
        }
    };
    collect(body);
    const std::map<std::string, InstanceId> top = roles;
    for (const auto& [role, id] : top) {
        const Definition& def = model.definition(model.instance(id).definition);
        if (!def.part && !model.childrenOf(model.instance(id).definition).empty()) {
            collect(model.instance(id).definition);
        }
    }
    return roles;
}

bool kindMatches(const Model& model, InstanceId id, DesiredPiece::Kind kind)
{
    const Definition& def = model.definition(model.instance(id).definition);
    switch (kind) {
    case DesiredPiece::Kind::Board:
        return def.part && def.part->kind == PartSpec::Kind::Board;
    case DesiredPiece::Kind::Hardware:
        return def.part && def.part->kind == PartSpec::Kind::Hardware;
    case DesiredPiece::Kind::Group:
        return !def.part;
    }
    return false;
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

MaterialId decor(Model& model)
{
    for (std::size_t i = 0; i < model.materials().size(); ++i) {
        if (model.materials().values()[i].libraryKey == kDecor) {
            return model.materials().keys()[i];
        }
    }
    const auto index = libraryMaterialByKey(kDecor);
    return index ? model.addMaterial(builtinMaterials()[*index]) : MaterialId {};
}

constexpr double kDoorSeconds = 0.8;

// Interacțiunea „clic → deschide / închide” a unei uși: o regulă care pornește animația ei în sensul
// „Comută”.
bool isDoorRule(const Rule& r)
{
    return r.trigger == Trigger::Click && r.actions.size() == 1
        && r.actions[0].kind == ActionKind::PlayAnimation && r.actions[0].direction == PlayDirection::Toggle;
}

AnimationId doorAnimation(const Model& model, const Instance& door)
{
    for (const Rule& r : door.rules) {
        if (isDoorRule(r) && model.animations().contains(r.actions[0].animation)) {
            return r.actions[0].animation;
        }
    }
    return {};
}

std::string doorLabel(std::string_view role)
{
    if (role == "door.lower") {
        return "ușa de jos";
    }
    if (role == "door.upper") {
        return "ușa de sus";
    }
    return "ușa " + std::string(role.substr(role.find('.') + 1));
}

// Ușa se deschide cu o animație proprie (rotire în jurul laturii balamalelor, spre față), pornită de un clic
// în Modul interacțiune; în Animare, animația ei arată ușa deschisă. Pivotul ușii e pe latura balamalelor.
void ensureDoorMotion(Model& model, DefinitionId body, InstanceId door, const DesiredPiece& p)
{
    Instance inst = model.instance(door);
    const bool left = p.behavior.anchor == Behavior::Anchor::Left;
    inst.behavior = {};
    inst.pivot = math::Vec3d { left ? 0.0 : p.length, 0.0, 0.0 };
    const AnimationId existing = doorAnimation(model, inst);
    Animation animation = existing ? *model.animations().find(existing) : Animation {};
    animation.name = model.definition(body).name + " · " + doorLabel(p.role);
    animation.duration = kDoorSeconds;
    animation.loop = false;
    std::erase_if(animation.tracks, [&](const ObjectTrack& t) { return t.instance == door; });
    setChannelKey(animation, door, Property::RotationZ, 0.0, 0.0);
    setChannelKey(animation, door, Property::RotationZ, kDoorSeconds, left ? -kDoorAngle : kDoorAngle);
    AnimationId id = existing;
    if (id) {
        model.setAnimation(id, animation);
    } else {
        id = model.addAnimation(animation);
        Action play;
        play.kind = ActionKind::PlayAnimation;
        play.animation = id;
        play.direction = PlayDirection::Toggle;
        Rule rule;
        rule.trigger = Trigger::Click;
        rule.actions.push_back(play);
        inst.rules.push_back(rule);
    }
    model.setInstance(door, inst);
}

// Șterge o piesă; animația unei uși pleacă odată cu ea.
void removePiece(Model& model, InstanceId id)
{
    if (const AnimationId animation = doorAnimation(model, model.instance(id))) {
        model.removeAnimation(animation);
    }
    removeSubtree(model, id);
}

// Transformarea de bază nouă; cheile de animație ale piesei se mută cu diferența (ca Auto-key oprit).
void setBase(Model& model, InstanceId id, Instance& inst, const math::Transform& target)
{
    if (inst.transform == target) {
        return;
    }
    const math::Vec3d pivot = pivotLocal(model, id);
    const TransformValues before = transformValues(inst.transform, pivot);
    const TransformValues after = transformValues(target, pivot);
    for (std::size_t i = 0; i < model.animations().size(); ++i) {
        Animation animation = model.animations().values()[i];
        if (findTrack(animation, id) != nullptr) {
            (void)adjustWithoutKeys(animation, id, before, after, inst.transform, pivot);
            model.setAnimation(model.animations().keys()[i], animation);
        }
    }
    inst.transform = target;
}

void updateBoard(Model& model, Instance& inst, const DesiredPiece& p)
{
    DefinitionId d = inst.definition;
    if (model.instancesOf(d).size() > 1) {
        // O copie legată pusă de utilizator în altă parte nu se schimbă odată cu piesa.
        Definition copy = model.definition(d);
        copy.sourceKey.clear();
        d = model.addDefinition(copy);
        inst.definition = d;
    }
    Definition def = model.definition(d);
    def.generator = kBoardGenerator;
    def.parameters = { { "length", std::max(1.0, p.length) }, { "width", std::max(1.0, p.width) } };
    PartSpec part = *def.part;
    if (findBoardType(part.typeKey) == nullptr) {
        part.typeKey = p.typeKey;
    }
    part.nominalThickness = nearestThickness(part.typeKey, p.thickness);
    part.thicknessAxis = 2;
    part.grainAxis = 0;
    def.part = part;
    model.setDefinition(d, def);
    rebuildBoard(model, d);
}

InstanceId create(Model& model, DefinitionId parent, const DesiredPiece& p)
{
    InstanceId id;
    switch (p.kind) {
    case DesiredPiece::Kind::Board:
        id = createBoard(model, parent, p.typeKey, p.thickness, p.length, p.width, p.transform);
        break;
    case DesiredPiece::Kind::Hardware:
        id = createHardware(model, parent, p.typeKey, p.transform);
        break;
    case DesiredPiece::Kind::Group: {
        const DefinitionId d = model.addDefinition({ "Ușă", false });
        Instance inst;
        inst.parent = parent;
        inst.definition = d;
        inst.transform = p.transform;
        id = model.addInstance(inst);
        break;
    }
    }
    if (!id) {
        return id;
    }
    Instance inst = model.instance(id);
    inst.roleKey = p.role;
    inst.nameKey = p.nameKey;
    inst.relativePosition = p.relativePosition;
    if (p.kind == DesiredPiece::Kind::Group) {
        // Deschiderea e animația ușii (ensureDoorMotion), nu un comportament.
    } else if (p.kind == DesiredPiece::Kind::Board) {
        inst.material = decor(model);
    }
    model.setInstance(id, inst);
    return id;
}

bool near(const math::Transform& a, const math::Transform& b)
{
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            if (std::abs(a.matrix()[c][r] - b.matrix()[c][r]) > 1e-4) {
                return false;
            }
        }
    }
    return true;
}

const std::map<std::string, std::string, std::less<>>& partNames()
{
    static const std::map<std::string, std::string, std::less<>> kNames {
        { "part.board", "Placă" },
        { "part.side.left", "Lateral stânga" },
        { "part.side.right", "Lateral dreapta" },
        { "part.bottom", "Bază" },
        { "part.top", "Capac" },
        { "part.back", "Spate" },
        { "part.divider", "Separator" },
        { "part.shelf", "Poliță" },
        { "part.niche.bottom", "Poliță nișă (jos)" },
        { "part.niche.top", "Poliță nișă (sus)" },
        { "part.door", "Ușă" },
    };
    return kNames;
}

} // namespace

bool isCarcassGenerator(std::string_view id)
{
    static constexpr std::array<std::string_view, 6> kIds { "base", "wall", "tall", "appliance", "wardrobe",
        "rack" };
    return std::find(kIds.begin(), kIds.end(), id) != kIds.end();
}

std::vector<DesiredPiece> desiredPieces(const Model& model, DefinitionId body)
{
    std::vector<DesiredPiece> out;
    const Definition& def = model.definition(body);
    if (!isCarcassGenerator(def.generator)) {
        return out;
    }
    const Body b = bodyOf(def);
    const double W = b.width, H = b.height, D = b.depth, t = b.board;
    out.push_back(board("side.left", "part.side.left", { 0, 0, 0 }, { t, D, H }, 0, 2, "pal", t));
    out.push_back(board("side.right", "part.side.right", { W - t, 0, 0 }, { W, D, H }, 0, 2, "pal", t));
    out.push_back(board("bottom", "part.bottom", { t, 0, 0 }, { W - t, D, t }, 2, 0, "pal", t));
    out.push_back(board("top", "part.top", { t, 0, H - t }, { W - t, D, H }, 2, 0, "pal", t));
    out.push_back(board("back", "part.back", { t, D - b.back, t }, { W - t, D, H - t }, 1, 2, "hdf", b.back));

    const int shelfCount = count(def, "shelves");
    if (def.generator == "appliance") {
        // Nișa deschisă între două polițe fixe; sub ea o ușă, deasupra o ușă; polițele în spațiul de jos.
        const double nicheHeight
            = std::clamp(param(def, "nicheHeight", 600.0), 1.0, std::max(1.0, H - 4 * t));
        const double nb
            = std::clamp(param(def, "nicheBottom", 850.0), t, std::max(t, H - 3 * t - nicheHeight));
        const double nt = nb + t + nicheHeight;
        out.push_back(board("niche.bottom", "part.niche.bottom", { t, 0, nb },
            { W - t, b.shelfDepth(), nb + t }, 2, 0, "pal", t));
        out.push_back(board(
            "niche.top", "part.niche.top", { t, 0, nt }, { W - t, b.shelfDepth(), nt + t }, 2, 0, "pal", t));
        shelves(out, "space.1", shelfCount, t, W - t, t, nb, b);
        door(out, "door.lower", 0.0, 0.0, W, nb + t, t, true);
        door(out, "door.upper", 0.0, nt, W, H - nt, t, true);
        return out;
    }

    const std::vector<Divider> dividers = dividersOf(model, body, def, b);
    double x0 = t;
    int space = 1;
    for (const Divider& d : dividers) {
        DesiredPiece piece = board(
            d.role, "part.divider", { d.left, 0, t }, { d.left + t, b.shelfDepth(), H - t }, 0, 2, "pal", t);
        piece.relativePosition = d.relative;
        out.push_back(piece);
        shelves(out, "space." + std::to_string(space++), shelfCount, x0, d.left, t, H - t, b);
        x0 = d.left + t;
    }
    shelves(out, "space." + std::to_string(space), shelfCount, x0, W - t, t, H - t, b);

    const int doors = count(def, "doors");
    if (doors > 0) {
        const double w = (W - (doors - 1) * kDoorGap) / doors;
        const int leftHinged = (doors + 1) / 2;
        for (int i = 1; i <= doors; ++i) {
            door(out, "door." + std::to_string(i), (i - 1) * (w + kDoorGap), 0.0, w, H, t, i <= leftHinged);
        }
    }
    return out;
}

void reconcileCarcass(Model& model, DefinitionId body)
{
    const std::vector<DesiredPiece> desired = desiredPieces(model, body);
    if (desired.empty()) {
        return;
    }
    std::vector<InstanceId> losers;
    std::map<std::string, InstanceId> existing = existingRoles(model, body, &losers);
    for (InstanceId id : losers) {
        Instance inst = model.instance(id);
        inst.roleKey.clear();
        inst.relativePosition.reset();
        model.setInstance(id, inst);
    }
    const auto alive = [&](InstanceId id) { return model.instances().contains(id); };

    std::set<std::string> wanted;
    std::map<std::string, InstanceId> placed;
    for (const DesiredPiece& p : desired) {
        wanted.insert(p.role);
        DefinitionId parent = body;
        if (!p.parentRole.empty()) {
            const auto group = placed.find(p.parentRole);
            if (group == placed.end()) {
                continue;
            }
            parent = model.instance(group->second).definition;
        }
        const auto it = existing.find(p.role);
        const bool found = it != existing.end() && alive(it->second);
        if (found && model.instance(it->second).parent == parent && kindMatches(model, it->second, p.kind)) {
            const InstanceId id = it->second;
            Instance inst = model.instance(id);
            if (p.kind == DesiredPiece::Kind::Board) {
                updateBoard(model, inst, p);
            }
            inst.relativePosition = p.relativePosition;
            setBase(model, id, inst, p.transform);
            model.setInstance(id, inst);
            placed[p.role] = id;
        } else {
            if (found) {
                removePiece(model, it->second);
            }
            placed[p.role] = create(model, parent, p);
        }
        if (p.kind == DesiredPiece::Kind::Group && placed[p.role]) {
            ensureDoorMotion(model, body, placed[p.role], p);
        }
    }
    for (const auto& [role, id] : existing) {
        if (!wanted.contains(role) && alive(id)) {
            removePiece(model, id);
        }
    }
}

void refreshDividerPositions(Model& model, DefinitionId body)
{
    const Definition& def = model.definition(body);
    if (!isCarcassGenerator(def.generator)) {
        return;
    }
    const Body b = bodyOf(def);
    if (b.innerWidth() <= 0.0) {
        return;
    }
    for (InstanceId id : model.childrenOf(body)) {
        Instance inst = model.instance(id);
        if (!inst.relativePosition || !inst.roleKey.starts_with(kClickedDivider)) {
            continue;
        }
        const Definition& piece = model.definition(inst.definition);
        const math::Vec3d size { param(piece, "length"), param(piece, "width"),
            piece.part ? piece.part->nominalThickness : b.board };
        const double center = inst.transform.applyToPoint(size * 0.5).x;
        inst.relativePosition = std::clamp((center - b.board) / b.innerWidth(), 0.0, 1.0);
        model.setInstance(id, inst);
    }
}

bool rolePiecesDiffer(const Model& model, DefinitionId body)
{
    const std::vector<DesiredPiece> desired = desiredPieces(model, body);
    const std::map<std::string, InstanceId> existing = existingRoles(model, body, nullptr);
    for (const DesiredPiece& p : desired) {
        const auto it = existing.find(p.role);
        if (it == existing.end()) {
            continue;
        }
        const Instance& inst = model.instance(it->second);
        if (p.kind == DesiredPiece::Kind::Group && inst.behavior.open) {
            continue; // o ușă deschisă se închide la refacere, fără pierderi
        }
        if (!near(inst.transform, p.transform)) {
            return true;
        }
        if (p.kind == DesiredPiece::Kind::Board) {
            const Definition& def = model.definition(inst.definition);
            if (!def.part || def.generator != kBoardGenerator) {
                return true;
            }
            const PartMeasure m = measurePart(model, inst.definition, math::Transform {});
            if (!m.box || std::abs(m.length - p.length) > 1e-3 || std::abs(m.width - p.width) > 1e-3
                || std::abs(m.thickness - nearestThickness(def.part->typeKey, p.thickness)) > 1e-3) {
                return true;
            }
        }
    }
    return false;
}

bool upgradeGenerator(Model& model, DefinitionId definition)
{
    Definition def = model.definition(definition);
    const bool cabinet = def.generator == "cabinet";
    if (!cabinet && def.generator != "shelf") {
        return false;
    }
    std::map<std::string, double> given {
        { "width", param(def, "width") },
        { "height", param(def, "height") },
        { "board", nearestThickness("pal", param(def, "board", 18.0)) },
        { "back", 3.0 },
        { "dividers", 0.0 },
    };
    if (cabinet) {
        // Adâncimea veche includea ușile (grosimea plăcii).
        given["depth"] = param(def, "depth") - param(def, "board", 18.0);
        given["doors"] = param(def, "doors", 2.0);
        given["shelves"] = 0.0;
    } else {
        given["depth"] = param(def, "depth");
        given["doors"] = 0.0;
        given["shelves"] = param(def, "shelves", 4.0);
    }
    const GeneratorInfo* info = findGenerator(cabinet ? "wardrobe" : "rack");
    for (InstanceId child : model.childrenOf(definition)) {
        removeSubtree(model, child);
    }
    def.generator = info->id;
    def.name = info->name;
    def.parameters = resolvedParameters(*info, given);
    model.setDefinition(definition, def);
    reconcileCarcass(model, definition);
    return true;
}

void makePlainBody(Model& model, DefinitionId definition)
{
    Definition def = model.definition(definition);
    if (def.generator.empty() || def.generator == kBoardGenerator) {
        return;
    }
    def.generator.clear();
    def.parameters.clear();
    model.setDefinition(definition, def);
}

DividerResult addDivider(
    Model& model, DefinitionId body, const math::Vec3d& pointInBody, bool atClick, InstanceId* created)
{
    const Definition& def = model.definition(body);
    if (!isCarcassGenerator(def.generator) || def.generator == "appliance") {
        return DividerResult::NotCarcass;
    }
    const Body b = bodyOf(def);
    const double x = pointInBody.x;
    if (x <= b.board || x >= b.width - b.board) {
        return DividerResult::Outside;
    }
    // Spațiul cu punctul (între laterale și separatoare).
    double x0 = b.board, x1 = b.width - b.board;
    int next = 1;
    for (const Divider& d : dividersOf(model, body, def, b)) {
        if (x >= d.left && x <= d.left + b.board) {
            return DividerResult::TooClose;
        }
        if (d.left + b.board <= x) {
            x0 = std::max(x0, d.left + b.board);
        } else {
            x1 = std::min(x1, d.left);
        }
        if (d.role.starts_with(kClickedDivider)) {
            next = std::max(
                next, std::atoi(d.role.c_str() + std::char_traits<char>::length(kClickedDivider)) + 1);
        }
    }
    const double center = atClick ? x : (x0 + x1) / 2;
    if (center - b.board / 2 - x0 < kMinDividerGap || x1 - (center + b.board / 2) < kMinDividerGap) {
        return DividerResult::TooClose;
    }
    const InstanceId id = createBoard(model, body, "pal", b.board, 1.0, 1.0, math::Transform {});
    Instance inst = model.instance(id);
    inst.roleKey = kClickedDivider + std::to_string(next);
    inst.nameKey = "part.divider";
    inst.relativePosition = (center - b.board) / b.innerWidth();
    inst.material = decor(model);
    model.setInstance(id, inst);
    reconcileCarcass(model, body);
    if (created != nullptr) {
        *created = id;
    }
    return DividerResult::Added;
}

std::optional<math::Transform> doorOpenTransform(const Model& model, InstanceId instance)
{
    if (!model.instances().contains(instance)) {
        return std::nullopt;
    }
    const Instance& inst = model.instance(instance);
    const AnimationId id = doorAnimation(model, inst);
    if (!id || !inst.pivot) {
        return std::nullopt;
    }
    const Animation& animation = *model.animations().find(id);
    const ObjectTrack* track = findTrack(animation, instance);
    if (track == nullptr) {
        return std::nullopt;
    }
    return evaluateInstance(*track, animation.duration, inst.transform, *inst.pivot);
}

bool hasDoors(const Model& model)
{
    for (const Instance& inst : model.instances().values()) {
        if (inst.roleKey.starts_with("door.") && inst.rules.size() == 1 && isDoorRule(inst.rules[0])) {
            return true;
        }
    }
    return false;
}

InstanceId carcassInPath(const Model& model, std::span<const InstanceId> path)
{
    for (auto it = path.rbegin(); it != path.rend(); ++it) {
        if (model.instances().contains(*it)
            && isCarcassGenerator(model.definition(model.instance(*it).definition).generator)) {
            return *it;
        }
    }
    return {};
}

std::string partName(const Model& model, InstanceId instance)
{
    if (!model.instances().contains(instance)) {
        return {};
    }
    const Instance& inst = model.instance(instance);
    if (!inst.name.empty()) {
        return inst.name;
    }
    if (!inst.nameKey.empty()) {
        const auto it = partNames().find(inst.nameKey);
        if (it != partNames().end()) {
            return it->second;
        }
        if (inst.nameKey.starts_with("hardware.")) {
            if (const HardwareType* type = findHardwareType(inst.nameKey.substr(9))) {
                return type->label;
            }
        }
    }
    const Definition& def = model.definition(inst.definition);
    if (!def.name.empty()) {
        return def.name;
    }
    return def.group ? "Grup" : "Obiect";
}

} // namespace aion::model
