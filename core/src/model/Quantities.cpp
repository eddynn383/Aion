#include "aion/core/model/Quantities.h"

#include "aion/core/geom/Mesh.h"
#include "aion/core/model/Furniture.h"
#include "aion/core/model/MaterialLibrary.h"
#include "aion/core/model/Operations.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
#include <string_view>
#include <optional>
#include <tuple>

namespace aion::model {

namespace {

using math::Transform;
using math::Vec3d;

// Vectorul de arie al unui inel (Newell): direcția = normala, lungimea = dublul ariei.
Vec3d newell(const std::vector<Vec3d>& ring)
{
    Vec3d n { 0.0 };
    for (std::size_t i = 0; i < ring.size(); ++i) {
        const Vec3d& a = ring[i];
        const Vec3d& b = ring[(i + 1) % ring.size()];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n;
}

std::vector<Vec3d> ringPoints(const geom::Mesh& mesh, const geom::Loop& loop, const Transform& t)
{
    std::vector<Vec3d> out;
    for (geom::VertexId v : mesh.loopVertices(loop)) {
        out.push_back(t.applyToPoint(mesh.position(v)));
    }
    return out;
}

// O față transformată: aria, normala (unitară, după transformare) și un punct (pentru volum).
struct FaceInfo {
    double area = 0.0;
    Vec3d normal { 0.0, 0.0, 1.0 };
    Vec3d point { 0.0 };
    MaterialId material;
};

FaceInfo faceInfo(const geom::Mesh& mesh, const geom::Face& face, const Transform& t)
{
    FaceInfo info;
    const std::vector<Vec3d> outer = ringPoints(mesh, face.outer, t);
    Vec3d n = newell(outer);
    // Normala transformată a planului feței dă orientarea (o oglindire ar inversa ordinea vârfurilor).
    const Vec3d planeNormal = glm::normalize(glm::transpose(glm::inverse(glm::dmat3(t.matrix()))) * face.plane.normal);
    if (glm::dot(n, planeNormal) < 0.0) {
        n = -n;
    }
    double twiceArea = glm::length(n);
    for (const geom::Loop& hole : face.inner) {
        twiceArea -= glm::length(newell(ringPoints(mesh, hole, t)));
    }
    info.area = std::max(twiceArea, 0.0) * 0.5;
    info.normal = planeNormal;
    info.point = outer.empty() ? Vec3d { 0.0 } : outer.front();
    info.material = face.material;
    return info;
}

std::string materialName(const Model& model, MaterialId id)
{
    if (id && model.materials().contains(id)) {
        return model.material(id).name;
    }
    return "Implicit";
}

std::string instanceName(const Model& model, InstanceId id)
{
    const Instance& inst = model.instance(id);
    if (!inst.name.empty()) {
        return inst.name;
    }
    const Definition& def = model.definition(inst.definition);
    if (!def.name.empty()) {
        return def.name;
    }
    return def.group ? "Grup" : "Obiect";
}

// O apariție a unei definiții în model (o instanță, cu toate transformările părinților).
struct Occurrence {
    InstanceId instance; // nul pentru geometria liberă din model
    DefinitionId definition;
    Transform toWorld;
    std::string path;
    MaterialId inherited; // materialul grupului (pentru fețele fără material)
    bool hidden = false; // ascunsă: ea, tag-ul ei sau un părinte
    bool inHardware = false; // în interiorul unei bucăți de feronerie (nu se numără separat)
    bool inBoard = false; // în interiorul unei plăci
};

void collect(const Model& model, DefinitionId definition, const Transform& toWorld, const std::string& path,
    MaterialId inherited, bool includeHidden, bool hidden, bool inHardware, bool inBoard, std::vector<Occurrence>& out)
{
    for (InstanceId id : model.childrenOf(definition)) {
        const bool isHidden = hidden || !model.isVisible(id);
        if (isHidden && !includeHidden) {
            continue;
        }
        const Instance& inst = model.instance(id);
        const Transform world = toWorld * inst.transform;
        const std::string name = path.empty() ? instanceName(model, id) : path + " / " + instanceName(model, id);
        const MaterialId material = inst.material ? inst.material : inherited;
        out.push_back({ id, inst.definition, world, name, material, isHidden, inHardware, inBoard });
        const std::optional<PartSpec>& part = model.definition(inst.definition).part;
        collect(model, inst.definition, world, name, material, includeHidden, isHidden,
            inHardware || (part && part->kind == PartSpec::Kind::Hardware),
            inBoard || (part && part->kind == PartSpec::Kind::Board), out);
    }
}

std::vector<Occurrence> occurrences(const Model& model, bool includeHidden = false)
{
    std::vector<Occurrence> out;
    out.push_back({ {}, model.root(), Transform {}, "", {} });
    collect(model, model.root(), Transform {}, "", {}, includeHidden, false, false, false, out);
    return out;
}

// Materialul predominant (după arie) al fețelor unei apariții, cu materialul efectiv (al feței, altfel moștenit); la
// egalitate, după cheia din bibliotecă și apoi după nume, ca rezultatul să nu depindă de ordine.
MaterialId dominantMaterialId(const Model& model, const Occurrence& o)
{
    const geom::Mesh& mesh = model.mesh(o.definition);
    std::map<MaterialId, double> byMaterial;
    for (const geom::Face& face : mesh.faces().values()) {
        byMaterial[face.material ? face.material : o.inherited] += faceInfo(mesh, face, o.toWorld).area;
    }
    MaterialId best;
    double bestArea = -1.0;
    const auto rank = [&](MaterialId id) {
        const Material* m = id ? model.materials().find(id) : nullptr;
        return m ? std::pair { m->libraryKey, m->name } : std::pair { std::string(), std::string() };
    };
    for (const auto& [id, area] : byMaterial) {
        if (area > bestArea + 1e-6 || (std::abs(area - bestArea) <= 1e-6 && rank(id) < rank(best))) {
            best = id;
            bestArea = area;
        }
    }
    return best;
}

// Materialul predominant (după arie) al fețelor unei apariții.
std::string dominantMaterial(const Model& model, const Occurrence& o)
{
    const geom::Mesh& mesh = model.mesh(o.definition);
    std::map<std::string, double> byName;
    for (const geom::Face& face : mesh.faces().values()) {
        const FaceInfo info = faceInfo(mesh, face, o.toWorld);
        byName[materialName(model, face.material ? face.material : o.inherited)] += info.area;
    }
    std::string best = "Implicit";
    double bestArea = -1.0;
    for (const auto& [name, area] : byName) {
        if (area > bestArea) {
            best = name;
            bestArea = area;
        }
    }
    return best;
}

std::string formatNumber(double value, int decimals, CsvFormat format)
{
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, "%.*f", decimals, value);
    std::string text = buffer;
    if (format.decimal != '.') {
        std::replace(text.begin(), text.end(), '.', format.decimal);
    }
    return text;
}

std::string csvField(const std::string& raw, CsvFormat format)
{
    // Un text care începe ca o formulă (=, +, -, @, TAB, CR) primește un apostrof: foaia de calcul îl arată ca text.
    std::string text = raw;
    if (!text.empty() && std::string_view("=+-@\t\r").find(text.front()) != std::string_view::npos) {
        text.insert(text.begin(), '\'');
    }
    const bool quote = text.find(format.separator) != std::string::npos || text.find('"') != std::string::npos
        || text.find('\n') != std::string::npos;
    if (!quote) {
        return text;
    }
    std::string out = "\"";
    for (char c : text) {
        out += c;
        if (c == '"') {
            out += '"';
        }
    }
    return out + "\"";
}

class CsvWriter {
public:
    explicit CsvWriter(CsvFormat format)
        : m_format(format)
    {
        m_text = "\xEF\xBB\xBF"; // BOM: Excel recunoaște UTF-8 (diacriticele)
    }

