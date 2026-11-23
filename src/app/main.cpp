#include "app/AppController.h"
#include "app/ConsolesController.h"
#include "app/FilePlayback.h"
#include "app/Fonts.h"
#include "app/IconProvider.h"
#include "app/InputController.h"
#include "app/PsnLoginClient.h"
#include "app/QtLogBridge.h"
#include "app/SettingsController.h"
#include "app/StreamController.h"
#include "input/GamepadManager.h"
#include "input/Sdl.h"
#include "app/Version.h"
#include "core/Log.h"
#include "core/Latency.h"
#include "core/Paths.h"
#include "core/Settings.h"
#include "remoteplay/ChiakiRuntime.h"
#include "remoteplay/ConsoleStore.h"
#include "remoteplay/PsnTokenStore.h"
#include "render/AdapterSelection.h"
#include "render/GpuDevice.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVariantMap>

#include <cstdlib>
#include <filesystem>
#include <memory>
#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

// Hybrid-graphics laptops read these exports to run the process on the
// discrete GPU.
extern "C"
{
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

namespace
{

constexpr double kMinFileFramesPerSecond = 1.0;
constexpr double kMaxFileFramesPerSecond = 240.0;

// Developer options: `--play-file <file> [--fps <rate>]` plays an elementary
// stream instead of opening the interface.
struct CommandLine
{
    std::optional<std::filesystem::path> playFile;
    double framesPerSecond = 60.0;
};

void startLogging()
{
    const auto dataDir = lynx::core::localDataDir();

    if (!dataDir)
    {
        spdlog::error("No log file: {}", dataDir.error());
        return;
    }

    if (const auto logging = lynx::core::initLogging(*dataDir / "logs"); !logging)
    {
        spdlog::error("No log file: {}", logging.error());
    }
}

bool startRemotePlay()
{
    const auto chiaki = lynx::remoteplay::initializeChiaki();

    if (!chiaki)
    {
        spdlog::error("Remote Play is unavailable: {}", chiaki.error());
        return false;
    }

    spdlog::info("chiaki-lib initialized");
    return true;
}

std::optional<lynx::remoteplay::ConsoleStore> openConsoleStore()
{
    const auto dataDir = lynx::core::roamingDataDir();

    if (!dataDir)
    {
        spdlog::error("Saved consoles are unavailable: {}", dataDir.error());
        return std::nullopt;
    }

    return lynx::remoteplay::ConsoleStore(*dataDir / "consoles.json",
                                          lynx::remoteplay::dpapiProtector());
}

std::optional<lynx::core::SettingsStore> openSettingsStore()
{
    const auto dataDir = lynx::core::roamingDataDir();

    if (!dataDir)
    {
        spdlog::error("Settings are unavailable: {}", dataDir.error());
        return std::nullopt;
    }

    return lynx::core::SettingsStore(*dataDir / "settings.json");
}

std::optional<lynx::remoteplay::PsnTokenStore> openPsnTokenStore()
{
    const auto dataDir = lynx::core::roamingDataDir();

    if (!dataDir)
    {
        spdlog::error("The PSN tokens are unavailable: {}", dataDir.error());
        return std::nullopt;
    }

    return lynx::remoteplay::PsnTokenStore(*dataDir / "psn.json",
                                           lynx::remoteplay::dpapiProtector());
}

QString findStreamGpu()
{
    const auto adapters = lynx::render::listAdapters();

    if (!adapters)
    {
        spdlog::error("Cannot list GPUs: {}", adapters.error());
        return {};
    }

    for (const auto& adapter : *adapters)
    {
        spdlog::info("GPU found: {} (vendor 0x{:04X})", adapter.name, adapter.vendorId);
    }

    const auto chosen = lynx::render::selectAdapter(*adapters);

    if (!chosen)
    {
        spdlog::error("No hardware GPU can render the stream");
        return {};
    }

    return QString::fromStdString(adapters->at(*chosen).name);
}

// Shows Qt's message and exits on an unknown option; nullopt on a bad rate.
std::optional<CommandLine> parseCommandLine(const QGuiApplication& application)
{
    using namespace Qt::StringLiterals;

    const QCommandLineOption playFile(
        u"play-file"_s, u"Plays an H.264 or H.265 elementary stream in a loop."_s, u"file"_s);
    const QCommandLineOption framesPerSecond(u"fps"_s, u"The file's frame rate (60)."_s,
                                             u"rate"_s, u"60"_s);

    QCommandLineParser parser;
    parser.addOptions({playFile, framesPerSecond});
    parser.process(application);

    CommandLine commandLine;
    bool isNumber = false;
    commandLine.framesPerSecond = parser.value(framesPerSecond).toDouble(&isNumber);

    const bool inRange = commandLine.framesPerSecond >= kMinFileFramesPerSecond &&
                         commandLine.framesPerSecond <= kMaxFileFramesPerSecond;

    if (!isNumber || !inRange)
    {
        spdlog::error("--fps takes a rate from 1 to 240");
        return std::nullopt;
    }

    if (parser.isSet(playFile))
    {
        commandLine.playFile = parser.value(playFile).toStdWString();
    }

    return commandLine;
}

} // namespace

int main(int argc, char* argv[])
{
    startLogging();
    lynx::app::installQtLogBridge();
    spdlog::info("Lynx Play {} starting", lynx::app::kVersion);
    lynx::core::disableProcessPowerThrottling();

    QGuiApplication application(argc, argv);
    const auto commandLine = parseCommandLine(application);

    if (!commandLine)
    {
        return EXIT_FAILURE;
    }

    lynx::app::SettingsController settings(openSettingsStore());

    if (commandLine->playFile)
    {
        return lynx::app::playFile(*commandLine->playFile, commandLine->framesPerSecond, settings);
    }

    lynx::app::loadBundledFonts();

    lynx::app::AppController controller(
        {
            .remotePlayReady = startRemotePlay(),
            .gpuName = findStreamGpu(),
        },
        settings);
    QGuiApplication::setApplicationName(QStringLiteral("Lynx Play"));
    QGuiApplication::setApplicationVersion(controller.version());

    auto sdl = lynx::input::SdlRuntime::create();

    if (!sdl)
    {
        spdlog::error("Controllers and audio are unavailable: {}", sdl.error());
    }

    std::unique_ptr<lynx::input::GamepadManager> gamepads;

    if (sdl)
    {
        auto created = lynx::input::GamepadManager::create(
            settings.preferredController().toStdString());

        if (created)
        {
            gamepads = std::move(*created);
        }
        else
        {
            spdlog::error("Controllers are unavailable: {}", created.error());
        }
    }

    lynx::app::InputController input(settings, std::move(gamepads), sdl.has_value());
    lynx::app::StreamController stream(settings, input);
    lynx::app::PsnLoginClient psnLogin(settings, openPsnTokenStore());
    lynx::app::ConsolesController consoles(controller.remotePlayReady(), openConsoleStore(),
                                           stream, psnLogin);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new lynx::app::IconProvider);
    engine.setInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(&controller)},
        {QStringLiteral("consoles"), QVariant::fromValue(&consoles)},
        {QStringLiteral("settings"), QVariant::fromValue(&settings)},
        {QStringLiteral("stream"), QVariant::fromValue(&stream)},
        {QStringLiteral("input"), QVariant::fromValue(&input)},
    });

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("LynxPlay", "Main");

    const int exitCode = QGuiApplication::exec();
    spdlog::info("Lynx Play exiting with code {}", exitCode);

    return exitCode;
}
