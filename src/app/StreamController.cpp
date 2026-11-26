#include "app/StreamController.h"

#include "app/InputController.h"
#include "app/SettingsController.h"
#include "core/ReflexMode.h"
#include "input/GamepadManager.h"
#include "input/KeyboardMouse.h"
#include "remoteplay/PsnPolicy.h"

#if defined(_WIN32)
#include "app/Fonts.h"
#include "app/OverlayInput.h"
#include "media/VideoDecoder.h"
#include "render/ReflexSession.h"
#endif

#include <QGuiApplication>
#include <QTimer>
#include <QWindow>

#include <chrono>
#include <optional>
#include <string>
#include <utility>

#include <spdlog/spdlog.h>

namespace mfplay::app
{

namespace
{

using namespace Qt::StringLiterals;
using remoteplay::SessionEnd;

// The pause before a reconnection, as chiaki-ng waits, so the network can
// come back first.
constexpr auto kReconnectWait = std::chrono::seconds(5);

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

FailureText textFor(const remoteplay::PsnFailure& failure)
{
    using remoteplay::PsnFailureKind;

    switch (failure.kind)
    {
    case PsnFailureKind::Timeout:
        return {u"O console não respondeu"_s,
                u"O PS5 não atendeu a tempo. Confira se ele está ligado ou em repouso, com "
                u"a Reprodução remota ativada, e tente de novo."_s};
    case PsnFailureKind::ConsoleOffline:
        return {u"O console está indisponível"_s,
                u"O PS5 precisa estar ligado, ou em repouso com acesso à internet e a "
                u"Reprodução remota ativada."_s};
    case PsnFailureKind::Network:
        return {u"A rede bloqueou a conexão"_s,
                u"Esta rede não deixa a conexão direta com o PS5 passar. Tente outra rede, "
                u"como o ponto de acesso do celular."_s};
    case PsnFailureKind::Rejected:
        return {u"A PSN recusou o login"_s,
                u"O login expirou ou foi recusado. Entre de novo na PSN para jogar pela "
                u"internet."_s};
    case PsnFailureKind::InvalidDevice:
        return {u"A PSN não reconhece este console"_s,
                u"O PS5 não está na lista da sua conta. Confira se ele usa a mesma conta da "
                u"PSN."_s};
    case PsnFailureKind::Canceled:
    case PsnFailureKind::Unknown:
        break;
    }

    return {u"Não foi possível conectar pela internet"_s, logsHint()};
}

QString stageText(remoteplay::ConnectionStage stage)
{
    using remoteplay::ConnectionStage;

    switch (stage)
    {
    case ConnectionStage::SigningIn:
        return u"Entrando na PSN"_s;
    case ConnectionStage::CreatingSession:
        return u"Criando sessão"_s;
    case ConnectionStage::StartingSession:
        return u"Iniciando sessão no console"_s;
    case ConnectionStage::PunchingControl:
        return u"Abrindo caminho (controle)"_s;
    case ConnectionStage::Registering:
        return u"Registrando este PC no console"_s;
    case ConnectionStage::PunchingData:
        return u"Abrindo caminho (vídeo)"_s;
    case ConnectionStage::Connecting:
        return u"Conectando"_s;
    }

    return {};
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
    case SessionEnd::PsnFailed:
        return {u"Não foi possível conectar pela internet"_s, logsHint()};
    case SessionEnd::Stopped:
    case SessionEnd::EndedByConsole:
    case SessionEnd::Unknown:
        break;
    }

    return {u"Não foi possível conectar"_s, logsHint()};
}

FailureText droppedText(int retries)
{
    if (retries < remoteplay::kMaxPsnRetries)
    {
        return {u"A conexão pela internet caiu"_s, u"Confira a rede e tente de novo."_s};
    }

    return {u"A conexão pela internet caiu"_s,
            u"Não foi possível reconectar depois de %1 tentativas. "
            u"Confira a rede e tente de novo."_s.arg(remoteplay::kMaxPsnRetries)};
}

// The heading of a session that couldn't connect, or couldn't pair.
QString connectFailedHeading(bool registering)
{
    if (registering)
    {
        return u"Não foi possível parear"_s;
    }

    return u"Não foi possível conectar"_s;
}

// A registration says what it was doing: "conectar" would mislead.
FailureText registrationText(const std::optional<remoteplay::PsnFailure>& failure)
{
    if (failure && failure->kind != remoteplay::PsnFailureKind::Unknown)
    {
        return textFor(*failure);
    }

    return {u"Algo deu errado no pareamento pela internet"_s,
            u"Tente de novo. "_s + logsHint()};
}

// A session that ended over the PSN gets its own wording where the local
// network's would mislead, such as "na mesma rede que este PC".
FailureText textForEnd(SessionEnd end, const std::optional<remoteplay::PsnFailure>& failure,
                       bool overInternet, int retries)
{
    if (end == SessionEnd::PsnFailed && failure)
    {
        return textFor(*failure);
    }

    if (!overInternet)
    {
        return textFor(end);
    }

    if (end == SessionEnd::Disconnected || end == SessionEnd::Unknown)
    {
        return droppedText(retries);
    }

    if (end == SessionEnd::ConsoleNotFound)
    {
        return textFor(remoteplay::PsnFailure{.kind = remoteplay::PsnFailureKind::ConsoleOffline});
    }

    return textFor(end);
}

std::optional<remoteplay::PsnFailureKind> kindOf(
    const std::optional<remoteplay::PsnFailure>& failure)
{
    if (!failure)
    {
        return std::nullopt;
    }

    return failure->kind;
}

#if defined(_WIN32)
HWND focusedWindow()
{
    const QWindow* window = QGuiApplication::focusWindow();

    if (window == nullptr)
    {
        return nullptr;
    }

    return reinterpret_cast<HWND>(window->winId());
}
#endif

} // namespace

