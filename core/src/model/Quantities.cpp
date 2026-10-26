#include "aion/core/model/Quantities.h"

#include "aion/core/geom/Mesh.h"
#include "aion/core/model/Operations.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <limits>
#include <map>
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
};

void collect(const Model& model, DefinitionId definition, const Transform& toWorld, const std::string& path,
    MaterialId inherited, std::vector<Occurrence>& out)
{
    for (InstanceId id : model.childrenOf(definition)) {
        if (!model.isVisible(id)) {
            continue;
        }
        const Instance& inst = model.instance(id);
        const Transform world = toWorld * inst.transform;
        const std::string name = path.empty() ? instanceName(model, id) : path + " / " + instanceName(model, id);
        const MaterialId material = inst.material ? inst.material : inherited;
        out.push_back({ id, inst.definition, world, name, material });
        collect(model, inst.definition, world, name, material, out);
    }
}

std::vector<Occurrence> occurrences(const Model& model)
{
    std::vector<Occurrence> out;
    out.push_back({ {}, model.root(), Transform {}, "", {} });
    collect(model, model.root(), Transform {}, "", {}, out);
    return out;
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

std::string csvField(const std::string& text, CsvFormat format)
{
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

CutList cutList(const Model& model, const CutListOptions& options)
{
    CutList list;
    // Piesele identice (aceleași dimensiuni la 0,1 mm, același material și nume) se adună.
    std::map<std::tuple<std::string, std::string, long long, long long, long long>, std::size_t> index;
    const auto key = [](double v) { return std::llround(v * 10.0); };

    for (const Occurrence& o : occurrences(model)) {
        if (!o.instance) {
            continue; // geometria liberă nu e o piesă
        }
        const geom::Mesh& mesh = model.mesh(o.definition);
        if (mesh.faces().empty()) {
            continue; // un grup doar cu alte grupuri (de exemplu un corp de mobilier întreg)
        }
        // Fața principală: cea mai mare.
        const geom::Face* main = nullptr;
        FaceInfo mainInfo;
        for (const geom::Face& face : mesh.faces().values()) {
            const FaceInfo info = faceInfo(mesh, face, o.toWorld);
            if (!main || info.area > mainInfo.area) {
                main = &face;
                mainInfo = info;
            }
        }
        // Lungimea: direcția celei mai lungi muchii a feței principale; lățimea: perpendiculara din plan.
        const std::vector<Vec3d> ring = ringPoints(mesh, main->outer, o.toWorld);
        Vec3d u { 1.0, 0.0, 0.0 };
        double longest = -1.0;
        for (std::size_t i = 0; i < ring.size(); ++i) {
            const Vec3d d = ring[(i + 1) % ring.size()] - ring[i];
            if (glm::length(d) > longest) {
                longest = glm::length(d);
                u = glm::normalize(d);
            }
        }
        const Vec3d n = mainInfo.normal;
        const Vec3d v = glm::normalize(glm::cross(n, u));
        double minU = std::numeric_limits<double>::infinity(), maxU = -minU;
        double minV = minU, maxV = -minU, minN = minU, maxN = -minU;
        for (const geom::Vertex& vertex : mesh.vertices().values()) {
            const Vec3d p = o.toWorld.applyToPoint(vertex.position);
            minU = std::min(minU, glm::dot(p, u));
            maxU = std::max(maxU, glm::dot(p, u));
            minV = std::min(minV, glm::dot(p, v));
            maxV = std::max(maxV, glm::dot(p, v));
            minN = std::min(minN, glm::dot(p, n));
            maxN = std::max(maxN, glm::dot(p, n));
        }
        CutPart part;
        part.name = instanceName(model, o.instance);
        part.material = dominantMaterial(model, o);
        part.length = std::max(maxU - minU, maxV - minV);
        part.width = std::min(maxU - minU, maxV - minV);
        part.thickness = maxN - minN;
        part.flat = part.thickness < 0.01;
        part.panel = !part.flat && part.thickness <= options.maxThickness;

        const auto k = std::tuple { part.name, part.material, key(part.length), key(part.width), key(part.thickness) };
        auto it = index.find(k);
        if (it == index.end()) {
            it = index.emplace(k, list.parts.size()).first;
            list.parts.push_back(part);
        }
        list.parts[it->second].count += 1;
        if (!part.flat) {
            list.edgeBanding += 2.0 * (part.length + part.width);
        }
    }

    // Necesarul de plăci, pe material și grosime.
    std::map<std::pair<std::string, long long>, SheetNeed> needs;
    for (const CutPart& part : list.parts) {
        if (!part.panel) {
            continue;
        }
        SheetNeed& need = needs[{ part.material, key(part.thickness) }];
        need.material = part.material;
        need.thickness = part.thickness;
        need.partsArea += part.length * part.width * part.count;
    }
    const double sheetArea = options.sheetLength * options.sheetWidth;
    for (auto& [k, need] : needs) {
        need.sheets = sheetArea > 0.0 ? static_cast<int>(std::ceil(need.partsArea * (1.0 + options.waste) / sheetArea)) : 0;
        list.sheets.push_back(need);
    }

    std::sort(list.parts.begin(), list.parts.end(), [](const CutPart& a, const CutPart& b) {
        return std::tie(a.material, b.thickness, b.length, b.width, a.name)
            < std::tie(b.material, a.thickness, a.length, a.width, b.name);
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
    CsvWriter csv(format);
    csv.text("Nr.").text("Piesă").text("Material").text("Lungime (mm)").text("Lățime (mm)").text("Grosime (mm)")
        .text("Buc.").text("Arie (m²)").text("Cant max (m)").endRow();
    int number = 1;
    for (const CutPart& p : list.parts) {
        csv.number(number++, 0).text(p.name + (p.flat ? " (fără grosime)" : p.panel ? "" : " (masiv)")).text(p.material)
            .number(p.length, 1).number(p.width, 1).number(p.thickness, 1).number(p.count, 0)
            .number(p.length * p.width * p.count / kMm2PerM2, 3)
            .number(2.0 * (p.length + p.width) * p.count / 1000.0, 2).endRow();
    }
    csv.endRow();
    csv.text("Necesar plăci").text("Grosime (mm)").text("Arie piese (m²)").text("Plăci").text("Placă (mm)")
        .text("Pierderi").endRow();
    for (const SheetNeed& s : list.sheets) {
        csv.text(s.material).number(s.thickness, 1).number(s.partsArea / kMm2PerM2, 3).number(s.sheets, 0)
            .text(formatNumber(options.sheetLength, 0, format) + " x " + formatNumber(options.sheetWidth, 0, format))
            .text(formatNumber(options.waste * 100.0, 0, format) + "%").endRow();
    }
    csv.endRow();
    csv.text("Cant total maxim (m)").number(list.edgeBanding / 1000.0, 2).endRow();
    return csv.result();
}

} // namespace aion::model