    CsvWriter& text(const std::string& value)
    {
        separate();
        m_text += csvField(value, m_format);
        return *this;
    }
    CsvWriter& number(double value, int decimals)
    {
        separate();
        m_text += formatNumber(value, decimals, m_format);
        return *this;
    }
    CsvWriter& endRow()
    {
        m_text += "\r\n";
        m_rowStarted = false;
        return *this;
    }
    [[nodiscard]] const std::string& result() const { return m_text; }

private:
    void separate()
    {
        if (m_rowStarted) {
            m_text += m_format.separator;
        }
        m_rowStarted = true;
    }

    CsvFormat m_format;
    std::string m_text;
    bool m_rowStarted = false;
};

constexpr double kMm2PerM2 = 1.0e6;
constexpr double kMm3PerM3 = 1.0e9;

} // namespace

QuantityReport quantityReport(const Model& model)
{
    QuantityReport report;
    std::map<std::string, double> byMaterial;
    std::map<std::string, double> byTag;
    std::map<std::tuple<std::string, DefinitionId>, std::size_t> rows;

    for (const Occurrence& o : occurrences(model)) {
        const geom::Mesh& mesh = model.mesh(o.definition);
        if (mesh.faces().empty() && mesh.edges().empty()) {
            continue;
        }
        const bool loose = !o.instance;
        std::string tag = "Fără tag";
        if (!loose) {
            const Instance& inst = model.instance(o.instance);
            if (inst.tag && model.tags().contains(inst.tag)) {
                tag = model.tags().at(inst.tag).name;
            }
        }

        double area = 0.0;
        double signedVolume = 0.0;
        Vec3d lo { std::numeric_limits<double>::infinity() };
        Vec3d hi { -std::numeric_limits<double>::infinity() };
        for (const geom::Face& face : mesh.faces().values()) {
            const FaceInfo info = faceInfo(mesh, face, o.toWorld);
            area += info.area;
            // Teorema divergenței: V = Σ (p · n) A / 3.
            signedVolume += glm::dot(info.point, info.normal) * info.area / 3.0;
            byMaterial[materialName(model, face.material ? face.material : o.inherited)] += info.area;
            byTag[tag] += info.area;
            if (info.normal.z > 0.999) {
                report.horizontalArea += info.area;
            }
            ++report.faceCount;
        }
        for (const geom::Edge& edge : mesh.edges().values()) {
            const Vec3d a = o.toWorld.applyToPoint(mesh.position(edge.v0));
            const Vec3d b = o.toWorld.applyToPoint(mesh.position(edge.v1));
            report.edgeLength += glm::length(b - a);
            lo = glm::min(lo, glm::min(a, b));
            hi = glm::max(hi, glm::max(a, b));
        }
        report.totalArea += area;

        const bool solid = !loose && isSolid(model, o.definition);
        const double volume = solid ? std::abs(signedVolume) : 0.0;
        report.totalVolume += volume;

        const std::string name = loose ? "Geometrie liberă (în afara grupurilor)" : o.path;
        const auto key = std::tuple { name, o.definition };
        auto it = rows.find(key);
        if (it == rows.end()) {
            ObjectQuantity row;
            row.name = name;
            row.component = !loose && !model.definition(o.definition).group;
            row.solid = solid;
            row.size = lo.x <= hi.x ? hi - lo : Vec3d { 0.0 };
            row.material = dominantMaterial(model, o);
            row.tag = tag;
            it = rows.emplace(key, report.objects.size()).first;
            report.objects.push_back(row);
        }
        ObjectQuantity& row = report.objects[it->second];
        row.count += 1;
        row.area += area;
        row.volume += volume;
    }

    for (const auto& [name, area] : byMaterial) {
        report.materials.push_back({ name, area });
    }
    for (const auto& [name, area] : byTag) {
        report.tags.push_back({ name, area });
    }
    const auto larger = [](const NamedArea& a, const NamedArea& b) { return a.area > b.area; };
    std::sort(report.materials.begin(), report.materials.end(), larger);
    std::sort(report.tags.begin(), report.tags.end(), larger);
    return report;
}

namespace {

const char* sectionKey(CutSection section)
{
    switch (section) {
    case CutSection::Board:
        return "board";
    case CutSection::Hardware:
        return "hardware";
    case CutSection::Other:
        return "other";
    case CutSection::Undefined:
        return "undefined";
    }
    return "";
}

std::string roundKey(double v)
{
    return std::to_string(std::llround(v * 10.0));
}

} // namespace

CutList cutList(const Model& model, const CutListOptions& options)
{
    CutList list;
    std::map<std::string, std::size_t> index; // rowKey → rând

    for (const Occurrence& o : occurrences(model, true)) {
        if (!o.instance || o.inHardware) {
            continue; // geometria liberă nu e o piesă; părțile feroneriei nu se numără separat
        }
        const Definition& def = model.definition(o.definition);
        const geom::Mesh& mesh = model.mesh(o.definition);
        const bool hardware = def.part && def.part->kind == PartSpec::Kind::Hardware;
        if (!hardware && mesh.faces().empty()) {
            continue; // un grup doar cu alte grupuri (de exemplu un corp de mobilier întreg)
        }
        CutPart part;
        part.name = instanceName(model, o.instance);
        part.hidden = o.hidden;
        const MaterialId decorId = dominantMaterialId(model, o);
        part.material = materialName(model, decorId);
        const Material* decor = decorId ? model.materials().find(decorId) : nullptr;
        std::string identity;

        if (hardware) {
            const HardwareType* type = findHardwareType(def.part->typeKey);
            part.section = type ? CutSection::Hardware : CutSection::Undefined;
            part.typeKey = def.part->typeKey;
            part.panel = false;
            if (type == nullptr) {
                part.flags.push_back("tip necunoscut");
            } else {
                part.name = type->label;
            }
            if (o.inBoard) {
                part.flags.push_back("în placă");
            }
            identity = part.typeKey;
        } else if (def.part && def.part->kind == PartSpec::Kind::Board && def.part->rawKind.empty()) {
            const PartSpec& spec = *def.part;
            const BoardType* type = findBoardType(spec.typeKey);
            const PartMeasure measure = measurePart(model, o.definition, o.toWorld);
            part.length = measure.length;
            part.width = measure.width;
            part.thickness = measure.thickness;
            part.flat = part.thickness < 0.01;
            part.typeKey = spec.typeKey;
            part.nominalThickness = spec.nominalThickness;
            if (type == nullptr) {
                part.section = CutSection::Undefined;
                part.panel = false;
                part.flags.push_back("tip necunoscut");
            } else {
                part.section = CutSection::Board;
                part.panel = type->toCutter && !part.flat;
            }
            if (std::abs(part.thickness - spec.nominalThickness) > 0.1) {
                part.flags.push_back("grosime nepotrivită");
            }
            if (!measure.box) {
                part.flags.push_back("formă neregulată");
            }
            if (measure.extraShells) {
                part.flags.push_back("geometrie în plus");
            }
            // Cantul: L1, L2, W1, W2 și dimensiunea de tăiere.
            const std::array<int, 4> slots = bandSlots(spec, measure);
            double deductL = 0.0;
            double deductW = 0.0;
            for (int k = 0; k < 4; ++k) {
                const BandSide& side = spec.bands[static_cast<std::size_t>(slots[static_cast<std::size_t>(k)])];
                part.bands[static_cast<std::size_t>(k)] = side.bandKey;
                if (side.bandKey.empty()) {
                    continue;
                }
                std::string decorName = part.material;
                if (!side.decor.empty()) {
                    if (const auto i = libraryMaterialByKey(side.decor)) {
                        decorName = builtinMaterials()[*i].name;
                    }
                }
                part.bandDecors[static_cast<std::size_t>(k)] = decorName;
                if (const BandType* band = findBandType(side.bandKey); band != nullptr && band->deduct) {
                    (k < 2 ? deductW : deductL) += band->thickness; // L1/L2 scad din lățime, W1/W2 din lungime
                }
            }
            part.cutLength = part.length - deductL;
            part.cutWidth = part.width - deductW;
            identity = spec.typeKey + "|" + roundKey(spec.nominalThickness);
            for (int k = 0; k < 4; ++k) {
                identity += "|" + part.bands[static_cast<std::size_t>(k)] + "/" + part.bandDecors[static_cast<std::size_t>(k)];
            }
        } else {
            // Fără date de piesă: după categoria materialului (D10), altfel regula din ADR 0017.
            const PartMeasure measure = measurePart(model, o.definition, o.toWorld);
            part.length = measure.length;
            part.width = measure.width;
            part.thickness = measure.thickness;
            part.flat = part.thickness < 0.01;
            const std::string category = decor ? decor->category : std::string();
            if (def.part) {
                part.section = CutSection::Undefined;
                part.panel = false;
                part.flags.push_back("tip necunoscut");
            } else if (category == "metal" || category == "glass" || category == "plastic" || category == "stone") {
                part.section = CutSection::Other;
                part.panel = false;
            } else if (category == "wood") {
                part.section = CutSection::Undefined;
                part.panel = false;
                part.flags.push_back("lemn fără tip");
            } else {
                part.section = CutSection::Board;
                part.panel = !part.flat && part.thickness <= options.maxThickness;
                part.flags.push_back("tip nedefinit");
            }
            part.cutLength = part.length;
            part.cutWidth = part.width;
            identity = "-";
        }
        if (o.hidden) {
            part.flags.push_back("ascunsă");
        }
        if (part.section != CutSection::Hardware && part.section != CutSection::Other) {
            identity += "|" + roundKey(part.length) + "|" + roundKey(part.width) + "|" + roundKey(part.thickness);
        } else if (part.section == CutSection::Other) {
            identity += "|" + roundKey(part.length) + "|" + roundKey(part.width) + "|" + roundKey(part.thickness);
        }
        part.rowKey = std::string(sectionKey(part.section)) + "|" + identity + "|" + part.material + "|"
            + (o.hidden ? "hidden" : "visible");

        auto it = index.find(part.rowKey);
        if (it == index.end()) {
            it = index.emplace(part.rowKey, list.parts.size()).first;
            list.parts.push_back(part);
        }
        CutPart& row = list.parts[it->second];
        row.count += 1;
        const std::string name = instanceName(model, o.instance);
        if (std::find(row.usedIn.begin(), row.usedIn.end(), name) == row.usedIn.end()) {
            row.usedIn.push_back(name);
        }
    }

    // Totalurile (deviz): doar rândurile incluse.
    std::map<std::tuple<std::string, std::string, long long>, SheetNeed> needs;
    std::map<std::pair<std::string, std::string>, BandNeed> bands;
    std::map<std::string, HardwareNeed> hardware;
    for (const CutPart& part : list.parts) {
        if (options.excluded.contains(part.rowKey)) {
            if (part.hidden) {
                list.hiddenPieces += part.count;
            }
            continue;
        }
        if (part.section == CutSection::Hardware) {
            HardwareNeed& need = hardware[part.typeKey];
            need.typeKey = part.typeKey;
            need.label = part.name;
            need.count += part.count;
            continue;
        }
        if (part.section != CutSection::Board) {
            continue;
        }
        if (part.panel) {
            const BoardType* type = findBoardType(part.typeKey);
            const std::string group = type ? part.typeKey : part.material;
            SheetNeed& need = needs[{ part.typeKey, group, std::llround(part.thickness * 10.0) }];
            need.material = type ? type->label : part.material;
            need.typeKey = part.typeKey;
            need.thickness = type ? part.nominalThickness : part.thickness;
            need.sheetLength = type && type->sheetLength > 0.0 ? type->sheetLength : options.sheetLength;
            need.sheetWidth = type && type->sheetWidth > 0.0 ? type->sheetWidth : options.sheetWidth;
            need.partsArea += part.length * part.width * part.count;
        }
        if (part.typeKey.empty() && !part.flat) {
            list.edgeBanding += 2.0 * (part.length + part.width) * part.count; // estimarea veche (ADR 0017)
        }
        for (int k = 0; k < 4; ++k) {
            const std::string& key = part.bands[static_cast<std::size_t>(k)];
            if (key.empty()) {
                continue;
            }
            const auto allowance = options.bandAllowanceByType.find(key);
            const double extra = allowance != options.bandAllowanceByType.end() ? allowance->second : options.bandAllowance;
            BandNeed& need = bands[{ key, part.bandDecors[static_cast<std::size_t>(k)] }];
            need.bandKey = key;
            need.decor = part.bandDecors[static_cast<std::size_t>(k)];
            need.length += ((k < 2 ? part.length : part.width) + extra) * part.count;
            need.edges += part.count;
        }
    }
    for (auto& [k, need] : needs) {
        const double sheetArea = need.sheetLength * need.sheetWidth;
        need.sheets = sheetArea > 0.0 ? static_cast<int>(std::ceil(need.partsArea * (1.0 + options.waste) / sheetArea)) : 0;
        list.sheets.push_back(need);
    }
    for (auto& [k, need] : bands) {
        list.bands.push_back(need);
    }
    for (auto& [k, need] : hardware) {
        list.hardware.push_back(need);
    }

    std::sort(list.parts.begin(), list.parts.end(), [](const CutPart& a, const CutPart& b) {
        return std::tie(a.section, a.typeKey, b.thickness, b.length, b.width, a.material, a.hidden, a.name)
            < std::tie(b.section, b.typeKey, a.thickness, a.length, a.width, b.material, b.hidden, b.name);
    });
    return list;
}

std::string toCsv(const QuantityReport& report, CsvFormat format)
{
    CsvWriter csv(format);
    csv.text("Obiect").text("Tip").text("Buc.").text("Material").text("Tag").text("Lungime X (mm)")
        .text("Lățime Y (mm)").text("Înălțime Z (mm)").text("Arie (m²)").text("Volum (m³)").endRow();
    for (const ObjectQuantity& o : report.objects) {
        csv.text(o.name).text(o.component ? "Obiect" : "Grup").number(o.count, 0).text(o.material).text(o.tag)
            .number(o.size.x, 1).number(o.size.y, 1).number(o.size.z, 1)
            .number(o.area / kMm2PerM2, 3).text(o.solid ? formatNumber(o.volume / kMm3PerM3, 4, format) : "")
            .endRow();
    }
    csv.endRow();
    csv.text("Material").text("Arie (m²)").endRow();
    for (const NamedArea& m : report.materials) {
        csv.text(m.name).number(m.area / kMm2PerM2, 3).endRow();
    }
    csv.endRow();
    csv.text("Tag").text("Arie (m²)").endRow();
    for (const NamedArea& t : report.tags) {
        csv.text(t.name).number(t.area / kMm2PerM2, 3).endRow();
    }
    csv.endRow();
    csv.text("Total arie (m²)").number(report.totalArea / kMm2PerM2, 3).endRow();
    csv.text("Total volum solide (m³)").number(report.totalVolume / kMm3PerM3, 4).endRow();
    csv.text("Suprafețe orizontale în sus (m²)").number(report.horizontalArea / kMm2PerM2, 3).endRow();
    csv.text("Fețe").number(static_cast<double>(report.faceCount), 0).endRow();
    csv.text("Lungime muchii (m)").number(report.edgeLength / 1000.0, 3).endRow();
    return csv.result();
}

std::string toCsv(const CutList& list, const CutListOptions& options, CsvFormat format)
{
    static const std::map<CutSection, std::string> kSection { { CutSection::Board, "Plăci" },
        { CutSection::Hardware, "Feronerie" }, { CutSection::Other, "Alte componente" },
        { CutSection::Undefined, "Nedefinite" } };
    const auto bandLabel = [](const std::string& key) {
        const BandType* band = findBandType(key);
        return band ? band->label : key;
    };
    CsvWriter csv(format);
    if (options.headerRow) {
        csv.text(options.cutDimensions ? "Dimensiuni: de tăiere" : "Dimensiuni: finite").endRow();
    }
    csv.text("Secțiune").text("Nume (folosită în)").text("Tip").text("Grosime (mm)").text("Lungime (mm)")
        .text("Lățime (mm)").text("Buc.").text("Decor").text("L1").text("L2").text("W1").text("W2")
        .text("Lungime de tăiere (mm)").text("Lățime de tăiere (mm)").text("Motiv").endRow();
    for (const CutPart& p : list.parts) {
        if (options.excluded.contains(p.rowKey)) {
            continue;
        }
        std::string names;
        for (const std::string& n : p.usedIn) {
            names += (names.empty() ? "" : ", ") + n;
        }
        std::string reasons;
        for (const std::string& f : p.flags) {
            reasons += (reasons.empty() ? "" : ", ") + f;
        }
        const BoardType* type = findBoardType(p.typeKey);
        const bool measured = p.section == CutSection::Board || p.section == CutSection::Undefined
            || p.section == CutSection::Other;
        const double length = options.cutDimensions ? p.cutLength : p.length;
        const double width = options.cutDimensions ? p.cutWidth : p.width;
        csv.text(kSection.at(p.section)).text(names).text(type ? type->label : p.typeKey);
        if (measured) {
            csv.number(p.thickness, 1).number(length, 1).number(width, 1);
        } else {
            csv.text("").text("").text("");
        }
        csv.number(p.count, 0).text(p.material);
        for (const std::string& b : p.bands) {
            csv.text(b.empty() ? "" : bandLabel(b));
        }
        if (measured) {
            csv.number(p.cutLength, 1).number(p.cutWidth, 1);
        } else {
            csv.text("").text("");
        }
        csv.text(reasons).endRow();
    }
    csv.endRow();
    csv.text("Necesar plăci").text("Grosime (mm)").text("Arie piese (m²)").text("Plăci").text("Placă (mm)")
        .text("Pierderi").endRow();
    for (const SheetNeed& s : list.sheets) {
        csv.text(s.material).number(s.thickness, 1).number(s.partsArea / kMm2PerM2, 3).number(s.sheets, 0)
            .text(formatNumber(s.sheetLength, 0, format) + " x " + formatNumber(s.sheetWidth, 0, format))
            .text(formatNumber(options.waste * 100.0, 0, format) + "%").endRow();
    }
    if (!list.bands.empty()) {
        csv.endRow();
        csv.text("Cant").text("Decor").text("Muchii").text("Lungime (m)").endRow();
        for (const BandNeed& b : list.bands) {
            csv.text(bandLabel(b.bandKey)).text(b.decor).number(b.edges, 0).number(b.length / 1000.0, 2).endRow();
        }
    }
    if (!list.hardware.empty()) {
        csv.endRow();
        csv.text("Feronerie").text("Buc.").endRow();
        for (const HardwareNeed& h : list.hardware) {
            csv.text(h.label).number(h.count, 0).endRow();
        }
    }
    if (list.edgeBanding > 0.0) {
        csv.endRow();
        csv.text("Cant estimat, piese fără tip (m)").number(list.edgeBanding / 1000.0, 2).endRow();
    }
    return csv.result();
}

} // namespace aion::model
