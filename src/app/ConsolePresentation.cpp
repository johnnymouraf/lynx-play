#include "app/ConsolePresentation.h"

#include <chrono>

namespace lynx::app
{

namespace
{

using namespace Qt::StringLiterals;
using remoteplay::HostState;

constexpr qint64 kSecondsPerMinute = 60;
constexpr qint64 kRecentMinutes = 2;
constexpr qint64 kMinutesPerHour = 60;

QDateTime toDateTime(remoteplay::Clock::time_point time)
{
    const auto sinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(
        time.time_since_epoch());
    return QDateTime::fromMSecsSinceEpoch(sinceEpoch.count());
}

bool isPaired(const ConsoleFacts& facts)
{
    return facts.saved != nullptr && facts.saved->credentials.has_value();
}

// "hoje às 21:40", "ontem às 23:15" or "em 12/11/2026".
QString dayAndTime(const QDateTime& when, const QDateTime& now)
{
    const qint64 days = when.date().daysTo(now.date());
    const QString time = when.toString(u"HH:mm"_s);

    if (days == 0)
    {
        return u"hoje às %1"_s.arg(time);
    }

    if (days == 1)
    {
        return u"ontem às %1"_s.arg(time);
    }

    return u"em %1"_s.arg(when.toString(u"dd/MM/yyyy"_s));
}

QString timeSince(const QDateTime& when, const QDateTime& now)
{
    const qint64 minutes = when.secsTo(now) / kSecondsPerMinute;

    if (minutes < kRecentMinutes)
    {
        return u"agora há pouco"_s;
    }

    if (minutes < kMinutesPerHour)
    {
        return u"há %1 minutos"_s.arg(minutes);
    }

    const qint64 days = when.date().daysTo(now.date());

    if (days <= 1)
    {
        return dayAndTime(when, now);
    }

    return u"há %1 dias"_s.arg(days);
}

QString answeringActivity(const ConsoleFacts& facts)
{
    if (isPaired(facts))
    {
        return u"Pareado com este PC"_s;
    }

    if (facts.discovered->state == HostState::Standby)
    {
        return u"Em repouso. Ligue o PS5 para parear."_s;
    }

    return u"Encontrado agora na rede"_s;
}

} // namespace

QString consoleStatus(const ConsoleFacts& facts)
{
    if (facts.waking)
    {
        return u"waking"_s;
    }

    if (facts.discovered == nullptr)
    {
        return u"offline"_s;
    }

    if (!isPaired(facts))
    {
        return u"unpaired"_s;
    }

    if (facts.discovered->state == HostState::Standby)
    {
        return u"standby"_s;
    }

    return u"ready"_s;
}

QString consoleActivity(const ConsoleFacts& facts, const QDateTime& now)
{
    if (facts.saved != nullptr && facts.saved->lastPlayed)
    {
        return u"Jogado %1"_s.arg(dayAndTime(toDateTime(*facts.saved->lastPlayed), now));
    }

    if (facts.discovered != nullptr)
    {
        return answeringActivity(facts);
    }

    if (facts.searching)
    {
        return u"Procurando neste endereço…"_s;
    }

    if (facts.saved != nullptr && facts.saved->lastSeen)
    {
        const QDateTime lastSeen = toDateTime(*facts.saved->lastSeen);
        return u"Visto pela última vez %1"_s.arg(timeSince(lastSeen, now));
    }

    return u"Ainda não respondeu"_s;
}

QString consoleName(const ConsoleFacts& facts)
{
    if (facts.saved != nullptr && !facts.saved->nickname.empty())
    {
        return fromUtf8(facts.saved->nickname);
    }

    if (facts.discovered != nullptr && !facts.discovered->name.empty())
    {
        return fromUtf8(facts.discovered->name);
    }

    if (facts.saved != nullptr && !facts.saved->hostName.empty())
    {
        return fromUtf8(facts.saved->hostName);
    }

    return consoleAddress(facts);
}

QString consoleAddress(const ConsoleFacts& facts)
{
    if (facts.discovered != nullptr)
    {
        return fromUtf8(facts.discovered->address);
    }

    if (facts.saved == nullptr)
    {
        return {};
    }

    if (!facts.saved->address.empty())
    {
        return fromUtf8(facts.saved->address);
    }

    return fromUtf8(facts.saved->manualHost);
}

QString fromUtf8(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace lynx::app
