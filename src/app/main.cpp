#include "app/AppController.h"
#include "app/ConsolesController.h"
#include "app/Fonts.h"
#include "app/IconProvider.h"
#include "app/QtLogBridge.h"
#include "app/Version.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "remoteplay/ChiakiRuntime.h"
#include "remoteplay/ConsoleStore.h"
#include "render/AdapterSelection.h"
#include "render/GpuDevice.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVariantMap>

#include <cstdlib>
#include <optional>

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

} // namespace

int main(int argc, char* argv[])
{
    startLogging();
    lynx::app::installQtLogBridge();
    spdlog::info("Lynx Play {} starting", lynx::app::kVersion);

    QGuiApplication application(argc, argv);
    lynx::app::loadBundledFonts();

    lynx::app::AppController controller({
        .remotePlayReady = startRemotePlay(),
        .gpuName = findStreamGpu(),
    });
    QGuiApplication::setApplicationName(QStringLiteral("Lynx Play"));
    QGuiApplication::setApplicationVersion(controller.version());

    lynx::app::ConsolesController consoles(controller.remotePlayReady(), openConsoleStore());

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("icon"), new lynx::app::IconProvider);
    engine.setInitialProperties({
        {QStringLiteral("controller"), QVariant::fromValue(&controller)},
        {QStringLiteral("consoles"), QVariant::fromValue(&consoles)},
    });

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("LynxPlay", "Main");

    const int exitCode = QGuiApplication::exec();
    spdlog::info("Lynx Play exiting with code {}", exitCode);

    return exitCode;
}
