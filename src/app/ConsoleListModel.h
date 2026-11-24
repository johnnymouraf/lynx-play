#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QtQmlIntegration>

#include <vector>

namespace mfplay::app
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
    // How many cards show each of the states the summary counts.
    Q_PROPERTY(int readyCount READ readyCount NOTIFY countsChanged)
    Q_PROPERTY(int standbyCount READ standbyCount NOTIFY countsChanged)
    Q_PROPERTY(int unpairedCount READ unpairedCount NOTIFY countsChanged)

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
    [[nodiscard]] int readyCount() const;
    [[nodiscard]] int standbyCount() const;
    [[nodiscard]] int unpairedCount() const;
    [[nodiscard]] const ConsoleRow* find(const QString& key) const;

    // Replaces the rows, updating in place when the same consoles are listed
    // in the same order, so the cards keep their state.
    void setRows(std::vector<ConsoleRow> rows);

signals:
    void countChanged();
    void countsChanged();

private:
    struct StatusCounts
    {
        int ready = 0;
        int standby = 0;
        int unpaired = 0;

        bool operator==(const StatusCounts&) const = default;
    };

    [[nodiscard]] StatusCounts statusCounts() const;

    std::vector<ConsoleRow> rows_;
};

} // namespace mfplay::app
