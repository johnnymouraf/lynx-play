#include "app/AppController.h"

#include "app/Version.h"
#include "render/StreamWindow.h"

#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::app
{

namespace
{

constexpr auto kVideoTestTitle = L"Lynx Play — Teste de vídeo";

} // namespace

AppController::AppController(StartupState state, QObject* parent)
    : QObject(parent), state_(std::move(state))
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

bool AppController::openVideoTest()
{
    if (videoTest_ != nullptr && videoTest_->isOpen())
    {
        videoTest_->bringToFront();
        return true;
    }

    auto window = render::StreamWindow::open(kVideoTestTitle);

    if (!window)
    {
        spdlog::error("Video test unavailable: {}", window.error());
        return false;
    }

    videoTest_ = std::move(*window);
    return true;
}

} // namespace lynx::app
