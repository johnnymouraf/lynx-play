#pragma once

#include "core/VideoCodec.h"
#include "media/VideoDecoder.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace lynx::media
{

// The codec of an elementary stream file, from its extension (.h264, .264,
// .h265, .265, .hevc).
[[nodiscard]] std::optional<core::VideoCodec> codecOfFile(const std::filesystem::path& path);

// A video source for development: plays an H.264 or H.265 elementary stream
// file in a loop through the GPU decoder, one access unit per tick of a fixed
// frame rate.
class FilePlayer
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<FilePlayer>, std::string> start(
        const render::GpuDevice& gpu, std::shared_ptr<render::FrameMailbox> mailbox,
        const std::filesystem::path& path, double framesPerSecond);

    FilePlayer(const FilePlayer&) = delete;
    FilePlayer& operator=(const FilePlayer&) = delete;
    FilePlayer(FilePlayer&&) = delete;
    FilePlayer& operator=(FilePlayer&&) = delete;

    // Stops playing and closes the decoder.
    ~FilePlayer();

private:
    FilePlayer(std::vector<std::uint8_t> stream, core::VideoCodec codec,
               std::unique_ptr<VideoDecoder> decoder, double framesPerSecond);

    void run(const std::stop_token& stop);

    std::vector<std::uint8_t> stream_;
    std::vector<std::span<const std::uint8_t>> accessUnits_;
    std::unique_ptr<VideoDecoder> decoder_;
    double framesPerSecond_ = 0.0;
    // Last, so the thread starts after every member exists and stops first.
    std::jthread thread_;
};

} // namespace lynx::media
