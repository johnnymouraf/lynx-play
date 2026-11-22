#include "app/FilePlayback.h"

#include "media/FilePlayer.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"
#include "render/StreamWindow.h"

#include <QCoreApplication>

#include <cstdlib>

#include <spdlog/spdlog.h>

namespace lynx::app
{

int playFile(const std::filesystem::path& path, double framesPerSecond,
             render::PresentTiming timing)
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

    auto player = media::FilePlayer::start(*gpu, *mailbox, path, framesPerSecond);

    if (!player)
    {
        spdlog::error("Can't play the file: {}", player.error());
        return EXIT_FAILURE;
    }

    const auto quit = [](render::CloseReason) { QCoreApplication::quit(); };
    auto window = render::StreamWindow::open(
        *gpu, std::move(*mailbox),
        {
            .title = L"Lynx Play — " + path.filename().wstring(),
            .timing = timing,
            .onClosed = quit,
        });

    if (!window)
    {
        spdlog::error("Can't play the file: {}", window.error());
        return EXIT_FAILURE;
    }

    return QCoreApplication::exec();
}

} // namespace lynx::app
