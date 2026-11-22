#include "app/AppController.h"

#include "app/SettingsController.h"
#include "app/Version.h"
#include "core/Paths.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"
#include "render/StreamWindow.h"
#include "render/TestPatternSource.h"

#include <QCoreApplication>

#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

using namespace Qt::StringLiterals;

constexpr auto kVideoTestTitle = L"Lynx Play — Teste de vídeo";

} // namespace

AppController::AppController(StartupState state, const SettingsController& settings,
                             QObject* parent)
    : QObject(parent), state_(std::move(state)), settings_(settings)
{
}

AppController::~AppController() = default;

QString AppController::version() const
{
    return QString::fromUtf8(kVersion.data(), static_cast<qsizetype>(kVersion.size()));
}

bool AppController::remotePlayReady() const
{
    return state_.remotePlayReady;
}

QString AppController::gpuName() const
{
    return state_.gpuName;
}

QUrl AppController::licensesUrl() const
{
    return QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + u"/licenses"_s);
}

QUrl AppController::logsUrl() const
{
    const auto dataDir = core::localDataDir();

    if (!dataDir)
    {
        return {};
    }

    return QUrl::fromLocalFile(QString::fromStdWString((*dataDir / "logs").wstring()));
}

bool AppController::openVideoTest()
{
    if (videoTestWindow_ != nullptr && videoTestWindow_->isOpen())
    {
        videoTestWindow_->bringToFront();
        return true;
    }

    closeVideoTest();

    if (const auto started = startVideoTest(); !started)
    {
        spdlog::error("Video test unavailable: {}", started.error());
        return false;
    }

    return true;
}

std::expected<void, std::string> AppController::startVideoTest()
{
    const auto gpu = render::createGpuDevice();

    if (!gpu)
    {
        return std::unexpected(gpu.error());
    }

    auto mailbox = render::FrameMailbox::create();

    if (!mailbox)
    {
        return std::unexpected(mailbox.error());
    }

    auto source = render::TestPatternSource::start(*gpu, *mailbox);

    if (!source)
    {
        return std::unexpected(source.error());
    }

    // The window is gone by then; the source and the window object go on the
    // next turn of the event loop.
    const auto closeLater = [this](render::CloseReason)
    { QMetaObject::invokeMethod(this, &AppController::closeVideoTest, Qt::QueuedConnection); };

    auto window = render::StreamWindow::open(*gpu, std::move(*mailbox),
                                             {
                                                 .title = kVideoTestTitle,
                                                 .timing = settings_.presentTiming(),
                                                 .onClosed = closeLater,
                                             });

    if (!window)
    {
        return std::unexpected(window.error());
    }

    videoTestSource_ = std::move(*source);
    videoTestWindow_ = std::move(*window);
    return {};
}

void AppController::closeVideoTest()
{
    videoTestWindow_.reset();
    videoTestSource_.reset();
}

} // namespace lynx::app
