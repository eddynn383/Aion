#include "CommandSearch.h"

#include "forma/core/text/FuzzyMatch.h"

#include <algorithm>

namespace {

// Potrivirile găsite doar în categorie („Vedere”, „Unelte”) contează mai puțin decât cele din titlu.
constexpr int kCategoryOnlyPenalty = 40;

} // namespace

CommandSearch::CommandSearch(QObject* parent)
    : QAbstractListModel(parent)
{
    connect(this, &CommandSearch::highlightColorChanged, this, [this] {
        if (!m_results.empty()) {
            emit dataChanged(index(0), index(count() - 1), { TitleHtmlRole });
        }
    });
}

int CommandSearch::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : count();
}

QVariantMap CommandSearch::commandAt(int commandIndex) const
{
    return m_commands.at(commandIndex).toMap();
}

QVariant CommandSearch::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= count()) {
        return {};
    }
    const Result& r = m_results[static_cast<std::size_t>(index.row())];
    const QVariantMap cmd = commandAt(r.commandIndex);

    switch (role) {
    case CommandIdRole:
        return cmd.value(QStringLiteral("id"));
    case TitleRole:
        return cmd.value(QStringLiteral("title"));
    case CategoryRole:
        return cmd.value(QStringLiteral("category"));
    case ShortcutRole:
        return cmd.value(QStringLiteral("shortcut"));
    case IconRole:
        return cmd.value(QStringLiteral("icon"));
    case TitleHtmlRole: {
        // Literele potrivite apar îngroșate, în culoarea de accent.
        const QString title = cmd.value(QStringLiteral("title")).toString();
        const QString color = m_highlightColor.name();
        QString html;
        std::size_t next = 0;
        for (qsizetype i = 0; i < title.size(); ++i) {
            const QString ch = title.mid(i, 1).toHtmlEscaped();
            const bool hit = next < r.titlePositions.size() && r.titlePositions[next] == static_cast<std::size_t>(i);
            if (hit) {
                html += QStringLiteral("<font color=\"%1\"><b>%2</b></font>").arg(color, ch);
                ++next;
            } else {
                html += ch;
            }
        }
        return html;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> CommandSearch::roleNames() const
{
    return {
        { CommandIdRole, "commandId" },
        { TitleRole, "title" },
        { TitleHtmlRole, "titleHtml" },
        { CategoryRole, "category" },
        { ShortcutRole, "shortcut" },
        { IconRole, "icon" },
    };
}

void CommandSearch::setCommands(const QVariantList& commands)
{
    m_commands = commands;
    emit commandsChanged();
    refilter();
}

void CommandSearch::setQuery(const QString& query)
{
    if (query == m_query) {
        return;
    }
    m_query = query;
    emit queryChanged();
    refilter();
}

QString CommandSearch::commandIdAt(int row) const
{
    if (row < 0 || row >= count()) {
        return {};
    }
    return commandAt(m_results[static_cast<std::size_t>(row)].commandIndex).value(QStringLiteral("id")).toString();
}

void CommandSearch::refilter()
{
    beginResetModel();
    m_results.clear();
    const QByteArray query = m_query.toUtf8();
    for (int i = 0; i < m_commands.size(); ++i) {
        const QVariantMap cmd = commandAt(i);
        const QByteArray title = cmd.value(QStringLiteral("title")).toString().toUtf8();
        if (auto m = forma::text::fuzzyMatch(query.toStdString(), title.toStdString())) {
            m_results.push_back({ i, m->score, std::move(m->positions) });
            continue;
        }
        const QByteArray full = (cmd.value(QStringLiteral("category")).toString() + QLatin1Char(' ')).toUtf8() + title;
        if (auto m = forma::text::fuzzyMatch(query.toStdString(), full.toStdString())) {
            m_results.push_back({ i, m->score - kCategoryOnlyPenalty, {} });
        }
    }
    std::stable_sort(m_results.begin(), m_results.end(),
        [](const Result& a, const Result& b) { return a.score > b.score; });
    endResetModel();
    emit countChanged();
}
