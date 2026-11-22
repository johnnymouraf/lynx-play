#include "app/StreamController.h"

#include "app/SettingsController.h"

#include <QGuiApplication>
#include <QWindow>

#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::app
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
    return u"Veja o log em %LOCALAPPDATA%\\LynxPlay\\logs."_s;
}

FailureText notPairedText()
{
    return {u"Este console não está pareado"_s,
            u"Pareie o PS5 com este PC pelo menu do card e tente de novo."_s};
}

FailureText noGpuText()
{
    return {u"Nenhuma GPU pode mostrar o vídeo"_s,
            u"O Lynx Play precisa de uma GPU com Direct3D 12. "_s + logsHint()};
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
                u"O PS5 pede uma versão da Reprodução remota que o Lynx Play ainda não tem."_s};
    case SessionEnd::ConsoleNotFound:
        return {u"O console não respondeu"_s,
                u"Confira se o PS5 está ligado e na mesma rede que este PC."_s};
    case SessionEnd::Disconnected:
        return {u"A transmissão caiu"_s,
                u"A conexão com o PS5 foi interrompida. Confira a rede e tente de novo."_s};
    case SessionEnd::LoginPinRequired:
        return {u"O PS5 pediu o PIN de login"_s,
                u"Desative o PIN de login do usuário no PS5 para jogar pelo Lynx Play."_s};
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

StreamController::StreamController(const SettingsController& settings, QObject* parent)
    : QObject(parent), settings_(settings)
{
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

    const remoteplay::SessionRequest request{
        .host = target.host,
        .ps5 = target.ps5,
        .credentials = *target.credentials,
        .video = settings_.video(),
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

    auto window = render::StreamWindow::open(*gpu_, mailbox_,
                                             {
                                                 .title = L"Lynx Play — " +
                                                          target_->name.toStdWString(),
                                                 .timing = settings_.presentTiming(),
                                                 .mode = render::WindowMode::Fullscreen,
                                                 .monitorOf = mainWindow_,
                                                 .onClosed = closed,
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
    window_.reset();
    session_.reset();
    mailbox_.reset();
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

} // namespace lynx::app
