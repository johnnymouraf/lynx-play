#include "app/AppController.h"
#include "app/QtLogBridge.h"
#include "app/Version.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "remoteplay/ChiakiRuntime.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QVariantMap>

#include <cstdlib>

#include <spdlog/spdlog.h>

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

} // namespace

int main(int argc, char* argv[])
{
    startLogging();
    lynx::app::installQtLogBridge();
    spdlog::info("Lynx Play {} starting", lynx::app::kVersion);

    QGuiApplication application(argc, argv);
    lynx::app::AppController controller(startRemotePlay());
    QGuiApplication::setApplicationName(QStringLiteral("Lynx Play"));
    QGuiApplication::setApplicationVersion(controller.version());

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{QStringLiteral("controller"), QVariant::fromValue(&controller)}});

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &application,
        [] { QCoreApplication::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("LynxPlay", "Main");

    const int exitCode = QGuiApplication::exec();
    spdlog::info("Lynx Play exiting with code {}", exitCode);

    return exitCode;
}
