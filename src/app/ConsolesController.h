#pragma once

#include "app/ConsoleListModel.h"
#include "remoteplay/Console.h"
#include "remoteplay/ConsoleMerge.h"
#include "remoteplay/ConsoleStore.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQmlIntegration>

#include <chrono>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace lynx::remoteplay
{
class ConsoleDiscovery;
}

namespace lynx::app
{

// The consoles the home screen lists, saved and answering, and the actions
// on them.
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

    // Without a store, nothing is remembered after the app closes.
    ConsolesController(bool remotePlayReady, std::optional<remoteplay::ConsoleStore> store,
                       QObject* parent = nullptr);
    ~ConsolesController() override;

    [[nodiscard]] ConsoleListModel* model();
    [[nodiscard]] DiscoveryStatus discoveryStatus() const;

    // Starts the network search again, after a failure or a network change.
    Q_INVOKABLE void retryDiscovery();
    // Shows the card as searching for a few seconds; the search itself never
    // stops.
    Q_INVOKABLE void refresh(const QString& key);
    // Saves a console that wasn't; an empty name shows the console's own.
    Q_INVOKABLE void rename(const QString& key, const QString& name);
    Q_INVOKABLE void forget(const QString& key);
    Q_INVOKABLE void play(const QString& key);

signals:
    void discoveryStatusChanged();
    // A message for a toast; `tone` is info, ready, standby or danger.
    void notice(const QString& tone, const QString& title, const QString& message);

private:
    using SteadyClock = std::chrono::steady_clock;

    void startDiscovery();
    void setDiscoveryStatus(DiscoveryStatus status);
    // Runs on chiaki's discovery threads.
    void postHosts(std::vector<remoteplay::DiscoveredHost> hosts);
    void onHostsDiscovered(std::vector<remoteplay::DiscoveredHost> hosts);
    void logChanges(const std::vector<remoteplay::DiscoveredHost>& hosts) const;
    void refreshModel();
    [[nodiscard]] ConsoleRow rowFor(const remoteplay::ConsoleEntry& entry,
                                    const QDateTime& now) const;
    void syncManualHosts();
    void markSearching(const std::string& key);
    // Ends a search unless it was started again since.
    void expireSearch(const std::string& key);

    [[nodiscard]] remoteplay::SavedConsole* findSaved(const QString& key);
    [[nodiscard]] const remoteplay::DiscoveredHost* findDiscovered(const QString& key) const;
    remoteplay::SavedConsole* saveDiscovered(const QString& key);
    [[nodiscard]] bool persist() const;
    void saveFile();

    ConsoleListModel model_;
    std::optional<remoteplay::ConsoleStore> store_;
    remoteplay::ConsoleFile file_;
    std::unique_ptr<remoteplay::ConsoleDiscovery> discovery_;
    std::vector<remoteplay::DiscoveredHost> discovered_;
    std::vector<remoteplay::ConsoleEntry> entries_;
    std::vector<std::string> queriedHosts_;
    std::map<std::string, SteadyClock::time_point> searchingUntil_;
    QTimer clockTimer_;
    DiscoveryStatus discoveryStatus_ = DiscoveryStatus::Unavailable;
    bool remotePlayReady_ = false;
};

} // namespace lynx::app
