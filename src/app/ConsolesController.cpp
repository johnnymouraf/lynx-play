#include "app/ConsolesController.h"

#include "remoteplay/ConsoleDiscovery.h"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

using remoteplay::DiscoveredHost;
using remoteplay::HostState;

std::string_view stateName(HostState state)
{
    switch (state)
    {
    case HostState::Ready:
        return "ready";
    case HostState::Standby:
        return "in rest mode";
    case HostState::Unknown:
        return "in an unknown state";
    }

    return "in an unknown state";
}

QString fromUtf8(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

ConsoleRow unpairedRow(const DiscoveredHost& host)
{
    const bool resting = host.state == HostState::Standby;

    return ConsoleRow{
        .key = fromUtf8(host.hostId),
        .name = fromUtf8(host.name),
        .address = fromUtf8(host.address),
        .status = QStringLiteral("unpaired"),
        .activity = resting ? QStringLiteral("Em repouso. Ligue o PS5 para parear.")
                            : QStringLiteral("Encontrado agora na rede"),
        .saved = false,
        .searching = false,
    };
}

} // namespace

ConsolesController::ConsolesController(bool remotePlayReady, QObject* parent)
    : QObject(parent), model_(this), remotePlayReady_(remotePlayReady)
{
    if (remotePlayReady_)
    {
        startDiscovery();
    }
}

ConsolesController::~ConsolesController() = default;

ConsoleListModel* ConsolesController::model()
{
    return &model_;
}

ConsolesController::DiscoveryStatus ConsolesController::discoveryStatus() const
{
    return discoveryStatus_;
}

void ConsolesController::retryDiscovery()
{
    if (!remotePlayReady_)
    {
        return;
    }

    discovery_.reset();
    discovered_.clear();
    refreshModel();
    startDiscovery();
}

void ConsolesController::startDiscovery()
{
    auto discovery = remoteplay::ConsoleDiscovery::start(
        [this](std::vector<DiscoveredHost> hosts) { postHosts(std::move(hosts)); });

    if (!discovery)
    {
        spdlog::error("Cannot search for consoles: {}", discovery.error());
        setDiscoveryStatus(DiscoveryStatus::Failed);
        return;
    }

    discovery_ = std::move(*discovery);

    if (!discovery_->hasNetwork())
    {
        spdlog::warn("No network adapter has an IPv4 address");
        setDiscoveryStatus(DiscoveryStatus::NoNetwork);
        return;
    }

    setDiscoveryStatus(DiscoveryStatus::Searching);
}

void ConsolesController::setDiscoveryStatus(DiscoveryStatus status)
{
    if (discoveryStatus_ == status)
    {
        return;
    }

    discoveryStatus_ = status;
    emit discoveryStatusChanged();
}

void ConsolesController::postHosts(std::vector<DiscoveredHost> hosts)
{
    QMetaObject::invokeMethod(
        this,
        [this, hosts = std::move(hosts)]() mutable { onHostsDiscovered(std::move(hosts)); },
        Qt::QueuedConnection);
}

void ConsolesController::onHostsDiscovered(std::vector<DiscoveredHost> hosts)
{
    std::erase_if(hosts, [](const DiscoveredHost& host) { return !host.ps5; });

    logChanges(hosts);
    discovered_ = std::move(hosts);
    refreshModel();
}

void ConsolesController::logChanges(const std::vector<DiscoveredHost>& hosts) const
{
    for (const DiscoveredHost& host : hosts)
    {
        const auto previous = std::ranges::find(discovered_, host.hostId, &DiscoveredHost::hostId);
        const bool isNew = previous == discovered_.end();

        if (isNew || previous->state != host.state || previous->address != host.address)
        {
            spdlog::info("Console {} ({}) answers from {}, {}", host.name, host.hostId,
                         host.address, stateName(host.state));
        }
    }

    for (const DiscoveredHost& host : discovered_)
    {
        if (!std::ranges::contains(hosts, host.hostId, &DiscoveredHost::hostId))
        {
            spdlog::info("Console {} ({}) stopped answering", host.name, host.hostId);
        }
    }
}

void ConsolesController::refreshModel()
{
    std::vector<ConsoleRow> rows;

    for (const DiscoveredHost& host : discovered_)
    {
        const QString key = fromUtf8(host.hostId);

        if (!std::ranges::contains(rows, key, &ConsoleRow::key))
        {
            rows.push_back(unpairedRow(host));
        }
    }

    std::ranges::sort(rows, [](const ConsoleRow& left, const ConsoleRow& right) {
        return QString::compare(left.name, right.name, Qt::CaseInsensitive) < 0;
    });
    model_.setRows(std::move(rows));
}

} // namespace lynx::app
