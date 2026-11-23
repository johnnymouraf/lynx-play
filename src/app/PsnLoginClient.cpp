#include "app/PsnLoginClient.h"

#include "app/SettingsController.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <chrono>
#include <format>
#include <string>
#include <string_view>
#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

using namespace Qt::StringLiterals;

constexpr int kRequestTimeoutMs = 15000;
constexpr int kServerErrorStatus = 500;
constexpr int kOkStatus = 200;
constexpr int kBadRequestStatus = 400;

struct FailureText
{
    QString title;
    QString message;
};

FailureText failureText(PsnLoginClient::Failure failure)
{
    switch (failure)
    {
    case PsnLoginClient::Failure::ExpiredCode:
        return {u"O código expirou ou já foi usado."_s,
                u"Ele vale por poucos minutos depois do login. Entre com a PSN de novo para "
                u"gerar outro."_s};
    case PsnLoginClient::Failure::AccessDenied:
        return {u"Login cancelado na PSN"_s,
                u"Você não autorizou o acesso. Entre com a PSN de novo quando quiser."_s};
    case PsnLoginClient::Failure::MalformedRedirect:
        return {u"O link de retorno não tem o código"_s,
                u"Copie o endereço completo da página para onde a PSN redirecionou o navegador "
                u"e cole aqui."_s};
    case PsnLoginClient::Failure::Rejected:
        return {u"A PSN recusou o acesso"_s, u"Entre com a PSN de novo."_s};
    case PsnLoginClient::Failure::Network:
        return {u"Não foi possível falar com a PSN"_s,
                u"Confira a conexão com a internet e tente de novo."_s};
    case PsnLoginClient::Failure::Server:
        return {u"A PSN não respondeu"_s, u"Tente de novo em instantes."_s};
    case PsnLoginClient::Failure::MalformedResponse:
        return {u"Resposta inesperada da PSN"_s, u"Tente de novo. Se continuar, veja o log."_s};
    case PsnLoginClient::Failure::Storage:
        return {u"Não foi possível salvar o login"_s,
                u"Os tokens não foram gravados no disco. Veja o log em "
                u"%LOCALAPPDATA%\\LynxPlay\\logs."_s};
    case PsnLoginClient::Failure::Browser:
        return {u"Não foi possível abrir o navegador"_s,
                u"O endereço da PSN está no log. Você também pode informar o Account ID "
                u"manualmente."_s};
    case PsnLoginClient::Failure::Unknown:
        break;
    }

    return {u"Algo deu errado no login"_s, u"Tente de novo. Se continuar, veja o log."_s};
}

std::string_view textOf(const QByteArray& bytes)
{
    if (bytes.isEmpty())
    {
        return {};
    }

    return {bytes.constData(), static_cast<std::size_t>(bytes.size())};
}

std::int64_t secondsOf(remoteplay::Clock::time_point time)
{
    return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}

} // namespace

PsnLoginClient::PsnLoginClient(SettingsController& settings,
                               std::optional<remoteplay::PsnTokenStore> store, QObject* parent)
    : QObject(parent),
      settings_(settings),
      store_(std::move(store)),
      network_(std::make_unique<QNetworkAccessManager>(this))
{
    if (!store_)
    {
        return;
    }

    stored_ = store_->load();

    if (stored_.accessToken.empty() || stored_.refreshToken.empty())
    {
        return;
    }

    const auto now = remoteplay::Clock::now();

    if (stored_.expiry && now < *stored_.expiry)
    {
        spdlog::info("The stored PSN token is still valid until {}",
                     formatExpiry(*stored_.expiry).toStdString());
        return;
    }

    refreshStoredTokens();
}

PsnLoginClient::~PsnLoginClient() = default;

PsnLoginClient::State PsnLoginClient::state() const
{
    return state_;
}

QString PsnLoginClient::failureTitle() const
{
    return failureText(failure_).title;
}

QString PsnLoginClient::failureMessage() const
{
    return failureText(failure_).message;
}

QString PsnLoginClient::accountId() const
{
    return accountId_;
}

QString PsnLoginClient::expiryText() const
{
    return expiryText_;
}

void PsnLoginClient::start()
{
    const std::uint64_t attempt = ++attempt_;
    abortPending();
    failure_ = Failure::Unknown;
    accountId_.clear();
    expiryText_.clear();

    const std::string url = remoteplay::psnAuthorizeUrl();
    spdlog::info("Opening the PSN sign-in page: {}", url);

    if (!QDesktopServices::openUrl(QUrl(QString::fromStdString(url))))
    {
        spdlog::warn("The default browser refused the PSN sign-in page");
        fail(Failure::Browser);
        return;
    }

    if (attempt != attempt_)
    {
        return;
    }

    setState(State::Waiting);
}

