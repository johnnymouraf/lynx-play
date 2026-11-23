#pragma once

#include "app/ConsoleListModel.h"
#include "app/PairingController.h"
#include "remoteplay/Console.h"
#include "remoteplay/ConsoleMerge.h"
#include "remoteplay/ConsoleRegistration.h"
#include "remoteplay/ConsoleStore.h"

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQmlIntegration>

#include <cstdint>
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

class StreamController;

// The consoles the home screen lists, saved and answering, and the actions
// on them.
class ConsolesController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    Q_PROPERTY(ConsoleListModel* model READ model CONSTANT)
    Q_PROPERTY(DiscoveryStatus discoveryStatus READ discoveryStatus NOTIFY discoveryStatusChanged)
    Q_PROPERTY(PairingController* pairing READ pairing CONSTANT)
    Q_PROPERTY(QString savedAccountId READ savedAccountId NOTIFY savedAccountIdChanged)

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
                       StreamController& stream, PsnLoginClient& psnLogin,
                       QObject* parent = nullptr);
    ~ConsolesController() override;

    [[nodiscard]] ConsoleListModel* model();
    [[nodiscard]] DiscoveryStatus discoveryStatus() const;
    [[nodiscard]] PairingController* pairing();
    // The account ID last used to pair, in Base64, to offer it again.
    [[nodiscard]] QString savedAccountId() const;

    // Where and how to pair the console of a card.
    struct PairingTarget
    {
        std::string address;
        int target = 0;
    };
    [[nodiscard]] std::optional<PairingTarget> pairingTarget(const QString& key) const;
    // Saves what pairing gave and returns the console's card key from now on.
    QString storePairing(const QString& key, const remoteplay::PairedConsole& paired,
                         const remoteplay::AccountId& accountId);

    // Starts the network search again, after a failure or a network change.
    Q_INVOKABLE void retryDiscovery();
    // Shows the card as searching for a few seconds; the search itself never
    // stops.
    Q_INVOKABLE void refresh(const QString& key);
    // Saves a console that wasn't; an empty name shows the console's own.
    Q_INVOKABLE void rename(const QString& key, const QString& name);
    Q_INVOKABLE void forget(const QString& key);
    // Connects to a paired console and plays on it.
    Q_INVOKABLE void play(const QString& key);
    // Sends the wake-up packet to a paired console in rest mode; its card shows
    // it turning on until it answers ready or the wait runs out.
    Q_INVOKABLE void wake(const QString& key);
    // Stops waiting for a console being turned on.
    Q_INVOKABLE void cancelWake(const QString& key);
    // The message to show under the address field, or empty when it is valid.
    Q_INVOKABLE QString addressError(const QString& text) const;
    // Saves a console by its address and queries that address directly.
    // Returns the key of its card, which already existed for a known address.
    Q_INVOKABLE QString addByAddress(const QString& text);

signals:
    void discoveryStatusChanged();
    void savedAccountIdChanged();
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
    [[nodiscard]] ConsoleRow rowFor(const remoteplay::ConsoleEntry& entry,
                                    const QDateTime& now) const;
    void syncManualHosts();
    void markSearching(const std::string& key);
    // Ends a search unless it was started again since `timer` was set.
    void expireSearch(const std::string& key, std::uint64_t timer);
    // Gives up on a console that didn't turn on in time.
    void expireWake(const std::string& key, std::uint64_t timer);
    // Ends the wait for the consoles that now answer ready.
    void finishWakes();
    // Remembers when the console was last played on.
    void markPlayed(const QString& key);

    [[nodiscard]] remoteplay::SavedConsole* findSaved(const QString& key);
    [[nodiscard]] const remoteplay::DiscoveredHost* findDiscovered(const QString& key) const;
    [[nodiscard]] const remoteplay::ConsoleEntry* findEntry(const QString& key) const;
    [[nodiscard]] const remoteplay::ConsoleEntry* findEntryByAddress(
        const std::string& address) const;
    remoteplay::SavedConsole* saveDiscovered(const QString& key);
    [[nodiscard]] bool persist() const;
    void saveFile();

    ConsoleListModel model_;
    PairingController pairing_;
    StreamController& stream_;
    std::optional<remoteplay::ConsoleStore> store_;
    remoteplay::ConsoleFile file_;
    std::unique_ptr<remoteplay::ConsoleDiscovery> discovery_;
    std::vector<remoteplay::DiscoveredHost> discovered_;
    std::vector<remoteplay::ConsoleEntry> entries_;
    std::vector<std::string> queriedHosts_;
    // The consoles being searched for or turned on, with the timer that ends
    // the wait.
    std::map<std::string, std::uint64_t> searching_;
    std::map<std::string, std::uint64_t> waking_;
    std::uint64_t timerGeneration_ = 0;
    QTimer clockTimer_;
    DiscoveryStatus discoveryStatus_ = DiscoveryStatus::Unavailable;
    bool remotePlayReady_ = false;
};

} // namespace lynx::app
