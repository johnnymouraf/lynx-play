#pragma once

#include "remoteplay/Console.h"
#include "remoteplay/StreamSession.h"

#include "core/StreamMetrics.h"

#if defined(_WIN32)
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"
#include "render/StreamWindow.h"
#endif

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace mfplay::input
{
class KeyboardMouseInput;
}

namespace mfplay::app
{

class InputController;
class SettingsController;

// Playing on a console: the session, the stream window it opens once the
// first picture is decoded, and what the connecting screen shows meanwhile.
class StreamController : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main()")

    Q_PROPERTY(Phase phase READ phase NOTIFY changed)
    Q_PROPERTY(QString consoleName READ consoleName NOTIFY changed)
    Q_PROPERTY(bool overInternet READ overInternet NOTIFY changed)
    Q_PROPERTY(QString connectionStageText READ connectionStageText NOTIFY changed)
    Q_PROPERTY(QString failureHeading READ failureHeading NOTIFY changed)
    Q_PROPERTY(QString failureTitle READ failureTitle NOTIFY changed)
    Q_PROPERTY(QString failureMessage READ failureMessage NOTIFY changed)
    Q_PROPERTY(QString connectionNote READ connectionNote NOTIFY changed)
    Q_PROPERTY(bool signInNeeded READ signInNeeded NOTIFY changed)

public:
    enum class Phase
    {
        Idle,
        Connecting,
        StartingVideo,
        // The stream window is open.
        Streaming,
        Failed,
    };
    Q_ENUM(Phase)

    // A console to play on, as its card knows it.
    struct Target
    {
        QString key;
        QString name;
        std::string host;
        bool ps5 = true;
        // Absent when the console isn't paired.
        std::optional<remoteplay::Credentials> credentials;
        // Set to reach the console through the PSN instead of `host`.
        std::optional<remoteplay::PsnTarget> psn;
        // The PSN was reached because the console didn't answer the local
        // network in time.
        bool internetFallback = false;
    };

    StreamController(SettingsController& settings, InputController& input,
                     QObject* parent = nullptr);
    ~StreamController() override;

    [[nodiscard]] Phase phase() const;
    [[nodiscard]] QString consoleName() const;
    // Whether the connection goes through the PSN instead of the local network.
    [[nodiscard]] bool overInternet() const;
    // What the PSN connection is doing now, for the connecting screen; empty
    // on the local network.
    [[nodiscard]] QString connectionStageText() const;
    [[nodiscard]] QString failureHeading() const;
    [[nodiscard]] QString failureTitle() const;
    [[nodiscard]] QString failureMessage() const;
    // The PSN connection's current stage; absent on the local network.
    [[nodiscard]] std::optional<remoteplay::ConnectionStage> connectionStage() const;
    // Why the last PSN connection failed; absent when it didn't.
    [[nodiscard]] std::optional<remoteplay::PsnFailure> psnFailure() const;
    // The route note for the connecting screen: the reconnection count, or
    // the switch to the internet; empty otherwise.
    [[nodiscard]] QString connectionNote() const;
    // Whether the failure is an expired or refused PSN sign-in, which the
    // error screen answers by signing in again.
    [[nodiscard]] bool signInNeeded() const;

    // Where the PSN connections get the sign-in's current access token; empty
    // once the sign-in has expired.
    void setAccessTokenSource(std::function<std::string()> source);

    // Ends any session and connects to the target; a console that isn't
    // paired fails at once.
    void play(Target target);

    // Stops connecting or leaves the failure, back to the consoles.
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void retry();

signals:
    void changed();
    // The first picture of a session with this console arrived.
    void streamStarted(const QString& key);
    // A message for a toast; `tone` is info, ready, standby or danger.
    void notice(const QString& tone, const QString& title, const QString& message);

private:
    void startSession();
    // Gives the PSN target the sign-in's current token; false when there is
    // none left.
    [[nodiscard]] bool renewAccessToken();
    void scheduleReconnect();
    void onConnected(std::uint64_t session);
#if defined(_WIN32)
    void onFirstPicture(std::uint64_t session);
#endif
    void onEnded(std::uint64_t session, remoteplay::SessionEnd end);
    void onStage(std::uint64_t session, remoteplay::ConnectionStage stage);
    void onPsnFailed(std::uint64_t session, remoteplay::PsnFailure failure);
#if defined(_WIN32)
    void onWindowClosed(std::uint64_t session, render::CloseReason reason);
#endif
    [[nodiscard]] remoteplay::StreamSession::Events eventsFor(std::uint64_t session);
    // Runs `work` on the interface thread; chiaki calls in from its own.
    void post(std::function<void()> work);
    void stop();
    void fail(const QString& title, const QString& message);
    void setPhase(Phase phase);

    SettingsController& settings_;
    InputController& input_;
    std::optional<Target> target_;
#if defined(_WIN32)
    // The main window, whose monitor the stream window opens on.
    HWND mainWindow_ = nullptr;
#endif
    Phase phase_ = Phase::Idle;
    QString failureHeading_;
    QString failureTitle_;
    QString failureMessage_;
    std::optional<remoteplay::ConnectionStage> connectionStage_;
    std::optional<remoteplay::PsnFailure> psnFailure_;
    // Counts sessions, so events of one already stopped are ignored.
    std::uint64_t sessionNumber_ = 0;
    // Whether the current session reached the console.
    bool sessionConnected_ = false;
    // The reconnections made since the last connection, for the drop at hand.
    int psnRetries_ = 0;
    std::function<std::string()> accessTokenSource_;
#if defined(_WIN32)
    std::optional<render::GpuDevice> gpu_;
    std::shared_ptr<render::FrameMailbox> mailbox_;
#endif
    std::shared_ptr<core::StreamMetrics> metrics_;
    std::shared_ptr<input::KeyboardMouseInput> keyboardMouse_;
    std::unique_ptr<remoteplay::StreamSession> session_;
#if defined(_WIN32)
    std::unique_ptr<render::StreamWindow> window_;
#endif
};

} // namespace mfplay::app
