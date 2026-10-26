#include "aion/core/io/AionFile.h"

#include "aion/core/Version.h"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <system_error>

namespace aion::io {

namespace {

using nlohmann::json;

constexpr const char* kModelEntry = "model.json";
constexpr const char* kThumbnailEntry = "thumbnail.png";

template <class IdT, class T>
std::vector<std::size_t> sortedIndices(const geom::SlotMap<IdT, T>& map)
{
    std::vector<std::size_t> order(map.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = i;
    }
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return map.keys()[a] < map.keys()[b]; });
    return order;
}

json loopToJson(const geom::Loop& loop)
{
    json uses = json::array();
    for (const geom::EdgeUse& u : loop.uses) {
        const auto id = static_cast<std::int64_t>(u.edge.value);
        uses.push_back(u.reversed ? -id : id);
    }
    return uses;
}

json meshToJson(const geom::Mesh& mesh)
{
    json vertices = json::array();
    for (std::size_t i : sortedIndices(mesh.vertices())) {
        const geom::Vertex& v = mesh.vertices().values()[i];
        vertices.push_back({ mesh.vertices().keys()[i].value, v.position.x, v.position.y, v.position.z });
    }
    json edges = json::array();
    for (std::size_t i : sortedIndices(mesh.edges())) {
        const geom::Edge& e = mesh.edges().values()[i];
        edges.push_back({ mesh.edges().keys()[i].value, e.v0.value, e.v1.value, e.soft ? 1 : 0, e.curve,
            e.smoothCurve ? 1 : 0 });
    }
    json faces = json::array();
    for (std::size_t i : sortedIndices(mesh.faces())) {
        const geom::Face& f = mesh.faces().values()[i];
        json inner = json::array();
        for (const geom::Loop& h : f.inner) {
            inner.push_back(loopToJson(h));
        }
        faces.push_back({
            { "id", mesh.faces().keys()[i].value },
            { "material", f.material.value },
            { "backMaterial", f.backMaterial.value },
            { "plane", { f.plane.normal.x, f.plane.normal.y, f.plane.normal.z, f.plane.d } },
            { "outer", loopToJson(f.outer) },
            { "inner", inner },
        });
    }
    return { { "vertices", vertices }, { "edges", edges }, { "faces", faces } };
}

geom::Loop loopFromJson(const json& j)
{
    geom::Loop loop;
    for (const json& u : j) {
        const std::int64_t id = u.get<std::int64_t>();
        loop.uses.push_back({ geom::EdgeId { static_cast<std::uint64_t>(id < 0 ? -id : id) }, id < 0 });
    }
    return loop;
}

// Geometria unei definiții, ca ChangeSet de creare (ID-urile păstrate); vecinătățile se refac din muchii și fețe.
geom::ChangeSet meshFromJson(const json& j)
{
    std::map<geom::VertexId, geom::Vertex> vertices;
    std::map<geom::EdgeId, geom::Edge> edges;
    std::map<geom::FaceId, geom::Face> faces;
    for (const json& v : j.at("vertices")) {
        vertices[geom::VertexId { v.at(0).get<std::uint64_t>() }]
            = geom::Vertex { { v.at(1).get<double>(), v.at(2).get<double>(), v.at(3).get<double>() }, {} };
    }
    for (const json& e : j.at("edges")) {
        geom::Edge edge { geom::VertexId { e.at(1).get<std::uint64_t>() }, geom::VertexId { e.at(2).get<std::uint64_t>() },
            {} };
        edge.soft = e.at(3).get<int>() != 0;
        edge.curve = e.at(4).get<std::uint32_t>();
        edge.smoothCurve = e.at(5).get<int>() != 0;
        edges[geom::EdgeId { e.at(0).get<std::uint64_t>() }] = edge;
    }
    for (const json& f : j.at("faces")) {
        const json& p = f.at("plane");
        geom::Face face;
        face.plane = { { p.at(0).get<double>(), p.at(1).get<double>(), p.at(2).get<double>() }, p.at(3).get<double>() };
        face.outer = loopFromJson(f.at("outer"));
        face.material = model::MaterialId { f.value("material", std::uint64_t { 0 }) };
        face.backMaterial = model::MaterialId { f.value("backMaterial", std::uint64_t { 0 }) };
        for (const json& h : f.at("inner")) {
            face.inner.push_back(loopFromJson(h));
        }
        faces[geom::FaceId { f.at("id").get<std::uint64_t>() }] = face;
    }
    for (auto& [id, e] : edges) {
        vertices.at(e.v0).edges.push_back(id);
        vertices.at(e.v1).edges.push_back(id);
    }
    for (auto& [id, f] : faces) {
        const auto attach = [&](const geom::Loop& loop) {
            for (const geom::EdgeUse& u : loop.uses) {
                edges.at(u.edge).faces.push_back(id);
            }
        };
        attach(f.outer);
        for (const geom::Loop& h : f.inner) {
            attach(h);
        }
    }
    geom::ChangeSet changes;
    for (auto& [id, v] : vertices) {
        changes.vertices[id].after = std::move(v);
    }
    for (auto& [id, e] : edges) {
        changes.edges[id].after = std::move(e);
    }
    for (auto& [id, f] : faces) {
        changes.faces[id].after = std::move(f);
    }
    return changes;
}

