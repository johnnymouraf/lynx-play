#include "app/StreamController.h"

#include "app/Fonts.h"
#include "app/InputController.h"
#include "app/OverlayInput.h"
#include "app/SettingsController.h"
#include "input/GamepadManager.h"
#include "input/KeyboardMouse.h"
#include "render/ReflexSession.h"

#include <QGuiApplication>
#include <QWindow>

#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::app
{

namespace
{

using namespace Qt::StringLiterals;
using remoteplay::SessionEnd;

struct FailureText
{
    QString title;
    QString message;
};

QString logsHint()
{
    return u"Veja o log em %LOCALAPPDATA%\\MFPlay\\logs."_s;
}

FailureText notPairedText()
{
    return {u"Este console não está pareado"_s,
            u"Pareie o PS5 com este PC pelo menu do card e tente de novo."_s};
}

FailureText noGpuText()
{
    return {u"Nenhuma GPU pode mostrar o vídeo"_s,
            u"O MFPlay precisa de uma GPU com Direct3D 12. "_s + logsHint()};
}

FailureText startFailedText()
{
    return {u"Não foi possível iniciar a conexão"_s, logsHint()};
}

FailureText renderFailedText()
{
    return {u"O vídeo parou"_s, u"A GPU deixou de responder. "_s + logsHint()};
}

FailureText textFor(SessionEnd end)
{
    switch (end)
    {
    case SessionEnd::ConnectionRefused:
        return {u"O console recusou a conexão"_s,
                u"Confira se a Reprodução remota está ativada no PS5 e se nenhum outro aparelho "
                u"está jogando nele agora."_s};
    case SessionEnd::InUse:
        return {u"O PS5 já está em uso"_s,
                u"Outro aparelho está jogando nele pela Reprodução remota. Encerre lá e tente "
                u"de novo."_s};
    case SessionEnd::ConsoleCrashed:
        return {u"A Reprodução remota do PS5 falhou"_s, u"Reinicie o PS5 e tente de novo."_s};
    case SessionEnd::VersionMismatch:
        return {u"Versões incompatíveis"_s,
                u"O PS5 pede uma versão da Reprodução remota que o MFPlay ainda não tem."_s};
    case SessionEnd::ConsoleNotFound:
        return {u"O console não respondeu"_s,
                u"Confira se o PS5 está ligado e na mesma rede que este PC."_s};
    case SessionEnd::Disconnected:
        return {u"A transmissão caiu"_s,
                u"A conexão com o PS5 foi interrompida. Confira a rede e tente de novo."_s};
    case SessionEnd::LoginPinRequired:
        return {u"O PS5 pediu o PIN de login"_s,
                u"Desative o PIN de login do usuário no PS5 para jogar pelo MFPlay."_s};
    case SessionEnd::VideoFailed:
        return {u"O vídeo não pôde ser decodificado"_s,
                u"A GPU não decodifica esse vídeo. Tente o codec H.264 em "
                u"Configurações › Vídeo."_s};
    case SessionEnd::Stopped:
    case SessionEnd::EndedByConsole:
    case SessionEnd::Unknown:
        break;
    }

    return {u"Não foi possível conectar"_s, logsHint()};
}

HWND focusedWindow()
{
    const QWindow* window = QGuiApplication::focusWindow();

    if (window == nullptr)
    {
        return nullptr;
    }

    return reinterpret_cast<HWND>(window->winId());
}

} // namespace

StreamController::StreamController(SettingsController& settings, InputController& input,
                                   QObject* parent)
    : QObject(parent), settings_(settings), input_(input)
{
    connect(&settings_, &SettingsController::rtxChanged, this,
            [this]
            {
                if (window_ != nullptr)
                {
                    window_->setReflexMode(settings_.reflexMode());
                    window_->setUpscaling(settings_.upscalingMode());
                    window_->setOpticalFlow(settings_.opticalFlow());
                    window_->setFrameGeneration(settings_.frameGenerationMode());
                }
            });

    connect(&settings_, &SettingsController::videoChanged, this,
            [this]
            {
                if (window_ != nullptr)
                {
                    window_->setHdrMode(settings_.hdrMode());
                    window_->applyWindowLayout(settings_.video().windowMode,
                                               settings_.video().windowSize);
                }
            });

    connect(&settings_, &SettingsController::audioChanged, this,
            [this]
            {
                if (session_ == nullptr)
                {
                    return;
                }

                session_->setVolumePercent(settings_.volumePercent());
                session_->setMicrophoneMuted(settings_.microphoneMuted());
            });

    connect(&settings_, &SettingsController::keyboardMouseChanged, this,
            [this]
            {
                if (keyboardMouse_ != nullptr)
                {
                    keyboardMouse_->setSettings(settings_.keyboardMouse());
                }
            });

    connect(&input_, &InputController::microphoneButtonPressed, this,
            [this]
            {
                const bool muted = !settings_.microphoneMuted();
                settings_.setMicrophoneMuted(muted);
                emit notice(u"info"_s, muted ? u"Microfone silenciado"_s : u"Microfone ativado"_s,
                            QString());
            });
}

StreamController::~StreamController()
{
    stop();
}

StreamController::Phase StreamController::phase() const
{
    return phase_;
}

QString StreamController::consoleName() const
{
    return target_ ? target_->name : QString();
}

QString StreamController::failureHeading() const
{
    return failureHeading_;
}

QString StreamController::failureTitle() const
{
    return failureTitle_;
}

QString StreamController::failureMessage() const
{
    return failureMessage_;
}

void StreamController::play(Target target)
{
    spdlog::info("Play requested on {}", target.key.toStdString());
    mainWindow_ = focusedWindow();
    target_ = std::move(target);
    startSession();
}

void StreamController::cancel()
{
    // While streaming, the stream window owns Esc and its close button.
    if (phase_ == Phase::Idle || phase_ == Phase::Streaming)
    {
        return;
    }

    if (phase_ == Phase::Connecting || phase_ == Phase::StartingVideo)
    {
        spdlog::info("Connecting canceled");
    }

    stop();
    setPhase(Phase::Idle);
}

void StreamController::retry()
{
    if (target_)
    {
        startSession();
    }
}

void StreamController::startSession()
{
    stop();
    const std::uint64_t session = ++sessionNumber_;
    const Target& target = *target_;
    setPhase(Phase::Connecting);

    if (!target.credentials)
    {
        spdlog::warn("Cannot play on {}: it isn't paired", target.key.toStdString());
        const FailureText text = notPairedText();
        fail(text.title, text.message);
        return;
    }

    auto gpu = render::createGpuDevice();

    if (!gpu)
    {
        spdlog::error("Cannot play: {}", gpu.error());
        const FailureText text = noGpuText();
        fail(text.title, text.message);
        return;
    }

    auto mailbox = render::FrameMailbox::create();

    if (!mailbox)
    {
        spdlog::error("Cannot start the session: {}", mailbox.error());
        const FailureText text = startFailedText();
        fail(text.title, text.message);
        return;
    }

    metrics_ = std::make_shared<core::StreamMetrics>();
    keyboardMouse_ = std::make_shared<input::KeyboardMouseInput>(settings_.keyboardMouse());

    const remoteplay::SessionRequest request{
        .host = target.host,
        .ps5 = target.ps5,
        .credentials = *target.credentials,
        .video = settings_.video(),
        .audio = settings_.audio(),
        .gamepads = input_.gamepads(),
        .keyboardMouse = keyboardMouse_.get(),
        .metrics = metrics_,
    };
    auto started = remoteplay::StreamSession::start(*gpu, *mailbox, request, eventsFor(session));

    if (!started)
    {
        spdlog::error("Cannot start the session: {}", started.error());
        const FailureText text = startFailedText();
        fail(text.title, text.message);
        return;
    }

    gpu_ = std::move(*gpu);
    mailbox_ = std::move(*mailbox);
    session_ = std::move(*started);
}

remoteplay::StreamSession::Events StreamController::eventsFor(std::uint64_t session)
{
    return {
        .onConnected = [this, session] { post([this, session] { onConnected(session); }); },
        .onFirstPicture = [this, session] { post([this, session] { onFirstPicture(session); }); },
        .onEnded = [this, session](SessionEnd end, const std::string&)
        { post([this, session, end] { onEnded(session, end); }); },
        .onMicrophoneMuted = [this, session](bool muted)
        {
            post(
                [this, session, muted]
                {
                    if (session == sessionNumber_)
                    {
                        settings_.setMicrophoneMuted(muted);
                    }
                });
        },
        .onNotice = [this, session](std::string title, std::string message)
        {
            post(
                [this, session, title = std::move(title), message = std::move(message)]
                {
                    if (session != sessionNumber_)
                    {
                        return;
                    }

                    emit notice(u"standby"_s, QString::fromStdString(title),
                                QString::fromStdString(message));
                });
        },
    };
}

void StreamController::post(std::function<void()> work)
{
    QMetaObject::invokeMethod(this, std::move(work), Qt::QueuedConnection);
}

void StreamController::onConnected(std::uint64_t session)
{
    if (session != sessionNumber_ || phase_ != Phase::Connecting)
    {
        return;
    }

    setPhase(Phase::StartingVideo);
}

void StreamController::onFirstPicture(std::uint64_t session)
{
    if (session != sessionNumber_)
    {
        return;
    }

    const auto closed = [this, session](render::CloseReason reason)
    { post([this, session, reason] { onWindowClosed(session, reason); }); };

    const render::OverlayActions overlayActions{
        .reflexChanged = [this](core::ReflexMode mode)
        {
            const QString name = QString::fromLatin1(render::reflexModeName(mode));
            post([this, name] { settings_.setReflex(name); });
        },
        .upscalingChanged = [this](core::Upscaling upscaling)
        {
            const QString name = QString::fromLatin1(core::upscalingName(upscaling).data());
            post([this, name] { settings_.setUpscaling(name); });
        },
        .frameGenerationChanged = [this](core::FrameGenerationMode mode)
        {
            const QString name = QString::fromLatin1(core::frameGenerationName(mode).data());
            post([this, name] { settings_.setFrameGeneration(name); });
        },
        .lowestLatencyChanged = [this](bool lowest)
        { post([this, lowest] { settings_.setLowestLatency(lowest); }); },
        .microphoneMutedChanged = [this](bool muted)
        { post([this, muted] { settings_.setMicrophoneMuted(muted); }); },
        .statisticsVisibleChanged = [this](bool visible)
        { post([this, visible] { settings_.setOverlayStatisticsVisible(visible); }); },
        .menuOpenChanged = [this](bool open)
        {
            post(
                [this, open]
                {
                    if (input_.gamepads() != nullptr)
                    {
                        input_.gamepads()->setOverlayOpen(open);
                    }

                    if (keyboardMouse_ != nullptr)
                    {
                        keyboardMouse_->setOverlayOpen(open);
                    }
                });
        },
        .windowLayoutChanged = [this](core::WindowMode mode, core::WindowSize size)
        { post([this, mode, size] { settings_.setWindowLayout(mode, size); }); },
        .disconnect = [this]
        {
            post(
                [this]
                {
                    if (window_ != nullptr)
                    {
                        window_->close();
                    }
                });
        },
    };

    auto window = render::StreamWindow::open(
        *gpu_, mailbox_,
        {
            .title = L"MFPlay — " + target_->name.toStdWString(),
            .timing = settings_.presentTiming(),
            .reflex = settings_.reflexMode(),
            .upscaling = settings_.upscalingMode(),
            .opticalFlow = settings_.opticalFlow(),
            .streamFramesPerSecond = session_->framesPerSecond(),
            .frameGeneration = settings_.frameGenerationMode(),
            .hdr = settings_.hdrMode(),
            .rtxVideoHdrAvailable = settings_.rtxVideoHdrAvailable(),
            .mode = settings_.video().windowMode,
            .windowedSize = settings_.video().windowSize,
            .monitorOf = mainWindow_,
            .overlayFonts = overlayFonts(),
            .overlay =
                {
                    .statisticsVisible = settings_.overlayStatisticsVisible(),
                    .reflex = settings_.reflexMode(),
                    .lowestLatency = settings_.lowestLatency(),
                    .microphoneMuted = settings_.microphoneMuted(),
                    .microphoneDetail = input_.controllerConnected() ? "DualSense" : "Sem controle",
                    .sessionName = target_->name.toStdString(),
                    .windowMode = settings_.video().windowMode,
                    .windowSize = settings_.video().windowSize,
                },
            .overlayActions = overlayActions,
            .metrics = metrics_,
            .keyboardMouse = keyboardMouse_,
            .gamepad =
                [this]
            {
                const input::GamepadManager* gamepads = input_.gamepads();

                if (gamepads == nullptr)
                {
                    return render::OverlayGamepad{};
                }

                const std::optional<ChiakiControllerState> state = gamepads->gamepadState();

                return state ? overlayGamepad(*state) : render::OverlayGamepad{};
            },
            .onClosed = closed,
            .onWindowModeChanged =
                [this](core::WindowMode mode)
            {
                const QString name = QString::fromLatin1(core::windowModeName(mode).data());
                post([this, name] { settings_.setWindowMode(name); });
            },
            .onWindowedSizeChanged = [this](core::WindowSize size)
            { post([this, size] { settings_.setWindowedSize(size); }); },
        });

    if (!window)
    {
        spdlog::error("Cannot open the stream window: {}", window.error());
        stop();
        const FailureText text = renderFailedText();
        fail(text.title, text.message);
        return;
    }

    window_ = std::move(*window);
    setPhase(Phase::Streaming);
    emit streamStarted(target_->key);
}

void StreamController::onEnded(std::uint64_t session, SessionEnd end)
{
    if (session != sessionNumber_)
    {
        return;
    }

    stop();

    if (end == SessionEnd::Stopped || end == SessionEnd::EndedByConsole)
    {
        setPhase(Phase::Idle);
        emit notice(u"info"_s, u"O PS5 encerrou a transmissão"_s, QString());
        return;
    }

    const FailureText text = textFor(end);
    fail(text.title, text.message);
}

void StreamController::onWindowClosed(std::uint64_t session, render::CloseReason reason)
{
    if (session != sessionNumber_)
    {
        return;
    }

    spdlog::info("Stream window closed; ending the session");
    stop();

    if (reason == render::CloseReason::RenderFailed)
    {
        const FailureText text = renderFailedText();
        fail(text.title, text.message);
        return;
    }

    setPhase(Phase::Idle);
}

// The window goes first, then the session: closing the session joins chiaki's
// threads, after which nothing feeds the decoder.
void StreamController::stop()
{
    ++sessionNumber_;

    if (input_.gamepads() != nullptr)
    {
        input_.gamepads()->setOverlayOpen(false);
    }

    if (keyboardMouse_ != nullptr)
    {
        keyboardMouse_->setOverlayOpen(false);
        keyboardMouse_->releaseAll();
    }

    window_.reset();
    session_.reset();
    mailbox_.reset();
    metrics_.reset();
    keyboardMouse_.reset();
    gpu_.reset();
}

void StreamController::fail(const QString& title, const QString& message)
{
    const bool wasStreaming = phase_ == Phase::Streaming;
    failureHeading_ = wasStreaming ? u"A transmissão parou"_s : u"Não foi possível conectar"_s;
    failureTitle_ = title;
    failureMessage_ = message;
    phase_ = Phase::Failed;
    emit changed();
}

void StreamController::setPhase(Phase phase)
{
    if (phase_ == phase)
    {
        return;
    }

    phase_ = phase;
    emit changed();
}

} // namespace mfplay::app
