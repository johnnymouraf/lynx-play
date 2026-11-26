#include "app/ConsolesController.h"

#include "app/ConsolePresentation.h"
#include "app/StreamController.h"
#include "remoteplay/ConsoleDiscovery.h"
#include "remoteplay/ConsoleWake.h"
#include "remoteplay/HostAddress.h"
#include "remoteplay/PsnPairing.h"
#include "remoteplay/PsnPolicy.h"

#include <QPointer>

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::app
{

namespace
{

using namespace Qt::StringLiterals;
using remoteplay::Clock;
using remoteplay::ConsoleEntry;
using remoteplay::DiscoveredHost;
using remoteplay::HostAddressError;
using remoteplay::HostState;
using remoteplay::PsnDevice;
using remoteplay::SavedConsole;

constexpr auto kSearchDuration = std::chrono::seconds(6);
constexpr auto kClockInterval = std::chrono::seconds(30);
constexpr auto kWakeTimeout = std::chrono::seconds(45);

// The card key of a PS5 the PSN lists and nothing here is paired with. It is
// the device's place in that list: the DUID stays out, since logs print keys.
QString psnCardKey(std::size_t index)
{
    return u"psn:%1"_s.arg(index);
}

ConsoleRow psnRow(std::size_t index, const PsnDevice& device)
{
    return ConsoleRow{
        .key = psnCardKey(index),
        .name = fromUtf8(device.name),
        .address = {},
        .status = u"unpaired"_s,
        .activity = u"Na sua conta PSN"_s,
        .saved = false,
        .paired = false,
        .psnReachable = true,
        .searching = false,
        .psnOnly = true,
    };
}

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

SavedConsole savedFrom(const DiscoveredHost& host)
{
    SavedConsole console;
    console.hostId = host.hostId;
    console.hostName = host.name;
    console.address = host.address;
    console.ps5 = host.ps5;
    console.target = host.target;
    console.systemVersion = host.systemVersion;
    console.lastSeen = Clock::now();
    return console;
}

// Whether the card's console answers at, was saved with, or was added by
// that address.
bool isAt(const ConsoleEntry& entry, const SavedConsole* saved, const std::string& address)
{
    const bool answersThere = entry.discovered && entry.discovered->address == address;
    const bool savedThere =
        saved != nullptr && (saved->manualHost == address || saved->address == address);

    return answersThere || savedThere;
}

std::expected<void, std::string> sendWakeupTo(const SavedConsole& console,
                                              const std::string& address)
{
    const auto credential = remoteplay::wakeCredential(console.credentials->registKey);

    if (!credential)
    {
        return std::unexpected(credential.error());
    }

    return remoteplay::sendWakeup(address, *credential, console.ps5);
}

} // namespace

ConsolesController::ConsolesController(bool remotePlayReady,
                                       std::optional<remoteplay::ConsoleStore> store,
                                       StreamController& stream, PsnLoginClient& psnLogin,
                                       QObject* parent)
    : QObject(parent),
      model_(this),
      pairing_(*this, psnLogin, this),
      stream_(stream),
      psnLogin_(psnLogin),
      store_(std::move(store)),
      remotePlayReady_(remotePlayReady)
{
    if (store_)
    {
        file_ = store_->load();
    }

    connect(&stream_, &StreamController::streamStarted, this, &ConsolesController::markPlayed);
    connect(&psnLogin_, &PsnLoginClient::accessTokenChanged, this,
            &ConsolesController::refreshPsnDevices);
    connect(&psnLogin_, &PsnLoginClient::accessTokenChanged, this,
            &ConsolesController::psnSignedInChanged);
    connect(&psnClient_, &PsnDevicesClient::devicesListed, this,
            &ConsolesController::onPsnDevicesListed);

    stream_.setAccessTokenSource([this] { return psnLogin_.validAccessToken(); });

    clockTimer_.setInterval(kClockInterval);
    connect(&clockTimer_, &QTimer::timeout, this, &ConsolesController::refreshModel);
    clockTimer_.start();

    if (remotePlayReady_)
    {
        startDiscovery();
    }

    refreshModel();
    refreshPsnDevices();
}

ConsolesController::~ConsolesController()
{
    discovery_.reset();

    if (!persist())
    {
        spdlog::error("The consoles' last-seen times weren't saved");
    }
}

ConsoleListModel* ConsolesController::model()
{
    return &model_;
}

ConsolesController::DiscoveryStatus ConsolesController::discoveryStatus() const
{
    return discoveryStatus_;
}

PairingController* ConsolesController::pairing()
{
    return &pairing_;
}

QString ConsolesController::savedAccountId() const
{
    if (!file_.accountId)
    {
        return {};
    }

    return fromUtf8(remoteplay::formatAccountId(*file_.accountId));
}

std::optional<ConsolesController::PairingTarget> ConsolesController::pairingTarget(
    const QString& key) const
{
    const ConsoleEntry* entry = findEntry(key);

    if (entry == nullptr)
    {
        return std::nullopt;
    }

    if (entry->discovered)
    {
        return PairingTarget{.address = entry->discovered->address,
                             .target = entry->discovered->target};
    }

    if (!entry->savedIndex)
    {
        return std::nullopt;
    }

    const SavedConsole& saved = file_.consoles.at(*entry->savedIndex);
    const std::string& address = saved.address.empty() ? saved.manualHost : saved.address;
    return PairingTarget{.address = address, .target = saved.target};
}

QString ConsolesController::storePairing(const QString& key,
                                         const remoteplay::PairedConsole& paired,
                                         const remoteplay::AccountId& accountId)
{
    SavedConsole* console = findSaved(key);

    if (console == nullptr)
    {
        console = saveDiscovered(key);
    }

    if (console == nullptr)
    {
        console = &file_.consoles.emplace_back();
    }

    if (console->hostId.empty())
    {
        console->hostId = remoteplay::hostIdFromMac(paired.serverMac);
    }

    if (console->hostName.empty())
    {
        console->hostName = paired.serverNickname;
    }

    if (paired.target != 0)
    {
        console->target = paired.target;
    }

    console->credentials = paired.credentials;
    console->serverMac = paired.serverMac;
    const std::string pairedKey = remoteplay::consoleKey(*console);

    file_.accountId = accountId;
    emit savedAccountIdChanged();
    saveFile();
    refreshModel();

    return fromUtf8(pairedKey);
}

void ConsolesController::retryDiscovery()
{
    if (!remotePlayReady_)
    {
        return;
    }

    discovery_.reset();
    discovered_.clear();
    startDiscovery();
    refreshModel();
}

void ConsolesController::refresh(const QString& key)
{
    markSearching(key.toStdString());
    refreshModel();
}

void ConsolesController::rename(const QString& key, const QString& name)
{
    SavedConsole* console = findSaved(key);

    if (console == nullptr)
    {
        console = saveDiscovered(key);
    }

    if (console == nullptr)
    {
        spdlog::warn("Cannot rename console {}: it's gone", key.toStdString());
        return;
    }

    console->nickname = name.trimmed().toStdString();
    saveFile();
    refreshModel();
}

void ConsolesController::forget(const QString& key)
{
    const std::string wanted = key.toStdString();
    const auto removed = std::erase_if(file_.consoles, [&wanted](const SavedConsole& console) {
        return remoteplay::consoleKey(console) == wanted;
    });

    if (removed == 0)
    {
        return;
    }

    spdlog::info("Console {} forgotten", wanted);
    saveFile();
    refreshModel();
}

void ConsolesController::play(const QString& key)
{
    route(key, false);
}

void ConsolesController::route(const QString& key, bool waited)
{
    endLocalWait(key.toStdString());

    const SavedConsole* console = findSaved(key);
    const remoteplay::PlayFacts facts{
        .answersLocally = answersOnLan(key),
        .waited = waited,
        .hasRemoteAddress = console != nullptr && !console->remoteHost.empty(),
        .answersRemotely = answersAtRemote(key),
        .internetReady = psnTargetFor(key).has_value(),
    };

    switch (remoteplay::playRouteFor(facts))
    {
    case remoteplay::PlayRoute::Local:
        playLocal(key);
        break;
    case remoteplay::PlayRoute::WaitForLocal:
        waitForLocal(key);
        break;
    case remoteplay::PlayRoute::Remote:
        startRemote(key);
        break;
    case remoteplay::PlayRoute::Internet:
        startInternet(key, true);
        break;
    }

    refreshModel();
}

void ConsolesController::waitForLocal(const QString& key)
{
    const std::string wanted = key.toStdString();
    const std::uint64_t timer = ++timerGeneration_;
    localWaits_[wanted] = timer;
    markSearching(wanted);
    spdlog::info("{} doesn't answer on the local network yet; waiting for it", wanted);

    QTimer::singleShot(remoteplay::kLocalAnswerWait, this,
                       [this, wanted, timer] { expireLocalWait(wanted, timer); });
}

void ConsolesController::endLocalWait(const std::string& key)
{
    localWaits_.erase(key);
    searching_.erase(key);
}

void ConsolesController::expireLocalWait(const std::string& key, std::uint64_t timer)
{
    const auto wait = localWaits_.find(key);

    if (wait == localWaits_.end() || wait->second != timer)
    {
        return;
    }

    spdlog::info("{} didn't answer on the local network in time", key);
    route(fromUtf8(key), true);
}

void ConsolesController::finishLocalWaits()
{
    std::vector<QString> answered;

    for (const auto& wait : localWaits_)
    {
        if (answersOnLan(fromUtf8(wait.first)))
        {
            answered.push_back(fromUtf8(wait.first));
        }
    }

    for (const QString& key : answered)
    {
        spdlog::info("{} answers on the local network now", key.toStdString());
        route(key, false);
    }
}

const DiscoveredHost* ConsolesController::localAnswer(const QString& key)
{
    const std::string wanted = key.toStdString();
    const SavedConsole* console = findSaved(key);
    const std::string remoteHost = console != nullptr ? console->remoteHost : std::string();

    for (const DiscoveredHost& host : discovered_)
    {
        if (host.hostId == wanted && !remoteplay::isRemoteAnswer(host, remoteHost))
        {
            return &host;
        }
    }

    return nullptr;
}

bool ConsolesController::answersOnLan(const QString& key)
{
    const DiscoveredHost* host = localAnswer(key);
    return host != nullptr && host->state == HostState::Ready;
}

bool ConsolesController::answersAtRemote(const QString& key)
{
    const SavedConsole* console = findSaved(key);

    if (console == nullptr)
    {
        return false;
    }

    const std::string wanted = key.toStdString();

    return std::ranges::any_of(discovered_, [&wanted, console](const DiscoveredHost& host) {
        return host.hostId == wanted && host.state == HostState::Ready &&
               remoteplay::isRemoteAnswer(host, console->remoteHost);
    });
}

void ConsolesController::playLocal(const QString& key)
{
    const SavedConsole* console = findSaved(key);
    const DiscoveredHost* host = localAnswer(key);

    StreamController::Target target{
        .key = key,
        .name = consoleName({.saved = console, .discovered = host}),
        .host = {},
        .ps5 = true,
        .credentials = std::nullopt,
    };

    if (console != nullptr)
    {
        target.host = console->address.empty() ? console->manualHost : console->address;
        target.ps5 = console->ps5;
        target.credentials = console->credentials;
    }

    if (host != nullptr)
    {
        target.host = host->address;
    }

    stream_.play(std::move(target));
}

bool ConsolesController::psnSignedIn() const
{
    return !psnLogin_.validAccessToken().empty();
}

bool ConsolesController::playOverInternet(const QString& key)
{
    // The explicit choice ends a wait for the local network, if one runs.
    endLocalWait(key.toStdString());
    refreshModel();
    return startInternet(key, false);
}

void ConsolesController::pairOverInternet(const QString& key)
{
    const PsnDevice* device = findUnpairedPsn(key);

    if (device == nullptr)
    {
        spdlog::warn("Cannot pair {}: the PSN doesn't list it as unpaired",
                     key.toStdString());
        return;
    }

    const std::string token = psnLogin_.validAccessToken();
    const std::optional<remoteplay::AccountId> psnAccount = psnAccountId();

    if (token.empty() || !psnAccount)
    {
        spdlog::warn("Cannot pair {} over the internet: no valid PSN sign-in",
                     key.toStdString());
        emit notice(u"danger"_s, u"Entre na PSN para parear pela internet"_s,
                    u"Entre na PSN em Rede e tente de novo."_s);
        return;
    }

    spdlog::info("Pairing {} over the internet", key.toStdString());
    stream_.pairOverInternet(
        {
            .key = key,
            .name = fromUtf8(device->name),
            .host = {},
            .ps5 = true,
            .credentials = std::nullopt,
            .psn = remoteplay::PsnTarget{
                .duid = device->duid,
                .accessToken = token,
                .accountId = *psnAccount,
            },
        },
        [self = QPointer<ConsolesController>(this), deviceName = device->name,
         accountId = *psnAccount](const remoteplay::PairedConsole& paired)
        {
            if (self)
            {
                self->storeRegistration(paired, deviceName, accountId);
            }
        });
}

void ConsolesController::storeRegistration(const remoteplay::PairedConsole& paired,
                                           const std::string& deviceName,
                                           const remoteplay::AccountId& accountId)
{
    const std::string hostId = remoteplay::hostIdFromMac(paired.serverMac);
    const auto saved = std::ranges::find(file_.consoles, hostId, &SavedConsole::hostId);

    if (saved != file_.consoles.end())
    {
        *saved = remoteplay::withPsnRegistration(std::move(*saved), paired, deviceName);
    }
    else
    {
        file_.consoles.push_back(
            remoteplay::withPsnRegistration(SavedConsole{}, paired, deviceName));
    }

    file_.accountId = accountId;
    emit savedAccountIdChanged();
    saveFile();
    refreshModel();

    emit notice(u"ready"_s, u"%1 pareado"_s.arg(fromUtf8(deviceName)),
                u"Ele já aparece na lista de consoles e pode ser jogado pela internet."_s);
}

std::optional<remoteplay::AccountId> ConsolesController::psnAccountId() const
{
    // The sign-in's token lists the devices of its own account, so its ID wins.
    const auto signedIn = remoteplay::parseAccountId(psnLogin_.accountId().toStdString());

    if (signedIn)
    {
        return *signedIn;
    }

    return file_.accountId;
}

const PsnDevice* ConsolesController::findUnpairedPsn(const QString& key) const
{
    for (std::size_t index = 0; index < unpairedPsn_.size(); ++index)
    {
        if (psnCardKey(index) == key)
        {
            return &unpairedPsn_[index];
        }
    }

    return nullptr;
}

std::optional<remoteplay::PsnTarget> ConsolesController::psnTargetFor(const QString& key)
{
    const SavedConsole* console = findSaved(key);
    const std::optional<std::string> duid = psnDuid(key);
    std::string token = psnLogin_.validAccessToken();

    const std::optional<remoteplay::AccountId> accountId = psnAccountId();

    if (console == nullptr || !console->credentials || !duid || token.empty() || !accountId)
    {
        return std::nullopt;
    }

    return remoteplay::PsnTarget{
        .duid = *duid,
        .accessToken = std::move(token),
        .accountId = *accountId,
    };
}

bool ConsolesController::startInternet(const QString& key, bool fallback)
{
    const SavedConsole* console = findSaved(key);
    std::optional<remoteplay::PsnTarget> psn = psnTargetFor(key);

    if (console == nullptr || !psn)
    {
        spdlog::warn("Cannot play over the internet on {}: the console isn't ready for it",
                     key.toStdString());
        return false;
    }

    stream_.play({
        .key = key,
        .name = consoleName({.saved = console, .discovered = nullptr}),
        .host = {},
        .ps5 = console->ps5,
        .credentials = console->credentials,
        .psn = std::move(psn),
        .internetFallback = fallback,
    });

    return true;
}

void ConsolesController::startRemote(const QString& key)
{
    const SavedConsole* console = findSaved(key);

    if (console == nullptr)
    {
        return;
    }

    stream_.play({
        .key = key,
        .name = consoleName({.saved = console, .discovered = nullptr}),
        .host = console->remoteHost,
        .ps5 = console->ps5,
        .credentials = console->credentials,
        .remote = true,
        .internetFallback = true,
    });
}

void ConsolesController::markPlayed(const QString& key)
{
    SavedConsole* console = findSaved(key);

    if (console == nullptr)
    {
        return;
    }

    console->lastPlayed = Clock::now();
    saveFile();
    refreshModel();
}

void ConsolesController::wake(const QString& key)
{
    const SavedConsole* console = findSaved(key);

    if (console == nullptr || !console->credentials)
    {
        spdlog::warn("Cannot wake {}: it isn't paired", key.toStdString());
        return;
    }

    const DiscoveredHost* host = findDiscovered(key);
    const std::string& address = host != nullptr ? host->address : console->address;
    const QString name = consoleName({.saved = console, .discovered = host});
    const auto sent = sendWakeupTo(*console, address);

    if (!sent)
    {
        spdlog::error("Cannot wake {}: {}", key.toStdString(), sent.error());
        emit notice(u"danger"_s, u"Não foi possível ligar o %1"_s.arg(name),
                    u"Veja o log em %LOCALAPPDATA%\\MFPlay\\logs."_s);
        return;
    }

    spdlog::info("Wake-up sent to {}", key.toStdString());
    const std::string wakingKey = key.toStdString();
    const std::uint64_t timer = ++timerGeneration_;
    waking_[wakingKey] = timer;
    QTimer::singleShot(kWakeTimeout, this,
                       [this, wakingKey, timer] { expireWake(wakingKey, timer); });
    refreshModel();
}

void ConsolesController::cancelWake(const QString& key)
{
    if (waking_.erase(key.toStdString()) == 0)
    {
        return;
    }

    spdlog::info("Stopped waiting for {} to turn on", key.toStdString());
    refreshModel();
}

QString ConsolesController::addressError(const QString& text) const
{
    const auto address = remoteplay::normalizeHostAddress(text.toStdString());

    if (address)
    {
        return {};
    }

    switch (address.error())
    {
    case HostAddressError::Empty:
        return u"Informe o endereço IP ou o nome do host do PS5."_s;
    case HostAddressError::Ipv4PartOutOfRange:
        return u"Endereço inválido: cada parte do IP vai de 0 a 255. Use um IP como "
               u"192.168.0.25 ou um nome como ps5.local."_s;
    case HostAddressError::Invalid:
        break;
    }

    return u"Endereço inválido. Use um IP como 192.168.0.25 ou um nome como ps5.local."_s;
}

QString ConsolesController::addByAddress(const QString& text)
{
    const auto address = remoteplay::normalizeHostAddress(text.toStdString());

    if (!address)
    {
        return {};
    }

    if (const ConsoleEntry* existing = findEntryByAddress(*address); existing != nullptr)
    {
        spdlog::info("The typed address is already listed");
        emit notice(u"info"_s, u"Esse console já está na lista"_s,
                    u"O endereço %1 já é de um dos consoles."_s.arg(fromUtf8(*address)));
        return fromUtf8(existing->key);
    }

    SavedConsole console;
    console.manualHost = *address;
    const std::string key = remoteplay::consoleKey(console);
    file_.consoles.push_back(std::move(console));
    spdlog::info("Console added by a typed address");

    saveFile();
    markSearching(key);
    refreshModel();

    return fromUtf8(key);
}

bool ConsolesController::setRemoteAddress(const QString& key, const QString& text)
{
    SavedConsole* console = findSaved(key);

    if (console == nullptr)
    {
        spdlog::warn("Cannot set the remote address of {}: it isn't saved", key.toStdString());
        return false;
    }

    const std::string typed = text.trimmed().toStdString();
    std::string remoteHost;

    if (!typed.empty())
    {
        const auto address = remoteplay::normalizeHostAddress(typed);

        if (!address)
        {
            return false;
        }

        remoteHost = *address;
    }

    // The address itself stays out of the logs: it may not be on the local network.
    spdlog::info("Remote address of {} {}", key.toStdString(),
                 remoteHost.empty() ? "removed" : "saved");
    console->remoteHost = std::move(remoteHost);
    saveFile();
    refreshModel();

    return true;
}

QString ConsolesController::remoteAddressOf(const QString& key)
{
    const SavedConsole* console = findSaved(key);

    if (console == nullptr)
    {
        return {};
    }

    return fromUtf8(console->remoteHost);
}

void ConsolesController::startDiscovery()
{
    queriedHosts_.clear();

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
    finishWakes();
    finishLocalWaits();
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
            // Only a broadcast names its address: a typed one may not be on the local network.
            const std::string answer =
                host.queriedHost.empty() ? "from " + host.address : "at a typed address";
            spdlog::info("Console {} ({}) answers {}, {}", host.name, host.hostId, answer,
                         stateName(host.state));
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

void ConsolesController::refreshPsnDevices()
{
    psnClient_.refresh(psnLogin_.validAccessToken());
}

void ConsolesController::onPsnDevicesListed(std::vector<remoteplay::PsnDevice> devices)
{
    psnDevices_ = std::move(devices);
    refreshModel();

    spdlog::info("PSN lists {} PS5, {} match a saved console", psnDevices_.size(),
                 psnMatches_.size());
}

std::optional<std::string> ConsolesController::psnDuid(const QString& key) const
{
    const std::string wanted = key.toStdString();
    const auto match = std::ranges::find(psnMatches_, wanted, &remoteplay::PsnMatch::consoleKey);

    if (match == psnMatches_.end())
    {
        return std::nullopt;
    }

    return match->duid;
}

void ConsolesController::refreshModel()
{
    auto merged = remoteplay::mergeConsoles(std::move(file_.consoles), discovered_, Clock::now());
    file_.consoles = std::move(merged.saved);
    entries_ = std::move(merged.entries);

    if (merged.savedChanged)
    {
        saveFile();
    }

    syncManualHosts();
    psnMatches_ = remoteplay::matchPsnDevices(psnDevices_, file_.consoles);
    unpairedPsn_ = remoteplay::unpairedPsnDevices(psnDevices_, file_.consoles, discovered_);

    const QDateTime now = QDateTime::currentDateTime();
    std::vector<ConsoleRow> rows;
    rows.reserve(entries_.size());

    for (const ConsoleEntry& entry : entries_)
    {
        rows.push_back(rowFor(entry, now));
    }

    for (std::size_t index = 0; index < unpairedPsn_.size(); ++index)
    {
        rows.push_back(psnRow(index, unpairedPsn_[index]));
    }

    model_.setRows(std::move(rows));
}

ConsoleRow ConsolesController::rowFor(const ConsoleEntry& entry, const QDateTime& now) const
{
    const bool isSearching = searching_.contains(entry.key);

    const ConsoleFacts facts{
        .saved = entry.savedIndex ? &file_.consoles.at(*entry.savedIndex) : nullptr,
        .discovered = entry.discovered ? &*entry.discovered : nullptr,
        .waking = waking_.contains(entry.key),
        .searching = isSearching,
    };

    return ConsoleRow{
        .key = fromUtf8(entry.key),
        .name = consoleName(facts),
        .address = consoleAddress(facts),
        .status = consoleStatus(facts),
        .activity = consoleActivity(facts, now),
        .saved = facts.saved != nullptr,
        .paired = facts.saved != nullptr && facts.saved->credentials.has_value(),
        .psnReachable = psnDuid(fromUtf8(entry.key)).has_value(),
        .searching = isSearching && facts.discovered == nullptr,
    };
}

void ConsolesController::syncManualHosts()
{
    if (discovery_ == nullptr)
    {
        return;
    }

    std::vector<std::string> hosts;

    for (const SavedConsole& console : file_.consoles)
    {
        if (!console.manualHost.empty())
        {
            hosts.push_back(console.manualHost);
        }

        if (!console.remoteHost.empty())
        {
            hosts.push_back(console.remoteHost);
        }
    }

    if (hosts == queriedHosts_)
    {
        return;
    }

    queriedHosts_ = hosts;
    discovery_->setManualHosts(hosts);
}

void ConsolesController::markSearching(const std::string& key)
{
    const std::uint64_t timer = ++timerGeneration_;
    searching_[key] = timer;

    QTimer::singleShot(kSearchDuration, this, [this, key, timer] { expireSearch(key, timer); });
}

void ConsolesController::expireSearch(const std::string& key, std::uint64_t timer)
{
    const auto searching = searching_.find(key);

    if (searching == searching_.end() || searching->second != timer)
    {
        return;
    }

    searching_.erase(searching);
    refreshModel();
}

void ConsolesController::expireWake(const std::string& key, std::uint64_t timer)
{
    const auto waking = waking_.find(key);

    if (waking == waking_.end() || waking->second != timer)
    {
        return;
    }

    waking_.erase(waking);
    spdlog::warn("{} didn't turn on in time", key);

    const ConsoleRow* row = model_.find(fromUtf8(key));
    const QString name = row != nullptr ? row->name : fromUtf8(key);
    emit notice(u"danger"_s, u"O %1 não ligou"_s.arg(name),
                u"Confira se ele pode ser ligado pela rede: Configurações › Sistema › Economia de "
                u"energia › Recursos disponíveis no modo de repouso."_s);
    refreshModel();
}

void ConsolesController::finishWakes()
{
    for (auto waking = waking_.begin(); waking != waking_.end();)
    {
        const auto host = std::ranges::find(discovered_, waking->first, &DiscoveredHost::hostId);
        const bool ready = host != discovered_.end() && host->state == HostState::Ready;

        if (!ready)
        {
            ++waking;
            continue;
        }

        spdlog::info("{} turned on", waking->first);
        waking = waking_.erase(waking);
    }
}

SavedConsole* ConsolesController::findSaved(const QString& key)
{
    const std::string wanted = key.toStdString();
    const auto console = std::ranges::find_if(file_.consoles, [&wanted](const SavedConsole& item) {
        return remoteplay::consoleKey(item) == wanted;
    });

    return console != file_.consoles.end() ? &*console : nullptr;
}

const DiscoveredHost* ConsolesController::findDiscovered(const QString& key) const
{
    const auto host = std::ranges::find(discovered_, key.toStdString(), &DiscoveredHost::hostId);
    return host != discovered_.end() ? &*host : nullptr;
}

const ConsoleEntry* ConsolesController::findEntry(const QString& key) const
{
    const auto entry = std::ranges::find(entries_, key.toStdString(), &ConsoleEntry::key);
    return entry != entries_.end() ? &*entry : nullptr;
}

const ConsoleEntry* ConsolesController::findEntryByAddress(const std::string& address) const
{
    for (const ConsoleEntry& entry : entries_)
    {
        const SavedConsole* saved =
            entry.savedIndex ? &file_.consoles.at(*entry.savedIndex) : nullptr;

        if (isAt(entry, saved, address))
        {
            return &entry;
        }
    }

    return nullptr;
}

SavedConsole* ConsolesController::saveDiscovered(const QString& key)
{
    const DiscoveredHost* host = findDiscovered(key);

    if (host == nullptr)
    {
        return nullptr;
    }

    file_.consoles.push_back(savedFrom(*host));
    return &file_.consoles.back();
}

bool ConsolesController::persist() const
{
    if (!store_)
    {
        return true;
    }

    if (const auto saved = store_->save(file_); !saved)
    {
        spdlog::error("Cannot save the consoles: {}", saved.error());
        return false;
    }

    return true;
}

void ConsolesController::saveFile()
{
    if (!persist())
    {
        emit notice(u"danger"_s, u"Não foi possível salvar os consoles"_s,
                    u"As mudanças valem até fechar o MFPlay. Veja o log."_s);
    }
}

} // namespace mfplay::app