json colorToJson(const model::Color& c)
{
    return { c.r, c.g, c.b, c.a };
}

} // namespace

std::string toJson(const model::Model& model)
{
    json definitions = json::array();
    for (std::size_t i : sortedIndices(model.definitions())) {
        const model::DefinitionId id = model.definitions().keys()[i];
        const model::Definition& d = model.definitions().values()[i];
        definitions.push_back({
            { "id", id.value },
            { "name", d.name },
            { "group", d.group },
            { "mesh", meshToJson(model.mesh(id)) },
        });
    }
    json instances = json::array();
    for (std::size_t i : sortedIndices(model.instances())) {
        const model::Instance& inst = model.instances().values()[i];
        json matrix = json::array();
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                matrix.push_back(inst.transform.matrix()[c][r]);
            }
        }
        instances.push_back({
            { "id", model.instances().keys()[i].value },
            { "parent", inst.parent.value },
            { "definition", inst.definition.value },
            { "transform", matrix },
            { "name", inst.name },
            { "tag", inst.tag.value },
            { "hidden", inst.hidden },
            { "locked", inst.locked },
            { "material", inst.material.value },
        });
        if (inst.behavior.kind != model::Behavior::Kind::None) {
            instances.back()["behavior"] = {
                { "kind", static_cast<int>(inst.behavior.kind) },
                { "anchor", static_cast<int>(inst.behavior.anchor) },
                { "amount", inst.behavior.amount },
                { "duration", inst.behavior.duration },
                { "open", inst.behavior.open },
            };
        }
    }
    json tags = json::array();
    for (std::size_t i : sortedIndices(model.tags())) {
        const model::Tag& t = model.tags().values()[i];
        tags.push_back({
            { "id", model.tags().keys()[i].value },
            { "name", t.name },
            { "color", colorToJson(t.color) },
            { "visible", t.visible },
        });
    }
    json materials = json::array();
    for (std::size_t i : sortedIndices(model.materials())) {
        const model::Material& m = model.materials().values()[i];
        materials.push_back({
            { "id", model.materials().keys()[i].value },
            { "name", m.name },
            { "color", colorToJson(m.color) },
            { "roughness", m.roughness },
            { "metallic", m.metallic },
        });
    }
    const auto vec = [](const math::Vec3d& v) { return json { v.x, v.y, v.z }; };
    json guides = json::array();
    for (std::size_t i : sortedIndices(model.guides())) {
        const model::Guide& g = model.guides().values()[i];
        guides.push_back({ { "id", model.guides().keys()[i].value }, { "context", g.context.value },
            { "point", vec(g.point) }, { "direction", vec(g.direction) } });
    }
    json annotations = json::array();
    for (std::size_t i : sortedIndices(model.annotations())) {
        const model::Annotation& a = model.annotations().values()[i];
        annotations.push_back({ { "id", model.annotations().keys()[i].value },
            { "kind", a.kind == model::Annotation::Kind::Text ? "text" : "dimension" }, { "context", a.context.value },
            { "a", vec(a.a) }, { "b", vec(a.b) }, { "offset", vec(a.offset) }, { "text", a.text } });
    }
    json sections = json::array();
    for (std::size_t i : sortedIndices(model.sections())) {
        const model::SectionPlane& s = model.sections().values()[i];
        sections.push_back({ { "id", model.sections().keys()[i].value }, { "name", s.name }, { "point", vec(s.point) },
            { "normal", vec(s.normal) }, { "active", s.active } });
    }
    json scenes = json::array();
    for (std::size_t i : sortedIndices(model.scenes())) {
        const model::Scene& s = model.scenes().values()[i];
        scenes.push_back({ { "id", model.scenes().keys()[i].value }, { "name", s.name }, { "target", vec(s.target) },
            { "yaw", s.yaw }, { "pitch", s.pitch }, { "distance", s.distance }, { "orthographic", s.orthographic },
            { "style", s.style } });
    }
    const json doc {
        { "format", "aion" },
        { "formatVersion", kFormatVersion },
        { "generator", std::string("Aion ") + std::string(versionString()) },
        { "units", "mm" },
        { "root", model.root().value },
        { "definitions", definitions },
        { "instances", instances },
        { "tags", tags },
        { "materials", materials },
        { "guides", guides },
        { "annotations", annotations },
        { "sections", sections },
        { "scenes", scenes },
    };
    return doc.dump(1);
}

