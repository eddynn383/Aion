#include "aion/core/model/Model.h"

#include <algorithm>
#include <cassert>

namespace aion::model {

namespace {

template <class IdT, class T>
void touchIn(std::map<IdT, geom::EntityChange<T>>& changes, const geom::SlotMap<IdT, T>& map, IdT id)
{
    if (changes.contains(id)) {
        return;
    }
    geom::EntityChange<T>& change = changes[id];
    if (const T* current = map.find(id)) {
        change.before = *current;
    }
}

template <class IdT, class T>
void finishChanges(std::map<IdT, geom::EntityChange<T>>& changes, const geom::SlotMap<IdT, T>& map)
{
    for (auto it = changes.begin(); it != changes.end();) {
        if (const T* current = map.find(it->first)) {
            it->second.after = *current;
        }
        if (it->second.before == it->second.after) {
            it = changes.erase(it);
        } else {
            ++it;
        }
    }
}

template <class IdT, class T>
void applyChanges(
    const std::map<IdT, geom::EntityChange<T>>& changes, geom::SlotMap<IdT, T>& map, geom::ChangeDirection direction)
{
    for (const auto& [id, change] : changes) {
        const std::optional<T>& target = direction == geom::ChangeDirection::Forward ? change.after : change.before;
        if (target) {
            if (T* existing = map.find(id)) {
                *existing = *target;
            } else {
                map.insertWithId(id, *target);
            }
        } else {
            map.erase(id);
        }
    }
}

template <class IdT, class T>
bool sameMap(const geom::SlotMap<IdT, T>& a, const geom::SlotMap<IdT, T>& b)
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        const T* theirs = b.find(a.keys()[i]);
        if (theirs == nullptr || !(*theirs == a.values()[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

bool ChangeSet::empty() const
{
    return definitions.empty() && instances.empty() && tags.empty() && materials.empty() && guides.empty()
        && annotations.empty() && sections.empty() && scenes.empty() && animations.empty()
        && cameras.empty() && lights.empty() && textures.empty()
        && std::all_of(meshes.begin(), meshes.end(), [](const auto& m) { return m.second.empty(); });
}

Model::Model()
{
    m_root = m_definitions.insert(Definition { "Model", false });
    createMesh(m_root);
}

Model::~Model()
{
    for (auto& [id, mesh] : m_meshes) {
        mesh->removeObserver(this);
    }
}

Model::Model(const Model& other)
{
    *this = other;
}

Model& Model::operator=(const Model& other)
{
    if (this == &other) {
        return *this;
    }
    assert(!other.m_recording && !m_recording);
    for (auto& [id, mesh] : m_meshes) {
        mesh->removeObserver(this);
    }
    m_root = other.m_root;
    m_definitions = other.m_definitions;
    m_instances = other.m_instances;
    m_tags = other.m_tags;
    m_materials = other.m_materials;
    m_guides = other.m_guides;
    m_annotations = other.m_annotations;
    m_sections = other.m_sections;
    m_scenes = other.m_scenes;
    m_animations = other.m_animations;
    m_cameras = other.m_cameras;
    m_lights = other.m_lights;
    m_textures = other.m_textures;
    m_meshes.clear();
    for (const auto& [id, mesh] : other.m_meshes) {
        auto copy = std::make_unique<geom::Mesh>(*mesh);
        copy->removeObserver(const_cast<Model*>(&other));
        copy->addObserver(this);
        m_meshes.emplace(id, std::move(copy));
    }
    m_graveyard.clear();
    for (const auto& [id, mesh] : other.m_graveyard) {
        auto copy = std::make_unique<geom::Mesh>(*mesh);
        copy->removeObserver(const_cast<Model*>(&other));
        m_graveyard.emplace(id, std::move(copy));
    }
    return *this;
}

std::vector<InstanceId> Model::childrenOf(DefinitionId parent) const
{
    std::vector<InstanceId> out;
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        if (m_instances.values()[i].parent == parent) {
            out.push_back(m_instances.keys()[i]);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<InstanceId> Model::instancesOf(DefinitionId definition) const
{
    std::vector<InstanceId> out;
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        if (m_instances.values()[i].definition == definition) {
            out.push_back(m_instances.keys()[i]);
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

bool Model::isVisible(InstanceId id) const
{
    const Instance& instance = m_instances.at(id);
    if (instance.hidden) {
        return false;
    }
    const Tag* tag = instance.tag ? m_tags.find(instance.tag) : nullptr;
    return tag == nullptr || tag->visible;
}

geom::Mesh& Model::createMesh(DefinitionId id)
{
    // O definiție ștearsă și readusă (undo) își recapătă geometria goală, cu contoarele de ID-uri intacte,
    // ca ID-urile noi să nu se suprapună cu cele din istoric.
    std::unique_ptr<geom::Mesh> mesh;
    if (const auto it = m_graveyard.find(id); it != m_graveyard.end()) {
        mesh = std::move(it->second);
        m_graveyard.erase(it);
    } else {
        mesh = std::make_unique<geom::Mesh>();
    }
    mesh->addObserver(this);
    geom::Mesh& ref = *mesh;
    m_meshes.emplace(id, std::move(mesh));
    return ref;
}

void Model::destroyMesh(DefinitionId id)
{
    auto it = m_meshes.find(id);
    if (it == m_meshes.end()) {
        return;
    }
    it->second->removeObserver(this);
    m_graveyard[id] = std::move(it->second);
    m_meshes.erase(it);
}

void Model::touch(DefinitionId id)
{
    if (m_recording) {
        touchIn(m_recording->definitions, m_definitions, id);
    }
    m_structureDirty = true;
}

void Model::touch(InstanceId id)
{
    if (m_recording) {
        touchIn(m_recording->instances, m_instances, id);
    }
    m_structureDirty = true;
}

void Model::touch(MaterialId id)
{
    if (m_recording) {
        touchIn(m_recording->materials, m_materials, id);
    }
    m_structureDirty = true;
}

void Model::touch(TagId id)
{
    if (m_recording) {
        touchIn(m_recording->tags, m_tags, id);
    }
    m_structureDirty = true;
}

DefinitionId Model::addDefinition(Definition definition)
{
    const DefinitionId id = m_definitions.insert(std::move(definition));
    if (m_recording) {
        m_recording->definitions[id];
        m_createdWhileRecording.insert(id);
    }
    geom::Mesh& mesh = createMesh(id);
    if (m_recording) {
        mesh.beginRecording();
    }
    m_structureDirty = true;
    notifyStructure();
    return id;
}

void Model::removeDefinition(DefinitionId id)
{
    assert(id != m_root);
    assert(instancesOf(id).empty() && childrenOf(id).empty());
    // Ghidajele și adnotările din definiție dispar odată cu ea.
    for (std::size_t i = m_guides.size(); i-- > 0;) {
        if (m_guides.values()[i].context == id) {
            removeGuide(m_guides.keys()[i]);
        }
    }
    for (std::size_t i = m_annotations.size(); i-- > 0;) {
        if (m_annotations.values()[i].context == id) {
            removeAnnotation(m_annotations.keys()[i]);
        }
    }
    geom::Mesh& mesh = this->mesh(id);
    // Geometria se șterge prin primitive, ca undo să o poată reface.
    for (const geom::FaceId f : std::vector(mesh.faces().keys())) {
        mesh.removeFace(f);
    }
    for (const geom::EdgeId e : std::vector(mesh.edges().keys())) {
        mesh.removeEdge(e);
    }
    for (const geom::VertexId v : std::vector(mesh.vertices().keys())) {
        mesh.removeVertex(v);
    }
    if (m_recording) {
        geom::ChangeSet changes = mesh.endRecording();
        if (!changes.empty()) {
            m_recording->meshes[id] = std::move(changes);
        }
    }
    touch(id);
    m_definitions.erase(id);
    destroyMesh(id);
    notifyStructure();
}

void Model::setDefinition(DefinitionId id, Definition definition)
{
    touch(id);
    m_definitions.at(id) = std::move(definition);
    notifyStructure();
}

InstanceId Model::addInstance(Instance instance)
{
    assert(m_definitions.contains(instance.parent) && m_definitions.contains(instance.definition));
    const InstanceId id = m_instances.insert(std::move(instance));
    if (m_recording) {
        m_recording->instances[id];
    }
    m_structureDirty = true;
    notifyStructure();
    return id;
}

void Model::removeInstance(InstanceId id)
{
    touch(id);
    m_instances.erase(id);
    notifyStructure();
}

void Model::setInstance(InstanceId id, Instance instance)
{
    touch(id);
    m_instances.at(id) = std::move(instance);
    notifyStructure();
}

TagId Model::addTag(Tag tag)
{
    const TagId id = m_tags.insert(std::move(tag));
    if (m_recording) {
        m_recording->tags[id];
    }
    m_structureDirty = true;
    notifyStructure();
    return id;
}

void Model::removeTag(TagId id)
{
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        if (m_instances.values()[i].tag == id) {
            Instance copy = m_instances.values()[i];
            copy.tag = {};
            setInstance(m_instances.keys()[i], copy);
        }
    }
    touch(id);
    m_tags.erase(id);
    notifyStructure();
}

void Model::setTag(TagId id, Tag tag)
{
    touch(id);
    m_tags.at(id) = std::move(tag);
    notifyStructure();
}

MaterialId Model::addMaterial(Material material)
{
    const MaterialId id = m_materials.insert(std::move(material));
    if (m_recording) {
        m_recording->materials[id];
    }
    m_structureDirty = true;
    notifyStructure();
    return id;
}

void Model::removeMaterial(MaterialId id)
{
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        if (m_instances.values()[i].material == id) {
            Instance copy = m_instances.values()[i];
            copy.material = {};
            setInstance(m_instances.keys()[i], copy);
        }
    }
    for (auto& [def, mesh] : m_meshes) {
        for (std::size_t i = 0; i < mesh->faces().size(); ++i) {
            if (mesh->faces().values()[i].material == id) {
                mesh->setFaceMaterial(mesh->faces().keys()[i], {});
            }
        }
    }
    touch(id);
    m_materials.erase(id);
    notifyStructure();
}

void Model::setMaterial(MaterialId id, Material material)
{
    touch(id);
    m_materials.at(id) = std::move(material);
    notifyStructure();
}

template <class IdT, class T>
IdT Model::addEntity(geom::SlotMap<IdT, T>& map, std::map<IdT, geom::EntityChange<T>> ChangeSet::*changes, T value)
{
    const IdT id = map.insert(std::move(value));
    if (m_recording) {
        ((*m_recording).*changes)[id];
    }
    m_structureDirty = true;
    notifyStructure();
    return id;
}

template <class IdT, class T>
void Model::removeEntity(geom::SlotMap<IdT, T>& map, std::map<IdT, geom::EntityChange<T>> ChangeSet::*changes, IdT id)
{
    if (m_recording) {
        touchIn((*m_recording).*changes, map, id);
    }
    map.erase(id);
    m_structureDirty = true;
    notifyStructure();
}

template <class IdT, class T>
void Model::setEntity(
    geom::SlotMap<IdT, T>& map, std::map<IdT, geom::EntityChange<T>> ChangeSet::*changes, IdT id, T value)
{
    if (m_recording) {
        touchIn((*m_recording).*changes, map, id);
    }
    map.at(id) = std::move(value);
    m_structureDirty = true;
    notifyStructure();
}

GuideId Model::addGuide(Guide guide)
{
    return addEntity(m_guides, &ChangeSet::guides, std::move(guide));
}

void Model::removeGuide(GuideId id)
{
    removeEntity(m_guides, &ChangeSet::guides, id);
}

AnnotationId Model::addAnnotation(Annotation annotation)
{
    return addEntity(m_annotations, &ChangeSet::annotations, std::move(annotation));
}

void Model::removeAnnotation(AnnotationId id)
{
    removeEntity(m_annotations, &ChangeSet::annotations, id);
}

void Model::setAnnotation(AnnotationId id, Annotation annotation)
{
    setEntity(m_annotations, &ChangeSet::annotations, id, std::move(annotation));
}

SectionId Model::addSection(SectionPlane section)
{
    return addEntity(m_sections, &ChangeSet::sections, std::move(section));
}

void Model::removeSection(SectionId id)
{
    removeEntity(m_sections, &ChangeSet::sections, id);
}

void Model::setSection(SectionId id, SectionPlane section)
{
    setEntity(m_sections, &ChangeSet::sections, id, std::move(section));
}

SceneId Model::addScene(Scene scene)
{
    return addEntity(m_scenes, &ChangeSet::scenes, std::move(scene));
}

void Model::removeScene(SceneId id)
{
    removeEntity(m_scenes, &ChangeSet::scenes, id);
}

void Model::setScene(SceneId id, Scene scene)
{
    setEntity(m_scenes, &ChangeSet::scenes, id, std::move(scene));
}

AnimationId Model::addAnimation(Animation animation)
{
    return addEntity(m_animations, &ChangeSet::animations, std::move(animation));
}

void Model::removeAnimation(AnimationId id)
{
    removeEntity(m_animations, &ChangeSet::animations, id);
}

void Model::setAnimation(AnimationId id, Animation animation)
{
    setEntity(m_animations, &ChangeSet::animations, id, std::move(animation));
}

CameraId Model::addCamera(CameraObject camera)
{
    return addEntity(m_cameras, &ChangeSet::cameras, std::move(camera));
}

void Model::removeCamera(CameraId id)
{
    removeEntity(m_cameras, &ChangeSet::cameras, id);
}

void Model::setCamera(CameraId id, CameraObject camera)
{
    setEntity(m_cameras, &ChangeSet::cameras, id, std::move(camera));
}

LightId Model::addLight(Light light)
{
    return addEntity(m_lights, &ChangeSet::lights, std::move(light));
}

void Model::removeLight(LightId id)
{
    removeEntity(m_lights, &ChangeSet::lights, id);
}

void Model::setLight(LightId id, Light light)
{
    setEntity(m_lights, &ChangeSet::lights, id, std::move(light));
}

TextureId Model::addTexture(Texture texture)
{
    return addEntity(m_textures, &ChangeSet::textures, std::move(texture));
}

void Model::removeTexture(TextureId id)
{
    // Materialele care o folosesc rămân doar cu culoarea.
    for (std::size_t i = 0; i < m_materials.size(); ++i) {
        if (m_materials.values()[i].texture == id) {
            Material copy = m_materials.values()[i];
            copy.texture = {};
            setMaterial(m_materials.keys()[i], copy);
        }
    }
    removeEntity(m_textures, &ChangeSet::textures, id);
}

void Model::setTexture(TextureId id, Texture texture)
{
    setEntity(m_textures, &ChangeSet::textures, id, std::move(texture));
}

void Model::beginRecording()
{
    assert(!m_recording);
    m_recording.emplace();
    m_createdWhileRecording.clear();
    for (auto& [id, mesh] : m_meshes) {
        mesh->beginRecording();
    }
}

ChangeSet Model::endRecording()
{
    assert(m_recording);
    ChangeSet changes = std::move(*m_recording);
    m_recording.reset();
    for (auto& [id, mesh] : m_meshes) {
        geom::ChangeSet meshChanges = mesh->endRecording();
        if (!meshChanges.empty()) {
            changes.meshes[id] = std::move(meshChanges);
        }
    }
    finishChanges(changes.definitions, m_definitions);
    finishChanges(changes.instances, m_instances);
    finishChanges(changes.tags, m_tags);
    finishChanges(changes.materials, m_materials);
    finishChanges(changes.guides, m_guides);
    finishChanges(changes.annotations, m_annotations);
    finishChanges(changes.sections, m_sections);
    finishChanges(changes.scenes, m_scenes);
    finishChanges(changes.animations, m_animations);
    finishChanges(changes.cameras, m_cameras);
    finishChanges(changes.lights, m_lights);
    finishChanges(changes.textures, m_textures);
    // O definiție creată și ștearsă în aceeași operație nu lasă nicio urmă.
    for (DefinitionId id : m_createdWhileRecording) {
        if (!changes.definitions.contains(id) && !m_definitions.contains(id)) {
            changes.meshes.erase(id);
        }
    }
    m_createdWhileRecording.clear();
    notifyStructure();
    return changes;
}

void Model::apply(const ChangeSet& changes, geom::ChangeDirection direction)
{
    assert(!m_recording);
    using geom::ChangeDirection;
    const auto applyMeshes = [&] {
        for (const auto& [id, meshChanges] : changes.meshes) {
            mesh(id).apply(meshChanges, direction);
        }
    };
    if (direction == ChangeDirection::Forward) {
        for (const auto& [id, change] : changes.definitions) {
            if (!change.before && change.after) {
                m_definitions.insertWithId(id, *change.after);
                createMesh(id);
            } else if (change.before && change.after) {
                m_definitions.at(id) = *change.after;
            }
        }
        applyChanges(changes.textures, m_textures, direction);
        applyChanges(changes.materials, m_materials, direction);
        applyMeshes();
        applyChanges(changes.instances, m_instances, direction);
        applyChanges(changes.tags, m_tags, direction);
        applyChanges(changes.guides, m_guides, direction);
        applyChanges(changes.annotations, m_annotations, direction);
        applyChanges(changes.sections, m_sections, direction);
        applyChanges(changes.scenes, m_scenes, direction);
        applyChanges(changes.animations, m_animations, direction);
        applyChanges(changes.cameras, m_cameras, direction);
        applyChanges(changes.lights, m_lights, direction);
        for (const auto& [id, change] : changes.definitions) {
            if (change.before && !change.after) {
                m_definitions.erase(id);
                destroyMesh(id);
            }
        }
    } else {
        for (const auto& [id, change] : changes.definitions) {
            if (change.before && !change.after) {
                m_definitions.insertWithId(id, *change.before);
                createMesh(id);
            }
        }
        applyChanges(changes.guides, m_guides, direction);
        applyChanges(changes.annotations, m_annotations, direction);
        applyChanges(changes.sections, m_sections, direction);
        applyChanges(changes.scenes, m_scenes, direction);
        applyChanges(changes.animations, m_animations, direction);
        applyChanges(changes.cameras, m_cameras, direction);
        applyChanges(changes.lights, m_lights, direction);
        applyChanges(changes.instances, m_instances, direction);
        applyChanges(changes.tags, m_tags, direction);
        applyMeshes();
        applyChanges(changes.materials, m_materials, direction);
        applyChanges(changes.textures, m_textures, direction);
        for (const auto& [id, change] : changes.definitions) {
            if (change.before && change.after) {
                m_definitions.at(id) = *change.before;
            } else if (!change.before && change.after) {
                m_definitions.erase(id);
                destroyMesh(id);
            }
        }
    }
    if (!changes.definitions.empty() || !changes.instances.empty() || !changes.tags.empty()
        || !changes.materials.empty() || !changes.guides.empty() || !changes.annotations.empty()
        || !changes.sections.empty() || !changes.scenes.empty() || !changes.animations.empty() || !changes.cameras.empty()
        || !changes.lights.empty() || !changes.textures.empty()) {
        m_structureDirty = true;
        notifyStructure();
    }
}

void Model::addObserver(ModelObserver* observer)
{
    m_observers.push_back(observer);
}

void Model::removeObserver(ModelObserver* observer)
{
    std::erase(m_observers, observer);
}

void Model::meshChanged(const geom::Mesh& mesh, const geom::ChangeSummary& summary)
{
    for (const auto& [id, m] : m_meshes) {
        if (m.get() == &mesh) {
            for (ModelObserver* o : m_observers) {
                o->meshChanged(id, mesh, summary);
            }
            return;
        }
    }
}

void Model::notifyStructure()
{
    if (m_recording || !m_structureDirty) {
        return;
    }
    m_structureDirty = false;
    for (ModelObserver* o : m_observers) {
        o->structureChanged(*this);
    }
}

geom::ValidationReport Model::validate(geom::ValidationLevel level) const
{
    geom::ValidationReport report;
    if (!m_definitions.contains(m_root)) {
        report.errors.push_back("rădăcina modelului lipsește");
        return report;
    }
    for (std::size_t i = 0; i < m_definitions.size(); ++i) {
        const DefinitionId id = m_definitions.keys()[i];
        const auto it = m_meshes.find(id);
        if (it == m_meshes.end()) {
            report.errors.push_back("definiția „" + m_definitions.values()[i].name + "” nu are geometrie");
            continue;
        }
        for (const geom::Face& f : it->second->faces().values()) {
            if (f.material && !m_materials.contains(f.material)) {
                report.errors.push_back("„" + m_definitions.values()[i].name + "”: față cu un material inexistent");
                break;
            }
        }
        for (const std::string& e : it->second->validate(level).errors) {
            report.errors.push_back("„" + m_definitions.values()[i].name + "”: " + e);
        }
    }
    if (m_meshes.size() != m_definitions.size()) {
        report.errors.push_back("geometrie fără definiție");
    }
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        const Instance& inst = m_instances.values()[i];
        if (!m_definitions.contains(inst.parent) || !m_definitions.contains(inst.definition)) {
            report.errors.push_back("instanță cu o definiție inexistentă");
        }
        if (inst.definition == m_root) {
            report.errors.push_back("o instanță a rădăcinii");
        }
        if (inst.tag && !m_tags.contains(inst.tag)) {
            report.errors.push_back("instanță cu un tag inexistent");
        }
        if (inst.material && !m_materials.contains(inst.material)) {
            report.errors.push_back("instanță cu un material inexistent");
        }
    }
    for (const Guide& g : m_guides.values()) {
        if (!m_definitions.contains(g.context)) {
            report.errors.push_back("ghidaj într-o definiție inexistentă");
        }
    }
    for (const Annotation& a : m_annotations.values()) {
        if (!m_definitions.contains(a.context)) {
            report.errors.push_back("adnotare într-o definiție inexistentă");
        }
    }
    // Fără cicluri: urmărim din fiecare definiție definițiile pe care le conține.
    for (std::size_t i = 0; i < m_definitions.size(); ++i) {
        const DefinitionId start = m_definitions.keys()[i];
        std::vector<DefinitionId> stack { start };
        std::set<DefinitionId> seen;
        bool cycle = false;
        while (!stack.empty() && !cycle) {
            const DefinitionId d = stack.back();
            stack.pop_back();
            for (const Instance& inst : m_instances.values()) {
                if (inst.parent != d) {
                    continue;
                }
                if (inst.definition == start) {
                    cycle = true;
                    break;
                }
                if (seen.insert(inst.definition).second) {
                    stack.push_back(inst.definition);
                }
            }
        }
        if (cycle) {
            report.errors.push_back("definiția „" + m_definitions.values()[i].name + "” se conține pe sine");
        }
    }
    return report;
}

bool Model::sameAs(const Model& other) const
{
    if (m_root != other.m_root || !sameMap(m_definitions, other.m_definitions)
        || !sameMap(m_instances, other.m_instances) || !sameMap(m_tags, other.m_tags)
        || !sameMap(m_materials, other.m_materials) || !sameMap(m_guides, other.m_guides)
        || !sameMap(m_annotations, other.m_annotations) || !sameMap(m_sections, other.m_sections)
        || !sameMap(m_scenes, other.m_scenes) || !sameMap(m_animations, other.m_animations)
        || !sameMap(m_cameras, other.m_cameras) || !sameMap(m_lights, other.m_lights)
        || !sameMap(m_textures, other.m_textures)) {
        return false;
    }
    for (const auto& [id, mesh] : m_meshes) {
        const auto it = other.m_meshes.find(id);
        if (it == other.m_meshes.end() || !mesh->sameAs(*it->second)) {
            return false;
        }
    }
    return m_meshes.size() == other.m_meshes.size();
}

} // namespace aion::model