void PsnLoginClient::submitRedirect(const QString& text)
{
    if (state_ == State::Exchanging)
    {
        return;
    }

    const remoteplay::Redirect redirect = remoteplay::parseRedirect(text.toStdString());

    switch (redirect.status)
    {
    case remoteplay::RedirectStatus::Code:
        exchangeCode(redirect.code);
        return;
    case remoteplay::RedirectStatus::AccessDenied:
        fail(Failure::AccessDenied);
        return;
    case remoteplay::RedirectStatus::Rejected:
        spdlog::warn("The PSN sign-in ended with error '{}'", redirect.error);
        fail(Failure::Rejected);
        return;
    case remoteplay::RedirectStatus::Malformed:
        break;
    }

    fail(Failure::MalformedRedirect);
}

void PsnLoginClient::reset()
{
    ++attempt_;
    abortPending();
    failure_ = Failure::Unknown;
    accountId_.clear();
    expiryText_.clear();
    setState(State::Idle);
}

void PsnLoginClient::refreshStoredTokens()
{
    if (!store_ || stored_.refreshToken.empty())
    {
        return;
    }

    const std::uint64_t attempt = ++attempt_;
    abortPending();
    spdlog::info("Refreshing the PSN token");
    postToken(remoteplay::refreshTokenBody(stored_.refreshToken), attempt, true);
}

void PsnLoginClient::exchangeCode(const std::string& code)
{
    const std::uint64_t attempt = ++attempt_;
    abortPending();
    failure_ = Failure::Unknown;
    spdlog::info("Exchanging the PSN redirect code for tokens");
    setState(State::Exchanging);
    postToken(remoteplay::authorizationCodeBody(code), attempt, false);
}

void PsnLoginClient::postToken(std::string body, std::uint64_t attempt, bool refresh)
{
    QNetworkRequest request{QUrl(QString::fromStdString(std::string(remoteplay::kPsnTokenUrl)))};
    request.setRawHeader("Authorization",
                         QByteArray::fromStdString(remoteplay::psnBasicAuthHeader()));
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                       QStringLiteral("application/x-www-form-urlencoded"));

    QNetworkReply* reply = network_->post(request, QByteArray::fromStdString(body));
    pending_ = reply;
    watchTimeout(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, attempt, refresh] {
        onTokenReply(reply, attempt, refresh);
    });
}

void PsnLoginClient::fetchUserId(std::uint64_t attempt)
{
    if (attempt != attempt_)
    {
        return;
    }

    const std::string url =
        std::format("{}/{}", remoteplay::kPsnTokenUrl, stored_.accessToken);
    QNetworkRequest request{QUrl(QString::fromStdString(url))};
    request.setRawHeader("Authorization",
                         QByteArray::fromStdString(remoteplay::psnBasicAuthHeader()));

    QNetworkReply* reply = network_->get(request);
    pending_ = reply;
    watchTimeout(reply);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, attempt] { onUserIdReply(reply, attempt); });
}

void PsnLoginClient::watchTimeout(QNetworkReply* reply)
{
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(kRequestTimeoutMs);
    connect(timer, &QTimer::timeout, reply, [reply] {
        if (reply->isFinished())
        {
            return;
        }

        reply->setProperty("timedOut", true);
        reply->abort();
    });
    timer->start();
}

void PsnLoginClient::onTokenReply(QNetworkReply* reply, std::uint64_t attempt, bool refresh)
{
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool timedOut = reply->property("timedOut").toBool();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorText = reply->errorString();
    reply->deleteLater();

    if (pending_ == reply)
    {
        pending_ = nullptr;
    }

    if (refresh)
    {
        if (timedOut || status != kOkStatus)
        {
            spdlog::warn("The PSN token refresh failed (HTTP {}, {})", status,
                         timedOut ? std::string("timed out") : errorText.toStdString());
            return;
        }

        onRefreshedToken(textOf(body));
        return;
    }

    if (attempt != attempt_)
    {
        return;
    }

    if (timedOut)
    {
        spdlog::warn("The PSN token request timed out");
        fail(Failure::Network);
        return;
    }

    if (status == 0 && error != QNetworkReply::NoError)
    {
        spdlog::warn("The PSN token request failed: {}", errorText.toStdString());
        fail(Failure::Network);
        return;
    }

    if (status == kBadRequestStatus)
    {
        spdlog::warn("The PSN refused the authorization code (HTTP 400)");
        fail(Failure::ExpiredCode);
        return;
    }

    if (status != kOkStatus)
    {
        spdlog::warn("The PSN token request failed (HTTP {})", status);
        fail(status >= kServerErrorStatus ? Failure::Server : Failure::Rejected);
        return;
    }

    const auto tokens = remoteplay::parseTokenResponse(textOf(body));

    if (!tokens)
    {
        spdlog::warn("The PSN token response has no usable tokens");
        fail(Failure::MalformedResponse);
        return;
    }

    stored_.accessToken = tokens->accessToken;

    if (!tokens->refreshToken.empty())
    {
        stored_.refreshToken = tokens->refreshToken;
    }

    stored_.expiry = remoteplay::Clock::now() + tokens->expiresIn;

    if (stored_.refreshToken.empty())
    {
        spdlog::warn("The PSN token response has no refresh token");
        fail(Failure::MalformedResponse);
        return;
    }

    if (!saveTokens())
    {
        fail(Failure::Storage);
        return;
    }

    fetchUserId(attempt);
}