LoadResult fromJson(std::string_view text)
{
    LoadResult result;
    const json doc = json::parse(text, nullptr, false);
    if (doc.is_discarded() || !doc.is_object()) {
        result.error = "Fișierul nu conține un model Aion valid.";
        return result;
    }
    if (doc.value("format", std::string()) != "aion") {
        result.error = "Fișierul nu e un model Aion.";
        return result;
    }
    if (doc.value("formatVersion", 0) > kFormatVersion) {
        result.error = "Fișierul a fost salvat de o versiune mai nouă a Aion.";
        return result;
    }
    try {
        model::Model model;
        if (model::DefinitionId { doc.at("root").get<std::uint64_t>() } != model.root()) {
            result.error = "Fișierul are o structură necunoscută (rădăcina modelului).";
            return result;
        }
        model::ChangeSet changes;
        for (const json& d : doc.at("definitions")) {
            const model::DefinitionId id { d.at("id").get<std::uint64_t>() };
            model::Definition def { d.at("name").get<std::string>(), d.at("group").get<bool>() };
            auto& change = changes.definitions[id];
            if (id == model.root()) {
                change.before = model.definition(id);
            }
            change.after = std::move(def);
            changes.meshes[id] = meshFromJson(d.at("mesh"));
        }
        for (const json& i : doc.at("instances")) {
            model::Instance inst;
            inst.parent = model::DefinitionId { i.at("parent").get<std::uint64_t>() };
            inst.definition = model::DefinitionId { i.at("definition").get<std::uint64_t>() };
            const json& m = i.at("transform");
            math::Mat4d matrix(1.0);
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    matrix[c][r] = m.at(static_cast<std::size_t>(c * 4 + r)).get<double>();
                }
            }
            inst.transform = math::Transform(matrix);
            inst.name = i.value("name", std::string());
            inst.tag = model::TagId { i.value("tag", std::uint64_t { 0 }) };
            inst.hidden = i.value("hidden", false);
            inst.locked = i.value("locked", false);
            inst.material = model::MaterialId { i.value("material", std::uint64_t { 0 }) };
            if (const auto b = i.find("behavior"); b != i.end()) {
                inst.behavior.kind = static_cast<model::Behavior::Kind>(std::clamp(b->value("kind", 0), 0, 2));
                inst.behavior.anchor = static_cast<model::Behavior::Anchor>(std::clamp(b->value("anchor", 0), 0, 5));
                inst.behavior.amount = b->value("amount", 90.0);
                inst.behavior.duration = b->value("duration", 0.6);
                inst.behavior.open = b->value("open", false);
            }
            changes.instances[model::InstanceId { i.at("id").get<std::uint64_t>() }].after = std::move(inst);
        }
        for (const json& t : doc.at("tags")) {
            const json& c = t.at("color");
            model::Tag tag { t.at("name").get<std::string>(),
                { c.at(0).get<std::uint8_t>(), c.at(1).get<std::uint8_t>(), c.at(2).get<std::uint8_t>(),
                    c.at(3).get<std::uint8_t>() },
                t.value("visible", true) };
            changes.tags[model::TagId { t.at("id").get<std::uint64_t>() }].after = std::move(tag);
        }
        for (const json& mj : doc.value("materials", json::array())) {
            const json& c = mj.at("color");
            model::Material material { mj.at("name").get<std::string>(),
                { c.at(0).get<std::uint8_t>(), c.at(1).get<std::uint8_t>(), c.at(2).get<std::uint8_t>(),
                    c.at(3).get<std::uint8_t>() },
                mj.value("roughness", 0.8f), mj.value("metallic", 0.0f) };
            changes.materials[model::MaterialId { mj.at("id").get<std::uint64_t>() }].after = std::move(material);
        }
        const auto vec = [](const json& v) {
            return math::Vec3d { v.at(0).get<double>(), v.at(1).get<double>(), v.at(2).get<double>() };
        };
        for (const json& g : doc.value("guides", json::array())) {
            changes.guides[model::GuideId { g.at("id").get<std::uint64_t>() }].after = model::Guide {
                model::DefinitionId { g.at("context").get<std::uint64_t>() }, vec(g.at("point")), vec(g.at("direction")) };
        }
        for (const json& a : doc.value("annotations", json::array())) {
            model::Annotation annotation;
            annotation.kind = a.value("kind", std::string()) == "text" ? model::Annotation::Kind::Text
                                                                      : model::Annotation::Kind::Dimension;
            annotation.context = model::DefinitionId { a.at("context").get<std::uint64_t>() };
            annotation.a = vec(a.at("a"));
            annotation.b = vec(a.at("b"));
            annotation.offset = vec(a.at("offset"));
            annotation.text = a.value("text", std::string());
            changes.annotations[model::AnnotationId { a.at("id").get<std::uint64_t>() }].after = std::move(annotation);
        }
        for (const json& s : doc.value("sections", json::array())) {
            changes.sections[model::SectionId { s.at("id").get<std::uint64_t>() }].after = model::SectionPlane {
                s.value("name", std::string()), vec(s.at("point")), vec(s.at("normal")), s.value("active", true) };
        }
        for (const json& s : doc.value("scenes", json::array())) {
            model::Scene scene;
            scene.name = s.value("name", std::string());
            scene.target = vec(s.at("target"));
            scene.yaw = s.value("yaw", 0.0);
            scene.pitch = s.value("pitch", 0.0);
            scene.distance = s.value("distance", 10000.0);
            scene.orthographic = s.value("orthographic", false);
            scene.style = s.value("style", std::string());
            changes.scenes[model::SceneId { s.at("id").get<std::uint64_t>() }].after = std::move(scene);
        }
        model.apply(changes, geom::ChangeDirection::Forward);
        const geom::ValidationReport report = model.validate(geom::ValidationLevel::Topology);
        if (!report.ok()) {
            result.error = "Fișierul e deteriorat: " + report.errors.front();
            return result;
        }
        result.model = std::move(model);
    } catch (const std::exception& e) {
        result.error = std::string("Fișierul e deteriorat (") + e.what() + ").";
    }
    return result;
}

