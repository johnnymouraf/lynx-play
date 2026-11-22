#include "media/FilePlayer.h"

#include "core/FramePacer.h"
#include "core/Windows.h"
#include "media/AnnexB.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <format>
#include <fstream>
#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::media
{

namespace
{

// Test streams are small; this only keeps a wrong file from filling memory.
constexpr std::uintmax_t kMaxFileBytes = 1024ULL * 1024 * 1024;
// Two seconds of failures at 60 fps: the file won't decode at all.
constexpr std::uint32_t kMaxFailuresInRow = 120;

constexpr std::array<std::pair<std::wstring_view, core::VideoCodec>, 5> kExtensions{{
    {L".h264", core::VideoCodec::H264},
    {L".264", core::VideoCodec::H264},
    {L".h265", core::VideoCodec::H265},
    {L".265", core::VideoCodec::H265},
    {L".hevc", core::VideoCodec::H265},
}};

std::string displayName(const std::filesystem::path& path)
{
    return core::toUtf8(path.native());
}

std::expected<std::vector<std::uint8_t>, std::string> readFile(const std::filesystem::path& path)
{
    std::error_code error;
    const std::uintmax_t size = std::filesystem::file_size(path, error);

    if (error)
    {
        return std::unexpected(std::format("{}: {}", displayName(path), error.message()));
    }

    if (size > kMaxFileBytes)
    {
        return std::unexpected(std::format("{} is larger than 1 GiB", displayName(path)));
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream file(path, std::ios::binary);
    file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    if (!file)
    {
        return std::unexpected(std::format("{} can't be read", displayName(path)));
    }

    return bytes;
}

} // namespace

std::optional<core::VideoCodec> codecOfFile(const std::filesystem::path& path)
{
    std::wstring extension = path.extension().wstring();
    const auto lower = [](wchar_t letter) { return static_cast<wchar_t>(std::towlower(letter)); };
    std::ranges::transform(extension, extension.begin(), lower);

    const auto known = std::ranges::find(kExtensions, std::wstring_view(extension),
                                         &std::pair<std::wstring_view, core::VideoCodec>::first);

    if (known == kExtensions.end())
    {
        return std::nullopt;
    }

    return known->second;
}

std::expected<std::unique_ptr<FilePlayer>, std::string> FilePlayer::start(
    const render::GpuDevice& gpu, std::shared_ptr<render::FrameMailbox> mailbox,
    const std::filesystem::path& path, double framesPerSecond,
    std::shared_ptr<core::StreamMetrics> metrics)
{
    const auto codec = codecOfFile(path);

    if (!codec)
    {
        return std::unexpected(
            std::format("{} isn't an .h264 or .h265 elementary stream", displayName(path)));
    }

    auto stream = readFile(path);

    if (!stream)
    {
        return std::unexpected(stream.error());
    }

    auto decoder = VideoDecoder::create(gpu, *codec, std::move(mailbox), std::move(metrics));

    if (!decoder)
    {
        return std::unexpected(decoder.error());
    }

    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<FilePlayer> player(
        new FilePlayer(std::move(*stream), *codec, std::move(*decoder), framesPerSecond));

    if (player->accessUnits_.empty())
    {
        return std::unexpected(std::format("{} holds no pictures", displayName(path)));
    }

    spdlog::info("Playing {} in a loop: {} {} pictures at {} fps", displayName(path),
                 player->accessUnits_.size(), core::codecName(*codec), framesPerSecond);

    FilePlayer* const self = player.get();
    player->thread_ = std::jthread([self](const std::stop_token& stop) { self->run(stop); });

    return player;
}

FilePlayer::FilePlayer(std::vector<std::uint8_t> stream, core::VideoCodec codec,
                       std::unique_ptr<VideoDecoder> decoder, double framesPerSecond)
    : stream_(std::move(stream)),
      accessUnits_(splitAccessUnits(stream_, codec)),
      decoder_(std::move(decoder)),
      framesPerSecond_(framesPerSecond)
{
}

FilePlayer::~FilePlayer()
{
    if (thread_.joinable())
    {
        thread_.request_stop();
        thread_.join();
    }

    spdlog::info("File playback stopped");
}

void FilePlayer::run(const std::stop_token& stop)
{
    SetThreadDescription(GetCurrentThread(), L"Lynx file player");

    auto pacer = core::FramePacer::create(framesPerSecond_);

    if (!pacer)
    {
        spdlog::error("File playback can't start: {}", pacer.error());
        return;
    }

    std::size_t next = 0;
    std::uint32_t failuresInRow = 0;

    while (pacer->waitForNextTick(stop))
    {
        const auto decoded = decoder_->decode(accessUnits_[next]);
        next = (next + 1) % accessUnits_.size();

        if (decoded)
        {
            failuresInRow = 0;
            continue;
        }

        if (failuresInRow == 0)
        {
            spdlog::warn("Decoding failed: {}", decoded.error());
        }

        if (++failuresInRow == kMaxFailuresInRow)
        {
            spdlog::error("The file doesn't decode; playback stopped");
            return;
        }
    }
}

} // namespace lynx::media
