#include "app/ConsoleListModel.h"

#include <algorithm>
#include <utility>

namespace mfplay::app
{

int ConsoleListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
    {
        return 0;
    }

    return count();
}

QVariant ConsoleListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid))
    {
        return {};
    }

    const ConsoleRow& row = rows_.at(static_cast<std::size_t>(index.row()));

    switch (role)
    {
    case KeyRole:
        return row.key;
    case NameRole:
        return row.name;
    case AddressRole:
        return row.address;
    case StatusRole:
        return row.status;
    case ActivityRole:
        return row.activity;
    case SavedRole:
        return row.saved;
    case PairedRole:
        return row.paired;
    case PsnReachableRole:
        return row.psnReachable;
    case SearchingRole:
        return row.searching;
    default:
        return {};
    }
}

QHash<int, QByteArray> ConsoleListModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {NameRole, "name"},
        {AddressRole, "address"},
        {StatusRole, "status"},
        {ActivityRole, "activity"},
        {SavedRole, "saved"},
        {PairedRole, "paired"},
        {PsnReachableRole, "psnReachable"},
        {SearchingRole, "searching"},
    };
}

int ConsoleListModel::count() const
{
    return static_cast<int>(rows_.size());
}

int ConsoleListModel::readyCount() const
{
    return statusCounts().ready;
}

int ConsoleListModel::standbyCount() const
{
    return statusCounts().standby;
}

int ConsoleListModel::unpairedCount() const
{
    return statusCounts().unpaired;
}

ConsoleListModel::StatusCounts ConsoleListModel::statusCounts() const
{
    StatusCounts counts;

    for (const ConsoleRow& row : rows_)
    {
        if (row.status == QLatin1String("ready"))
        {
            ++counts.ready;
        }
        else if (row.status == QLatin1String("standby"))
        {
            ++counts.standby;
        }
        else if (row.status == QLatin1String("unpaired"))
        {
            ++counts.unpaired;
        }
    }

    return counts;
}

const ConsoleRow* ConsoleListModel::find(const QString& key) const
{
    const auto row = std::ranges::find(rows_, key, &ConsoleRow::key);
    return row != rows_.end() ? &*row : nullptr;
}

void ConsoleListModel::setRows(std::vector<ConsoleRow> rows)
{
    const StatusCounts previousCounts = statusCounts();
    const bool sameConsoles = std::ranges::equal(rows_, rows, {}, &ConsoleRow::key,
                                                 &ConsoleRow::key);

    if (!sameConsoles)
    {
        const bool countChanges = rows.size() != rows_.size();

        beginResetModel();
        rows_ = std::move(rows);
        endResetModel();

        if (countChanges)
        {
            emit countChanged();
        }

        if (statusCounts() != previousCounts)
        {
            emit countsChanged();
        }

        return;
    }

    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        if (rows[i] == rows_[i])
        {
            continue;
        }

        rows_[i] = std::move(rows[i]);
        const QModelIndex changed = index(static_cast<int>(i));
        emit dataChanged(changed, changed);
    }

    if (statusCounts() != previousCounts)
    {
        emit countsChanged();
    }
}

} // namespace mfplay::app
