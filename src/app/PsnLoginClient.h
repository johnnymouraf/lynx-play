#pragma once

#include "remoteplay/PsnLogin.h"
#include "remoteplay/PsnTokenStore.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QtQmlIntegration>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class QNetworkAccessManager;
class QNetworkReply;

namespace mfplay::app
{

class SettingsController;

// The pairing dialog's PSN sign-in: opens Sony's page in the default browser,
// takes the redirect URL pasted back, exchanges its code and fills the account
// ID. The tokens stay sealed in psn.json; settings.json keeps the expiry.
class PsnLoginClient : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by main()")

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString failureTitle READ failureTitle NOTIFY stateChanged)
    Q_PROPERTY(QString failureMessage READ failureMessage NOTIFY stateChanged)
    Q_PROPERTY(QString accountId READ accountId NOTIFY stateChanged)
    Q_PROPERTY(QString expiryText READ expiryText NOTIFY stateChanged)

public:
    enum class State
    {
        Idle,
        Waiting,
        Exchanging,
        Succeeded,
        Failed,
    };
    Q_ENUM(State)

    enum class Failure
    {
        Unknown,
        Browser,
        MalformedRedirect,
        AccessDenied,
        Rejected,
        ExpiredCode,
        Network,
        Server,
        MalformedResponse,
        Storage,
    };

    PsnLoginClient(SettingsController& settings, std::optional<remoteplay::PsnTokenStore> store,
                   QObject* parent = nullptr);
    ~PsnLoginClient() override;

    [[nodiscard]] State state() const;
    [[nodiscard]] QString failureTitle() const;
    [[nodiscard]] QString failureMessage() const;
    // The Base64 account ID of the completed sign-in.
    [[nodiscard]] QString accountId() const;
    // When the access token expires, in the dialog's format.
    [[nodiscard]] QString expiryText() const;

    // The access token while it hasn't expired; empty otherwise.
    [[nodiscard]] std::string validAccessToken() const;

    // Opens the authorize page in the default browser.
    Q_INVOKABLE void start();
    // Takes the URL the browser landed on and exchanges its code.
    Q_INVOKABLE void submitRedirect(const QString& text);
    Q_INVOKABLE void reset();

signals:
    void stateChanged();
    // A usable access token came from a sign-in or a refresh.
    void accessTokenChanged();

private:
    void refreshStoredTokens();
    // Renews the token shortly before it expires, so a reconnection after a
    // long session still finds a valid one.
    void scheduleRenewal();
    void renewDueToken();
    void exchangeCode(const std::string& code);
    void postToken(std::string body, std::uint64_t attempt, bool refresh);
    void fetchUserId(std::uint64_t attempt);
    void watchTimeout(QNetworkReply* reply);
    void onTokenReply(QNetworkReply* reply, std::uint64_t attempt, bool refresh);
    void onUserIdReply(QNetworkReply* reply, std::uint64_t attempt);
    void onRefreshedToken(std::string_view body);
    void succeed(const remoteplay::AccountId& accountId);
    void fail(Failure failure);
    void setState(State state);
    void abortPending();
    [[nodiscard]] bool saveTokens();

    static QString formatExpiry(remoteplay::Clock::time_point time);

    SettingsController& settings_;
    std::optional<remoteplay::PsnTokenStore> store_;
    remoteplay::PsnTokens stored_;
    std::unique_ptr<QNetworkAccessManager> network_;
    QTimer renewal_;
    QPointer<QNetworkReply> pending_;
    QString accountId_;
    QString expiryText_;
    Failure failure_ = Failure::Unknown;
    State state_ = State::Idle;
    // Tells a reply of an abandoned attempt from the current one.
    std::uint64_t attempt_ = 0;
};

} // namespace mfplay::app
