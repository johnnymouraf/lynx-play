#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QtQmlIntegration>

#include <vector>

namespace lynx::app
{

// One console card as the home screen shows it.
struct ConsoleRow
{
    QString key;
    QString name;
    QString address;
    // ready, standby, unpaired, offline or waking.
    QString status;
    // The card's last line: when it was played, found or last seen.
    QString activity;
    bool saved = false;
    bool paired = false;
    // A direct query of its address is in progress.
    bool searching = false;

    bool operator==(const ConsoleRow&) const = default;
};

class ConsoleListModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by ConsolesController")

    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Role
    {
        KeyRole = Qt::UserRole + 1,
        NameRole,
        AddressRole,
        StatusRole,
        ActivityRole,
        SavedRole,
        PairedRole,
        SearchingRole,
    };

    using QAbstractListModel::QAbstractListModel;

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const;
    [[nodiscard]] const ConsoleRow* find(const QString& key) const;

    // Replaces the rows, updating in place when the same consoles are listed
    // in the same order, so the cards keep their state.
    void setRows(std::vector<ConsoleRow> rows);

signals:
    void countChanged();

private:
    std::vector<ConsoleRow> rows_;
};

} // namespace lynx::app
