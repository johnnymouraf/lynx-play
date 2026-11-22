#include "app/ConsolesController.h"

#include "app/ConsolePresentation.h"
#include "remoteplay/ConsoleDiscovery.h"
#include "remoteplay/ConsoleWake.h"
#include "remoteplay/HostAddress.h"

#include <algorithm>
#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

using namespace Qt::StringLiterals;
using remoteplay::Clock;
using remoteplay::ConsoleEntry;
using remoteplay::DiscoveredHost;
using remoteplay::HostAddressError;
using remoteplay::HostState;
using remoteplay::SavedConsole;

constexpr auto kSearchDuration = std::chrono::seconds(6);
constexpr auto kClockInterval = std::chrono::seconds(30);
constexpr auto kWakeTimeout = std::chrono::seconds(45);

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
                                       QObject* parent)
    : QObject(parent),
      model_(this),
      pairing_(*this, this),
      store_(std::move(store)),
      remotePlayReady_(remotePlayReady)
{
    if (store_)
    {
        file_ = store_->load();
    }

    clockTimer_.setInterval(kClockInterval);
    connect(&clockTimer_, &QTimer::timeout, this, &ConsolesController::refreshModel);
    clockTimer_.start();

    if (remotePlayReady_)
    {
        startDiscovery();
    }

    refreshModel();
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
    spdlog::info("Play requested on {}; streaming isn't available yet", key.toStdString());
    emit notice(u"info"_s, u"O streaming ainda não chegou"_s,
                u"Esta versão encontra, pareia e liga o PS5. Jogar chega na próxima versão."_s);
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
                    u"Veja o log em %LOCALAPPDATA%\\LynxPlay\\logs."_s);
        return;
    }

    spdlog::info("Wake-up sent to {} at {}", key.toStdString(), address);
    const std::string wakingKey = key.toStdString();
    wakingUntil_[wakingKey] = SteadyClock::now() + kWakeTimeout;
    QTimer::singleShot(kWakeTimeout, this, [this, wakingKey] { expireWake(wakingKey); });
    refreshModel();
}

void ConsolesController::cancelWake(const QString& key)
{
    if (wakingUntil_.erase(key.toStdString()) == 0)
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
        spdlog::info("{} is already listed", *address);
        emit notice(u"info"_s, u"Esse console já está na lista"_s,
                    u"O endereço %1 já é de um dos consoles."_s.arg(fromUtf8(*address)));
        return fromUtf8(existing->key);
    }

    SavedConsole console;
    console.manualHost = *address;
    const std::string key = remoteplay::consoleKey(console);
    file_.consoles.push_back(std::move(console));
    spdlog::info("Console added by address {}", *address);

    saveFile();
    markSearching(key);
    refreshModel();

    return fromUtf8(key);
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
    auto merged = remoteplay::mergeConsoles(std::move(file_.consoles), discovered_, Clock::now());
    file_.consoles = std::move(merged.saved);
    entries_ = std::move(merged.entries);

    if (merged.savedChanged)
    {
        saveFile();
    }

    syncManualHosts();

    const QDateTime now = QDateTime::currentDateTime();
    std::vector<ConsoleRow> rows;
    rows.reserve(entries_.size());

    for (const ConsoleEntry& entry : entries_)
    {
        rows.push_back(rowFor(entry, now));
    }

    model_.setRows(std::move(rows));
}

ConsoleRow ConsolesController::rowFor(const ConsoleEntry& entry, const QDateTime& now) const
{
    const auto searching = searchingUntil_.find(entry.key);
    const bool isSearching = searching != searchingUntil_.end() &&
                             searching->second > SteadyClock::now();

    const ConsoleFacts facts{
        .saved = entry.savedIndex ? &file_.consoles.at(*entry.savedIndex) : nullptr,
        .discovered = entry.discovered ? &*entry.discovered : nullptr,
        .waking = wakingUntil_.contains(entry.key),
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
    searchingUntil_[key] = SteadyClock::now() + kSearchDuration;

    QTimer::singleShot(kSearchDuration, this, [this, key] { expireSearch(key); });
}

void ConsolesController::expireSearch(const std::string& key)
{
    const auto searching = searchingUntil_.find(key);

    if (searching == searchingUntil_.end() || searching->second > SteadyClock::now())
    {
        return;
    }

    searchingUntil_.erase(searching);
    refreshModel();
}

void ConsolesController::expireWake(const std::string& key)
{
    const auto waking = wakingUntil_.find(key);

    if (waking == wakingUntil_.end() || waking->second > SteadyClock::now())
    {
        return;
    }

    wakingUntil_.erase(waking);
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
    for (auto waking = wakingUntil_.begin(); waking != wakingUntil_.end();)
    {
        const auto host = std::ranges::find(discovered_, waking->first, &DiscoveredHost::hostId);
        const bool ready = host != discovered_.end() && host->state == HostState::Ready;

        if (!ready)
        {
            ++waking;
            continue;
        }

        spdlog::info("{} turned on", waking->first);
        waking = wakingUntil_.erase(waking);
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
                    u"As mudanças valem até fechar o Lynx Play. Veja o log."_s);
    }
}

} // namespace lynx::app