std::vector<std::uint8_t> writeArchive(const model::Model& model, std::span<const std::uint8_t> thumbnailPng)
{
    const std::string text = toJson(model);
    mz_zip_archive zip {};
    if (!mz_zip_writer_init_heap(&zip, 0, 0)) {
        return {};
    }
    bool ok = mz_zip_writer_add_mem(&zip, kModelEntry, text.data(), text.size(), MZ_BEST_COMPRESSION);
    if (ok && !thumbnailPng.empty()) {
        ok = mz_zip_writer_add_mem(&zip, kThumbnailEntry, thumbnailPng.data(), thumbnailPng.size(), MZ_NO_COMPRESSION);
    }
    void* buffer = nullptr;
    std::size_t size = 0;
    ok = ok && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size);
    std::vector<std::uint8_t> out;
    if (ok) {
        const auto* bytes = static_cast<const std::uint8_t*>(buffer);
        out.assign(bytes, bytes + size);
    }
    mz_zip_writer_end(&zip);
    return out;
}

namespace {

std::optional<std::vector<std::uint8_t>> extract(std::span<const std::uint8_t> bytes, const char* name)
{
    mz_zip_archive zip {};
    if (!mz_zip_reader_init_mem(&zip, bytes.data(), bytes.size(), 0)) {
        return std::nullopt;
    }
    std::size_t size = 0;
    void* data = mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
    std::optional<std::vector<std::uint8_t>> out;
    if (data != nullptr) {
        const auto* p = static_cast<const std::uint8_t*>(data);
        out.emplace(p, p + size);
        mz_free(data);
    }
    mz_zip_reader_end(&zip);
    return out;
}

} // namespace

LoadResult readArchive(std::span<const std::uint8_t> bytes)
{
    const auto text = extract(bytes, kModelEntry);
    if (!text) {
        LoadResult r;
        r.error = "Fișierul nu e o arhivă Aion (lipsește model.json).";
        return r;
    }
    return fromJson(std::string_view(reinterpret_cast<const char*>(text->data()), text->size()));
}

std::vector<std::uint8_t> readThumbnail(std::span<const std::uint8_t> bytes)
{
    return extract(bytes, kThumbnailEntry).value_or(std::vector<std::uint8_t> {});
}

bool saveFile(const std::filesystem::path& path, const model::Model& model, std::span<const std::uint8_t> thumbnailPng,
    std::string* error)
{
    const std::vector<std::uint8_t> bytes = writeArchive(model, thumbnailPng);
    if (bytes.empty()) {
        if (error != nullptr) {
            *error = "Arhiva nu a putut fi creată.";
        }
        return false;
    }
    std::filesystem::path temp = path;
    temp += ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!out) {
            if (error != nullptr) {
                *error = "Fișierul nu a putut fi scris.";
            }
            return false;
        }
    }
    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(temp, ec);
        if (error != nullptr) {
            *error = "Fișierul nu a putut fi înlocuit.";
        }
        return false;
    }
    return true;
}

LoadResult loadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        LoadResult r;
        r.error = "Fișierul nu poate fi deschis.";
        return r;
    }
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return readArchive(bytes);
}

} // namespace aion::io