StreamController::StreamController(SettingsController& settings, InputController& input,
                                   QObject* parent)
    : QObject(parent), settings_(settings), input_(input)
{
#if defined(_WIN32)
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
#endif

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

bool StreamController::overInternet() const
{
    return target_ && (target_->psn || target_->remote);
}

QString StreamController::connectionStageText() const
{
    if (connectionStage_)
    {
        return stageText(*connectionStage_);
    }

    if (target_ && target_->remote)
    {
        return u"Conectando pelo endereço fora de casa"_s;
    }

    return overInternet() ? stageText(remoteplay::ConnectionStage::SigningIn) : QString();
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

std::optional<remoteplay::ConnectionStage> StreamController::connectionStage() const
{
    return connectionStage_;
}

std::optional<remoteplay::PsnFailure> StreamController::psnFailure() const
{
    return psnFailure_;
}

QString StreamController::connectionNote() const
{
    if (psnRetries_ > 0)
    {
        return u"Reconectando (%1 de %2)"_s.arg(psnRetries_).arg(remoteplay::kMaxPsnRetries);
    }

    if (target_ && target_->internetFallback)
    {
        return u"Sem resposta na rede local; conectando pela internet"_s;
    }

    return {};
}

bool StreamController::signInNeeded() const
{
    return phase_ == Phase::Failed && psnFailure_ &&
           psnFailure_->kind == remoteplay::PsnFailureKind::Rejected;
}

void StreamController::setAccessTokenSource(std::function<std::string()> source)
{
    accessTokenSource_ = std::move(source);
}

void StreamController::play(Target target)
{
    spdlog::info("Play requested on {}", target.key.toStdString());
#if defined(_WIN32)
    mainWindow_ = focusedWindow();
#endif
    target_ = std::move(target);
    psnRetries_ = 0;
    registering_ = false;
    onPaired_ = nullptr;
    startSession();
}

void StreamController::pairOverInternet(Target target, PairedHandler onPaired)
{
    spdlog::info("Pairing over the PSN started on {}", target.key.toStdString());
    target_ = std::move(target);
    psnRetries_ = 0;
    registering_ = true;
    onPaired_ = std::move(onPaired);
    startSession();
}

bool StreamController::registering() const
{
    return registering_;
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
    psnRetries_ = 0;
    registering_ = false;
    onPaired_ = nullptr;
    setPhase(Phase::Idle);
}

void StreamController::retry()
{
    if (target_)
    {
        psnRetries_ = 0;
        startSession();
    }
}

void StreamController::startSession()
{
    stop();
    [[maybe_unused]] const std::uint64_t session = ++sessionNumber_;
    const Target& target = *target_;
    connectionStage_.reset();
    psnFailure_.reset();
    sessionConnected_ = false;
    registered_ = false;
    setPhase(Phase::Connecting);

    if (!registering_ && !target.credentials)
    {
        spdlog::warn("Cannot play on {}: it isn't paired", target.key.toStdString());
        const FailureText text = notPairedText();
        fail(text.title, text.message);
        return;
    }

    if (target.psn && !renewAccessToken())
    {
        spdlog::warn("The PSN sign-in has expired");
        psnFailure_ = remoteplay::PsnFailure{
            .stage = remoteplay::ConnectionStage::SigningIn,
            .kind = remoteplay::PsnFailureKind::Rejected,
            .detail = "the PSN sign-in has expired",
        };
        const FailureText text = textFor(*psnFailure_);
        fail(text.title, text.message);
        return;
    }

#if defined(_WIN32)
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
        .remote = target.remote,
        .ps5 = target.ps5,
        .credentials = target.credentials.value_or(remoteplay::Credentials{}),
        .video = settings_.video(),
        .audio = settings_.audio(),
        .gamepads = input_.gamepads(),
        .keyboardMouse = keyboardMouse_.get(),
        .metrics = metrics_,
        .psn = target.psn,
        .autoRegist = registering_,
    };
    auto makeDecoder = [&gpu, frameMailbox = *mailbox, metrics = metrics_](
                           core::VideoCodec codec, bool hdr) -> media::VideoDecoderResult
    {
        auto decoder = media::VideoDecoder::create(*gpu, codec, frameMailbox, metrics, hdr);

        if (!decoder)
        {
            return std::unexpected(decoder.error());
        }

        return std::move(*decoder);
    };

    auto started = remoteplay::StreamSession::start(std::move(makeDecoder), request,
                                                    eventsFor(session));

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
#else
    spdlog::error("Cannot play: streaming is not supported on this platform yet");
    fail(u"Transmissão indisponível"_s, u"A transmissão ainda não é suportada neste sistema."_s);
#endif
}

remoteplay::StreamSession::Events StreamController::eventsFor(std::uint64_t session)
{
    return {
        .onConnected = [this, session] { post([this, session] { onConnected(session); }); },
#if defined(_WIN32)
        .onFirstPicture = [this, session] { post([this, session] { onFirstPicture(session); }); },
#endif
        .onStage = [this, session](remoteplay::ConnectionStage stage)
        { post([this, session, stage] { onStage(session, stage); }); },
        .onPsnFailed = [this, session](remoteplay::PsnFailure failure)
        { post([this, session, failure = std::move(failure)] { onPsnFailed(session, failure); }); },
        .onRegistered = [this, session](remoteplay::PairedConsole paired)
        { post([this, session, paired = std::move(paired)] { onRegistered(session, paired); }); },
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

    sessionConnected_ = true;
    psnRetries_ = 0;
    setPhase(Phase::StartingVideo);
}

void StreamController::onStage(std::uint64_t session, remoteplay::ConnectionStage stage)
{
    if (session != sessionNumber_)
    {
        return;
    }

    connectionStage_ = stage;
    emit changed();
}

void StreamController::onPsnFailed(std::uint64_t session, remoteplay::PsnFailure failure)
{
    if (session != sessionNumber_)
    {
        return;
    }

    psnFailure_ = std::move(failure);
}

void StreamController::onRegistered(std::uint64_t session,
                                    const remoteplay::PairedConsole& paired)
{
    if (session != sessionNumber_)
    {
        return;
    }

    registered_ = true;

    if (onPaired_)
    {
        onPaired_(paired);
    }
}

#if defined(_WIN32)
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
            const QString name = QString::fromLatin1(core::reflexModeName(mode));
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
        .statisticsModeChanged = [this](core::StatisticsMode mode)
        { post([this, mode] { settings_.setOverlayStatisticsMode(mode); }); },
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
                    .statisticsMode = settings_.overlayStatisticsMode(),
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
#endif

void StreamController::onEnded(std::uint64_t session, SessionEnd end)
{
    if (session != sessionNumber_)
    {
        return;
    }

    const bool overPsn = overInternet();
    stop();

    if (registering_)
    {
        // The console accepted the pairing; the session quits right after.
        if (registered_)
        {
            setPhase(Phase::Idle);
            return;
        }

        const FailureText text = registrationText(psnFailure_);
        fail(text.title, text.message);
        return;
    }

    const bool recovering = sessionConnected_ || psnRetries_ > 0;

    if (overPsn && shouldRetryPsn(end, kindOf(psnFailure_), recovering, psnRetries_))
    {
        scheduleReconnect();
        return;
    }

    if (end == SessionEnd::Stopped || end == SessionEnd::EndedByConsole)
    {
        setPhase(Phase::Idle);
        emit notice(u"info"_s, u"O PS5 encerrou a transmissão"_s, QString());
        return;
    }

    const FailureText text = textForEnd(end, psnFailure_, overPsn, psnRetries_);
    fail(text.title, text.message);
}

void StreamController::scheduleReconnect()
{
    ++psnRetries_;
    spdlog::warn("The connection dropped; reconnecting ({} of {})", psnRetries_,
                 remoteplay::kMaxPsnRetries);
    setPhase(Phase::Connecting);
    emit changed();

    const std::uint64_t pending = sessionNumber_;
    QTimer::singleShot(kReconnectWait, this,
                       [this, pending]
                       {
                           // A cancel or a new play in the wait moves the session on.
                           if (pending != sessionNumber_ || phase_ != Phase::Connecting)
                           {
                               return;
                           }

                           startSession();
                       });
}

bool StreamController::renewAccessToken()
{
    if (!target_ || !target_->psn || !accessTokenSource_)
    {
        return true;
    }

    std::string token = accessTokenSource_();

    if (token.empty())
    {
        return false;
    }

    target_->psn->accessToken = std::move(token);
    return true;
}

#if defined(_WIN32)
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
#endif

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

#if defined(_WIN32)
    window_.reset();
#endif
    session_.reset();
#if defined(_WIN32)
    mailbox_.reset();
#endif
    metrics_.reset();
    keyboardMouse_.reset();
#if defined(_WIN32)
    gpu_.reset();
#endif
}

void StreamController::fail(const QString& title, const QString& message)
{
    const bool wasStreaming = phase_ == Phase::Streaming;
    failureHeading_ = wasStreaming ? u"A transmissão parou"_s : connectFailedHeading(registering_);
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
