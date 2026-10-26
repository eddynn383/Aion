#include "aion/core/commands/CommandStack.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

namespace aion::commands {

CommandStack::CommandStack(geom::Mesh& mesh)
    : m_mesh(mesh)
#ifdef NDEBUG
    , m_validate(false)
#else
    , m_validate(true)
#endif
{
    m_onInvalid = [](const geom::ValidationReport& report) {
        std::fprintf(stderr, "Modelul nu mai e valid:\n%s", report.text().c_str());
        std::abort();
    };
}

void CommandStack::beginTransaction(std::string name)
{
    assert(!inTransaction());
    m_transactionName = std::move(name);
    m_mesh.beginRecording();
}

bool CommandStack::commit()
{
    assert(inTransaction());
    geom::ChangeSet changes = m_mesh.endRecording();
    std::string name = std::move(*m_transactionName);
    m_transactionName.reset();
    if (changes.empty()) {
        return false;
    }
    m_undo.push_back(std::make_unique<RecordedCommand>(std::move(name), std::move(changes)));
    m_redo.clear();
    afterChange();
    return true;
}

void CommandStack::abort()
{
    assert(inTransaction());
    const geom::ChangeSet changes = m_mesh.endRecording();
    m_transactionName.reset();
    m_mesh.apply(changes, geom::ChangeDirection::Backward);
}

void CommandStack::push(std::unique_ptr<Command> command)
{
    assert(!inTransaction());
    command->apply(m_mesh);
    m_undo.push_back(std::move(command));
    m_redo.clear();
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
    command->revert(m_mesh);
    m_redo.push_back(std::move(command));
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
    command->apply(m_mesh);
    m_undo.push_back(std::move(command));
    afterChange();
    return true;
}

void CommandStack::clear()
{
    m_undo.clear();
    m_redo.clear();
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
        const geom::ValidationReport report = m_mesh.validate(m_validationLevel);
        if (!report.ok() && m_onInvalid) {
            m_onInvalid(report);
        }
    }
    if (m_onChanged) {
        m_onChanged();
    }
}

} // namespace aion::commands
