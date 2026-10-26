#pragma once

#include <QAbstractListModel>
#include <QColor>
#include <QVariantList>
#include <QtQml/qqmlregistration.h>

// Lista de comenzi filtrată fuzzy după textul tastat în command palette.
// Comenzile se definesc în QML ca obiecte { id, title, category, shortcut, icon }.
class CommandSearch : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVariantList commands READ commands WRITE setCommands NOTIFY commandsChanged)
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(QColor highlightColor MEMBER m_highlightColor NOTIFY highlightColorChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role { CommandIdRole = Qt::UserRole + 1, TitleRole, TitleHtmlRole, CategoryRole, ShortcutRole, IconRole };

    explicit CommandSearch(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QVariantList commands() const { return m_commands; }
    void setCommands(const QVariantList& commands);
    QString query() const { return m_query; }
    void setQuery(const QString& query);
    int count() const { return static_cast<int>(m_results.size()); }

    Q_INVOKABLE QString commandIdAt(int row) const;

signals:
    void commandsChanged();
    void queryChanged();
    void highlightColorChanged();
    void countChanged();

private:
    struct Result {
        int commandIndex;
        int score;
        std::vector<std::size_t> titlePositions;
    };

    void refilter();
    QVariantMap commandAt(int commandIndex) const;

    QVariantList m_commands;
    QString m_query;
    QColor m_highlightColor { 0x3B, 0x82, 0xF6 };
    std::vector<Result> m_results;
};