void PsnLoginClient::onRefreshedToken(std::string_view body)
{
    const auto tokens = remoteplay::parseTokenResponse(body);

    if (!tokens)
    {
        spdlog::warn("The PSN token refresh returned an unexpected response");
        return;
    }

    stored_.accessToken = tokens->accessToken;

    if (!tokens->refreshToken.empty())
    {
        stored_.refreshToken = tokens->refreshToken;
    }

    stored_.expiry = remoteplay::Clock::now() + tokens->expiresIn;

    if (!saveTokens())
    {
        return;
    }

    settings_.recordPsnSignIn(secondsOf(*stored_.expiry));
    spdlog::info("PSN token refreshed; the access token expires at {}",
                 formatExpiry(*stored_.expiry).toStdString());
}

void PsnLoginClient::onUserIdReply(QNetworkReply* reply, std::uint64_t attempt)
{
    const QByteArray body = reply->readAll();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const bool timedOut = reply->property("timedOut").toBool();
    const QNetworkReply::NetworkError error = reply->error();
    const QString errorText = reply->errorString();
    reply->deleteLater();

    if (pending_ == reply)
    {
        pending_ = nullptr;
    }

    if (attempt != attempt_)
    {
        return;
    }

    if (timedOut)
    {
        spdlog::warn("The PSN account request timed out");
        fail(Failure::Network);
        return;
    }

    if (status == 0 && error != QNetworkReply::NoError)
    {
        spdlog::warn("The PSN account request failed: {}", errorText.toStdString());
        fail(Failure::Network);
        return;
    }

    if (status >= kServerErrorStatus)
    {
        spdlog::warn("The PSN account request failed (HTTP {})", status);
        fail(Failure::Server);
        return;
    }

    if (status != kOkStatus)
    {
        spdlog::warn("The PSN refused the account request (HTTP {})", status);
        fail(Failure::Rejected);
        return;
    }

    const auto userId = remoteplay::parseUserIdResponse(textOf(body));

    if (!userId)
    {
        spdlog::warn("The PSN account response has no user ID");
        fail(Failure::MalformedResponse);
        return;
    }

    const auto accountId = remoteplay::accountIdFromUserId(*userId);

    if (!accountId)
    {
        spdlog::warn("The PSN user ID is not a number");
        fail(Failure::MalformedResponse);
        return;
    }

    succeed(*accountId);
}

void PsnLoginClient::succeed(const remoteplay::AccountId& accountId)
{
    accountId_ = QString::fromStdString(remoteplay::formatAccountId(accountId));

    if (stored_.expiry)
    {
        expiryText_ = formatExpiry(*stored_.expiry);
        settings_.recordPsnSignIn(secondsOf(*stored_.expiry));
    }

    spdlog::info("PSN sign-in completed; the access token expires at {}",
                 expiryText_.toStdString());
    setState(State::Succeeded);
}

void PsnLoginClient::fail(Failure failure)
{
    failure_ = failure;

    if (state_ == State::Failed)
    {
        emit stateChanged();
        return;
    }

    setState(State::Failed);
}

void PsnLoginClient::setState(State state)
{
    if (state_ == state)
    {
        return;
    }

    state_ = state;
    emit stateChanged();
}

void PsnLoginClient::abortPending()
{
    if (pending_.isNull())
    {
        return;
    }

    QNetworkReply* reply = pending_;
    pending_.clear();
    reply->abort();
}

bool PsnLoginClient::saveTokens()
{
    if (!store_)
    {
        return true;
    }

    if (const auto saved = store_->save(stored_); !saved)
    {
        spdlog::error("Cannot save the PSN tokens: {}", saved.error());
        return false;
    }

    return true;
}

QString PsnLoginClient::formatExpiry(remoteplay::Clock::time_point time)
{
    const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(secondsOf(time));
    const QLocale locale(QLocale::Portuguese, QLocale::Brazil);

    return locale.toString(dateTime, QStringLiteral("dd/MM/yyyy 'às' HH:mm"));
}

} // namespace lynx::app
