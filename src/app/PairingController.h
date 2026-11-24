#pragma once

#include "app/PsnLoginClient.h"
#include "remoteplay/AccountId.h"
#include "remoteplay/ConsoleRegistration.h"

#include <QObject>
#include <QString>
#include <QTimer>
#include <QtQmlIntegration>

#include <cstdint>
#include <memory>

namespace mfplay::app
{

class ConsolesController;

// The pairing dialog's flow: checks what the user typed, runs one pairing
// attempt at a time and says how it ended.
class PairingController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by ConsolesController")

    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QString failureTitle READ failureTitle NOTIFY stateChanged)
    Q_PROPERTY(QString failureMessage READ failureMessage NOTIFY stateChanged)
    Q_PROPERTY(QString retryStep READ retryStep NOTIFY stateChanged)
    Q_PROPERTY(QString pairedKey READ pairedKey NOTIFY stateChanged)
    Q_PROPERTY(PsnLoginClient* psnLogin READ psnLogin CONSTANT)

public:
    enum class State
    {
        Idle,
        Pairing,
        Succeeded,
        Failed,
    };
    Q_ENUM(State)

    explicit PairingController(ConsolesController& consoles, PsnLoginClient& psnLogin,
                               QObject* parent = nullptr);
    ~PairingController() override;

    [[nodiscard]] State state() const;
    [[nodiscard]] QString failureTitle() const;
    [[nodiscard]] QString failureMessage() const;
    // The dialog step that fixes the failure: "account", "pin", or empty
    // when trying again as is may work.
    [[nodiscard]] QString retryStep() const;
    // The card key of the console just paired, which may differ from the key
    // pairing started with.
    [[nodiscard]] QString pairedKey() const;
    [[nodiscard]] PsnLoginClient* psnLogin();

    // The message to show under the field, or empty when the text is valid.
    Q_INVOKABLE QString accountIdError(const QString& text) const;
    Q_INVOKABLE QString pinError(const QString& text) const;

    Q_INVOKABLE void start(const QString& key, const QString& accountId, const QString& pin);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void reset();

signals:
    void stateChanged();

private:
    // Runs on chiaki's registration thread.
    void postResult(std::uint64_t attempt, remoteplay::PairingResult result);
    void onResult(std::uint64_t attempt, const remoteplay::PairingResult& result);
    void onTimeout();
    void fail(remoteplay::PairingFailure failure);
    void setState(State state);

    ConsolesController& consoles_;
    PsnLoginClient& psnLogin_;
    std::unique_ptr<remoteplay::ConsoleRegistration> registration_;
    QTimer watchdog_;
    QString key_;
    QString pairedKey_;
    remoteplay::AccountId accountId_{};
    remoteplay::PairingFailure failure_ = remoteplay::PairingFailure::Unknown;
    State state_ = State::Idle;
    // Tells a late result of a canceled attempt from the current one.
    std::uint64_t attempt_ = 0;
};

} // namespace mfplay::app
