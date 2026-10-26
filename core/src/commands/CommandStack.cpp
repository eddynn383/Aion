#include "aion/core/commands/CommandStack.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace aion::commands {

namespace {

std::function<void(const geom::ValidationReport&)> abortOnInvalid()
{
    return [](const geom::ValidationReport& report) {
        std::fprintf(stderr, "Modelul nu mai e valid:\n%s", report.text().c_str());
        std::abort();
    };
}

#ifdef NDEBUG
constexpr bool kValidateByDefault = false;
#else
constexpr bool kValidateByDefault = true;
#endif

} // namespace

CommandStack::CommandStack(geom::Mesh& mesh)
    : m_mesh(&mesh)
    , m_validate(kValidateByDefault)
    , m_onInvalid(abortOnInvalid())
{
}

CommandStack::CommandStack(model::Model& model)
    : m_model(&model)
    , m_active(model.root())
    , m_validate(kValidateByDefault)
    , m_onInvalid(abortOnInvalid())
{
}

void CommandStack::setActiveDefinition(model::DefinitionId definition)
{
    assert(m_model != nullptr);
    m_active = definition;
}

geom::Mesh& CommandStack::activeMesh()
{
    return m_model ? m_model->mesh(m_active) : *m_mesh;
}

void CommandStack::beginTransaction(std::string name)
{
    assert(!inTransaction());
    m_transactionName = std::move(name);
    if (m_model) {
        m_model->beginRecording();
    } else {
        m_mesh->beginRecording();
    }
}

bool CommandStack::commit()
{
    assert(inTransaction());
    std::string name = std::move(*m_transactionName);
    m_transactionName.reset();
    std::unique_ptr<Command> command;
    if (m_model) {
        model::ChangeSet changes = m_model->endRecording();
        if (!changes.empty()) {
            command = std::make_unique<ModelCommand>(*m_model, std::move(name), std::move(changes));
        }
    } else {
        geom::ChangeSet changes = m_mesh->endRecording();
        if (!changes.empty()) {
            command = std::make_unique<RecordedCommand>(*m_mesh, std::move(name), std::move(changes));
        }
    }
    if (!command) {
        return false;
    }
    m_undo.push_back(std::move(command));
    m_undoSerials.push_back(m_nextSerial++);
    m_redo.clear();
    m_redoSerials.clear();
    afterChange();
    return true;
}

void CommandStack::abort()
{
    assert(inTransaction());
    m_transactionName.reset();
    if (m_model) {
        const model::ChangeSet changes = m_model->endRecording();
        m_model->apply(changes, geom::ChangeDirection::Backward);
    } else {
        const geom::ChangeSet changes = m_mesh->endRecording();
        m_mesh->apply(changes, geom::ChangeDirection::Backward);
    }
}

void CommandStack::push(std::unique_ptr<Command> command)
{
    assert(!inTransaction());
    command->apply();
    m_undo.push_back(std::move(command));
    m_undoSerials.push_back(m_nextSerial++);
    m_redo.clear();
    m_redoSerials.clear();
    afterChange();
}

std::string CommandStack::undoName() const
{
    return m_undo.empty() ? std::string() : m_undo.back()->name();
}

std::string CommandStack::redoName() const
{
    return m_redo.empty() ? std::string() : m_redo.back()->name();
}

bool CommandStack::undo()
{
    assert(!inTransaction());
    if (m_undo.empty()) {
        return false;
    }
    std::unique_ptr<Command> command = std::move(m_undo.back());
    m_undo.pop_back();
    command->revert();
    m_redo.push_back(std::move(command));
    m_redoSerials.push_back(m_undoSerials.back());
    m_undoSerials.pop_back();
    afterChange();
    return true;
}

bool CommandStack::redo()
{
    assert(!inTransaction());
    if (m_redo.empty()) {
        return false;
    }
    std::unique_ptr<Command> command = std::move(m_redo.back());
    m_redo.pop_back();
    command->apply();
    m_undo.push_back(std::move(command));
    m_undoSerials.push_back(m_redoSerials.back());
    m_redoSerials.pop_back();
    afterChange();
    return true;
}

void CommandStack::markClean()
{
    m_cleanSerial = m_undoSerials.empty() ? 0 : m_undoSerials.back();
    if (m_onChanged) {
        m_onChanged();
    }
}

void CommandStack::markDirty()
{
    m_cleanSerial = ~std::uint64_t { 0 };
    if (m_onChanged) {
        m_onChanged();
    }
}

bool CommandStack::isClean() const
{
    return (m_undoSerials.empty() ? 0 : m_undoSerials.back()) == m_cleanSerial;
}

void CommandStack::clear()
{
    m_undo.clear();
    m_redo.clear();
    m_undoSerials.clear();
    m_redoSerials.clear();
    m_cleanSerial = 0;
    afterChange();
}

void CommandStack::setValidation(bool enabled, geom::ValidationLevel level)
{
    m_validate = enabled;
    m_validationLevel = level;
}

void CommandStack::setOnInvalid(std::function<void(const geom::ValidationReport&)> handler)
{
    m_onInvalid = std::move(handler);
}

void CommandStack::afterChange()
{
    if (m_validate) {
        const geom::ValidationReport report
            = m_model ? m_model->validate(m_validationLevel) : m_mesh->validate(m_validationLevel);
        if (!report.ok() && m_onInvalid) {
            m_onInvalid(report);
        }
    }
    if (m_onChanged) {
        m_onChanged();
    }
}

} // namespace aion::commands
