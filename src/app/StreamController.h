#pragma once

#include "remoteplay/Console.h"
#include "remoteplay/StreamSession.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"
#include "render/StreamWindow.h"

#include "core/StreamMetrics.h"

#include <QObject>
#include <QString>
#include <QtQmlIntegration>

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <windows.h>

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
    Q_PROPERTY(QString failureHeading READ failureHeading NOTIFY changed)
    Q_PROPERTY(QString failureTitle READ failureTitle NOTIFY changed)
    Q_PROPERTY(QString failureMessage READ failureMessage NOTIFY changed)

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
    };

    StreamController(SettingsController& settings, InputController& input,
                     QObject* parent = nullptr);
    ~StreamController() override;

    [[nodiscard]] Phase phase() const;
    [[nodiscard]] QString consoleName() const;
    [[nodiscard]] QString failureHeading() const;
    [[nodiscard]] QString failureTitle() const;
    [[nodiscard]] QString failureMessage() const;

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
    void onConnected(std::uint64_t session);
    void onFirstPicture(std::uint64_t session);
    void onEnded(std::uint64_t session, remoteplay::SessionEnd end);
    void onWindowClosed(std::uint64_t session, render::CloseReason reason);
    [[nodiscard]] remoteplay::StreamSession::Events eventsFor(std::uint64_t session);
    // Runs `work` on the interface thread; chiaki calls in from its own.
    void post(std::function<void()> work);
    void stop();
    void fail(const QString& title, const QString& message);
    void setPhase(Phase phase);

    SettingsController& settings_;
    InputController& input_;
    std::optional<Target> target_;
    // The main window, whose monitor the stream window opens on.
    HWND mainWindow_ = nullptr;
    Phase phase_ = Phase::Idle;
    QString failureHeading_;
    QString failureTitle_;
    QString failureMessage_;
    // Counts sessions, so events of one already stopped are ignored.
    std::uint64_t sessionNumber_ = 0;
    std::optional<render::GpuDevice> gpu_;
    std::shared_ptr<render::FrameMailbox> mailbox_;
    std::shared_ptr<core::StreamMetrics> metrics_;
    std::shared_ptr<input::KeyboardMouseInput> keyboardMouse_;
    std::unique_ptr<remoteplay::StreamSession> session_;
    std::unique_ptr<render::StreamWindow> window_;
};

} // namespace mfplay::app
