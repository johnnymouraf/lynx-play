#pragma once

#include "app/ConsoleListModel.h"
#include "remoteplay/Console.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

#include <memory>
#include <vector>

namespace lynx::remoteplay
{
class ConsoleDiscovery;
}

namespace lynx::app
{

// The consoles the home screen lists, and the actions on them.
class ConsolesController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    Q_PROPERTY(ConsoleListModel* model READ model CONSTANT)
    Q_PROPERTY(DiscoveryStatus discoveryStatus READ discoveryStatus NOTIFY discoveryStatusChanged)

public:
    enum class DiscoveryStatus
    {
        Searching,
        NoNetwork,
        Failed,
        Unavailable,
    };
    Q_ENUM(DiscoveryStatus)

    explicit ConsolesController(bool remotePlayReady, QObject* parent = nullptr);
    ~ConsolesController() override;

    [[nodiscard]] ConsoleListModel* model();
    [[nodiscard]] DiscoveryStatus discoveryStatus() const;

    // Starts the network search again, after a failure or a network change.
    Q_INVOKABLE void retryDiscovery();

signals:
    void discoveryStatusChanged();
    // A message for a toast; `tone` is info, ready, standby or danger.
    void notice(const QString& tone, const QString& title, const QString& message);

private:
    void startDiscovery();
    void setDiscoveryStatus(DiscoveryStatus status);
    // Runs on chiaki's discovery threads.
    void postHosts(std::vector<remoteplay::DiscoveredHost> hosts);
    void onHostsDiscovered(std::vector<remoteplay::DiscoveredHost> hosts);
    void logChanges(const std::vector<remoteplay::DiscoveredHost>& hosts) const;
    void refreshModel();

    ConsoleListModel model_;
    std::unique_ptr<remoteplay::ConsoleDiscovery> discovery_;
    std::vector<remoteplay::DiscoveredHost> discovered_;
    DiscoveryStatus discoveryStatus_ = DiscoveryStatus::Unavailable;
    bool remotePlayReady_ = false;
};

} // namespace lynx::app
