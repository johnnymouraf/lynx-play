#include "app/FilePlayback.h"

#include "app/Fonts.h"
#include "app/OverlayInput.h"
#include "app/SettingsController.h"
#include "core/StreamMetrics.h"
#include "input/GamepadManager.h"
#include "input/Sdl.h"
#include "media/FilePlayer.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"
#include "render/ReflexSession.h"
#include "render/StreamWindow.h"

#include <QCoreApplication>
#include <QTimer>

#include <cstdlib>
#include <memory>
#include <utility>

#include <SDL3/SDL.h>
#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

constexpr int kGamepadPollMilliseconds = 4;

std::unique_ptr<input::GamepadManager> openGamepads(const SettingsController& settings)
{
    auto gamepads = input::GamepadManager::create(settings.preferredController().toStdString());

    if (!gamepads)
    {
        spdlog::warn("Gamepads are unavailable for the file playback: {}", gamepads.error());
        return nullptr;
    }

    return std::move(*gamepads);
}

} // namespace

int playFile(const std::filesystem::path& path, double framesPerSecond,
             SettingsController& settings)
{
    const auto gpu = render::createGpuDevice();

    if (!gpu)
    {
        spdlog::error("Can't play the file: {}", gpu.error());
        return EXIT_FAILURE;
    }

    auto mailbox = render::FrameMailbox::create();

    if (!mailbox)
    {
        spdlog::error("Can't play the file: {}", mailbox.error());
        return EXIT_FAILURE;
    }

    auto metrics = std::make_shared<core::StreamMetrics>();
    auto player = media::FilePlayer::start(*gpu, *mailbox, path, framesPerSecond, metrics);

    if (!player)
    {
        spdlog::error("Can't play the file: {}", player.error());
        return EXIT_FAILURE;
    }

    // The quick menu works from a controller here too, so this path keeps its
    // own SDL pump.
    auto sdl = input::SdlRuntime::create();

    if (!sdl)
    {
        spdlog::warn("Gamepads are unavailable for the file playback: {}", sdl.error());
    }

    std::unique_ptr<input::GamepadManager> gamepads;

    if (sdl)
    {
        gamepads = openGamepads(settings);
    }

    QTimer gamepadTimer;
    gamepadTimer.setInterval(kGamepadPollMilliseconds);
    gamepadTimer.setTimerType(Qt::PreciseTimer);

    QObject::connect(&gamepadTimer, &QTimer::timeout,
                     [&gamepads]
                     {
                         SDL_PumpEvents();
                         SDL_Event event;

                         while (SDL_PollEvent(&event))
                         {
                             if (gamepads != nullptr)
                             {
                                 gamepads->handleEvent(event);
                             }
                         }

                         if (gamepads != nullptr)
                         {
                             gamepads->tick();
                         }
                     });

    if (gamepads != nullptr)
    {
        gamepadTimer.start();
    }

    const auto quit = [](render::CloseReason) { QCoreApplication::quit(); };

    const render::OverlayActions overlayActions{
        .reflexChanged = [&settings](core::ReflexMode mode)
        {
            const QString name = QString::fromLatin1(render::reflexModeName(mode));
            QMetaObject::invokeMethod(
                &settings, [&settings, name] { settings.setReflex(name); }, Qt::QueuedConnection);
        },
        .microphoneMutedChanged = [&settings](bool muted)
        {
            QMetaObject::invokeMethod(
                &settings, [&settings, muted] { settings.setMicrophoneMuted(muted); },
                Qt::QueuedConnection);
        },
        .statisticsVisibleChanged = [&settings](bool visible)
        {
            QMetaObject::invokeMethod(
                &settings,
                [&settings, visible] { settings.setOverlayStatisticsVisible(visible); },
                Qt::QueuedConnection);
        },
        .windowLayoutChanged = [&settings](core::WindowMode mode, core::WindowSize size)
        {
            QMetaObject::invokeMethod(
                &settings, [&settings, mode, size] { settings.setWindowLayout(mode, size); },
                Qt::QueuedConnection);
        },
        .disconnect = []
        { QMetaObject::invokeMethod(qApp, [] { QCoreApplication::quit(); }, Qt::QueuedConnection); },
    };

    auto window = render::StreamWindow::open(
        *gpu, std::move(*mailbox),
        {
            .title = L"Lynx Play — " + path.filename().wstring(),
            .timing = settings.presentTiming(),
            .reflex = settings.reflexMode(),
            .upscaling = settings.upscalingMode(),
            .opticalFlow = settings.opticalFlow(),
            .frameGeneration = settings.frameGenerationMode(),
            .hdr = settings.hdrMode(),
            .rtxVideoHdrAvailable = settings.rtxVideoHdrAvailable(),
            .mode = settings.video().windowMode,
            .windowedSize = settings.video().windowSize,
            .overlayFonts = overlayFonts(),
            .overlay =
                {
                    .statisticsVisible = settings.overlayStatisticsVisible(),
                    .reflex = settings.reflexMode(),
                    .microphoneMuted = settings.microphoneMuted(),
                    .microphoneDetail = gamepads != nullptr && gamepads->activeName().has_value()
                                             ? "DualSense"
                                             : "Sem controle",
                    .sessionName = "Arquivo de teste",
                    .windowMode = settings.video().windowMode,
                    .windowSize = settings.video().windowSize,
                },
            .overlayActions = overlayActions,
            .metrics = metrics,
            .gamepad = [&gamepads]
            {
                if (gamepads == nullptr)
                {
                    return render::OverlayGamepad{};
                }

                const std::optional<input::GamepadSample> sample = gamepads->sample();

                return sample ? overlayGamepad(*sample) : render::OverlayGamepad{};
            },
            .onClosed = quit,
            .onWindowModeChanged = [&settings](core::WindowMode mode)
            {
                const QString name =
                    QString::fromLatin1(core::windowModeName(mode).data());
                QMetaObject::invokeMethod(
                    &settings, [&settings, name] { settings.setWindowMode(name); },
                    Qt::QueuedConnection);
            },
            .onWindowedSizeChanged = [&settings](core::WindowSize size)
            {
                QMetaObject::invokeMethod(
                    &settings, [&settings, size] { settings.setWindowedSize(size); },
                    Qt::QueuedConnection);
            },
        });

    if (!window)
    {
        spdlog::error("Can't play the file: {}", window.error());
        return EXIT_FAILURE;
    }

    // The Vídeo screen and the menu change the same settings; apply them to
    // the running window through their signal.
    render::StreamWindow* const streamWindow = window->get();
    QObject layoutContext;
    QObject::connect(&settings, &SettingsController::videoChanged, &layoutContext,
                     [&settings, streamWindow]
                     {
                         streamWindow->applyWindowLayout(settings.video().windowMode,
                                                         settings.video().windowSize);
                     });

    return QCoreApplication::exec();
}

} // namespace lynx::app
