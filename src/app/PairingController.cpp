#include "app/PairingController.h"

#include "app/ConsolesController.h"
#include "remoteplay/Pin.h"

#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

using namespace Qt::StringLiterals;
using remoteplay::AccountIdError;
using remoteplay::PairingFailure;

// chiaki gives up on its own within seconds; this only catches a hang.
constexpr auto kPairingTimeout = std::chrono::seconds(30);

struct FailureText
{
    QString title;
    QString message;
};

FailureText failureText(PairingFailure failure)
{
    switch (failure)
    {
    case PairingFailure::ConsoleNotFound:
        return {u"O PS5 não respondeu"_s,
                u"Confira se ele está ligado, na mesma rede e com a tela Vincular dispositivo "
                u"aberta."_s};
    case PairingFailure::NotInLinkScreen:
        return {u"O PS5 não está pronto para parear"_s,
                u"Abra Configurações › Sistema › Reprodução remota › Vincular dispositivo no PS5 "
                u"e tente de novo."_s};
    case PairingFailure::WrongPin:
        return {u"O PS5 recusou o código"_s,
                u"Confira os 8 dígitos na TV. Se o código expirou, gere outro em Vincular "
                u"dispositivo."_s};
    case PairingFailure::InvalidAccountId:
        return {u"O PS5 não reconheceu o Account ID"_s,
                u"Confira se é o Account ID da conta PSN usada no PS5."_s};
    case PairingFailure::Rejected:
        return {u"O PS5 recusou o pareamento"_s,
                u"Ele pode já ter o máximo de dispositivos vinculados. Remova um em Reprodução "
                u"remota no PS5 e tente de novo."_s};
    case PairingFailure::Unknown:
        break;
    }

    return {u"Algo deu errado no pareamento"_s,
            u"Tente de novo. Se continuar, veja o log em %LOCALAPPDATA%\\LynxPlay\\logs."_s};
}

std::string_view failureName(PairingFailure failure)
{
    switch (failure)
    {
    case PairingFailure::ConsoleNotFound:
        return "the console didn't answer";
    case PairingFailure::NotInLinkScreen:
        return "the console isn't on its link screen";
    case PairingFailure::WrongPin:
        return "wrong PIN";
    case PairingFailure::InvalidAccountId:
        return "account ID not recognized";
    case PairingFailure::Rejected:
        return "the console refused";
    case PairingFailure::Unknown:
        break;
    }

    return "unknown reason";
}

} // namespace

PairingController::PairingController(ConsolesController& consoles, PsnLoginClient& psnLogin,
                                     QObject* parent)
    : QObject(parent), consoles_(consoles), psnLogin_(psnLogin)
{
    watchdog_.setSingleShot(true);
    watchdog_.setInterval(kPairingTimeout);
    connect(&watchdog_, &QTimer::timeout, this, &PairingController::onTimeout);
}

PairingController::~PairingController() = default;

PairingController::State PairingController::state() const
{
    return state_;
}

QString PairingController::failureTitle() const
{
    return failureText(failure_).title;
}

QString PairingController::failureMessage() const
{
    return failureText(failure_).message;
}

QString PairingController::retryStep() const
{
    switch (failure_)
    {
    case PairingFailure::WrongPin:
        return u"pin"_s;
    case PairingFailure::InvalidAccountId:
        return u"account"_s;
    default:
        return {};
    }
}

QString PairingController::pairedKey() const
{
    return pairedKey_;
}

PsnLoginClient* PairingController::psnLogin()
{
    return &psnLogin_;
}

QString PairingController::accountIdError(const QString& text) const
{
    const auto accountId = remoteplay::parseAccountId(text.toStdString());

    if (accountId)
    {
        return {};
    }

    switch (accountId.error())
    {
    case AccountIdError::Empty:
        return u"Informe o Account ID da sua conta PSN."_s;
    case AccountIdError::InvalidNumber:
        return u"Esse número não é um Account ID válido."_s;
    case AccountIdError::InvalidFormat:
        break;
    }

    return u"Account ID inválido. Use o número da conta ou o formato Base64, como "
           u"Kx3fQ9aZ0bM=."_s;
}

QString PairingController::pinError(const QString& text) const
{
    if (remoteplay::parsePin(text.toStdString()))
    {
        return {};
    }

    return u"Digite os 8 dígitos mostrados na TV."_s;
}

void PairingController::start(const QString& key, const QString& accountId, const QString& pin)
{
    const auto parsedAccountId = remoteplay::parseAccountId(accountId.toStdString());
    const auto parsedPin = remoteplay::parsePin(pin.toStdString());
    const auto target = consoles_.pairingTarget(key);

    if (!parsedAccountId || !parsedPin || !target)
    {
        spdlog::error("Pairing with {} can't start: invalid input or unknown console",
                      key.toStdString());
        fail(PairingFailure::Unknown);
        return;
    }

    cancel();
    key_ = key;
    accountId_ = *parsedAccountId;
    const std::uint64_t attempt = ++attempt_;

    auto registration = remoteplay::ConsoleRegistration::start(
        {
            .host = target->address,
            .accountId = *parsedAccountId,
            .pin = *parsedPin,
            .target = target->target,
        },
        [this, attempt](remoteplay::PairingResult result) {
            postResult(attempt, std::move(result));
        });

    if (!registration)
    {
        spdlog::error("Pairing with {} can't start: {}", key.toStdString(), registration.error());
        fail(PairingFailure::Unknown);
        return;
    }

    spdlog::info("Pairing with {} at {}", key.toStdString(), target->address);
    registration_ = std::move(*registration);
    watchdog_.start();
    setState(State::Pairing);
}

void PairingController::cancel()
{
    ++attempt_;
    watchdog_.stop();

    if (registration_ != nullptr)
    {
        spdlog::info("Pairing canceled");
        registration_.reset();
    }

    setState(State::Idle);
}

void PairingController::reset()
{
    cancel();
    pairedKey_.clear();
    failure_ = PairingFailure::Unknown;
    emit stateChanged();
}

void PairingController::postResult(std::uint64_t attempt, remoteplay::PairingResult result)
{
    QMetaObject::invokeMethod(
        this, [this, attempt, result = std::move(result)] { onResult(attempt, result); },
        Qt::QueuedConnection);
}

void PairingController::onResult(std::uint64_t attempt, const remoteplay::PairingResult& result)
{
    if (attempt != attempt_)
    {
        return;
    }

    watchdog_.stop();
    registration_.reset();

    if (!result)
    {
        fail(result.error());
        return;
    }

    spdlog::info("Paired with {}", key_.toStdString());
    pairedKey_ = consoles_.storePairing(key_, *result, accountId_);
    setState(State::Succeeded);
}

void PairingController::onTimeout()
{
    spdlog::warn("Pairing with {} took too long", key_.toStdString());
    ++attempt_;
    registration_.reset();
    fail(PairingFailure::ConsoleNotFound);
}

void PairingController::fail(PairingFailure failure)
{
    spdlog::warn("Pairing failed: {}", failureName(failure));
    failure_ = failure;
    state_ = State::Failed;
    emit stateChanged();
}

void PairingController::setState(State state)
{
    if (state_ == state)
    {
        return;
    }

    state_ = state;
    emit stateChanged();
}

} // namespace lynx::app
