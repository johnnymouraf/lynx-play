#pragma once

#include "core/Settings.h"
#include "remoteplay/Console.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace lynx::remoteplay
{

// Why a session ended, as the interface tells it.
enum class SessionEnd
{
    // Stopped from this PC.
    Stopped,
    // The console ended it: turned off, or the stream closed on its side.
    EndedByConsole,
    ConnectionRefused,
    // Another device is already playing on the console.
    InUse,
    ConsoleCrashed,
    VersionMismatch,
    // No answer: off, asleep or on another network.
    ConsoleNotFound,
    Disconnected,
    // The console's user asks for a login PIN, which isn't supported yet.
    LoginPinRequired,
    // The pictures don't decode.
    VideoFailed,
    Unknown,
};

// What the console is asked to stream.
struct VideoProfile
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t framesPerSecond = 0;
    std::uint32_t bitrateKbps = 0;
    core::VideoCodec codec = core::VideoCodec::H265;

    bool operator==(const VideoProfile&) const = default;
};

[[nodiscard]] VideoProfile videoProfileFor(const core::VideoSettings& settings);

struct SessionRequest
{
    // The console's address on the local network.
    std::string host;
    bool ps5 = true;
    Credentials credentials;
    core::VideoSettings video;
};

// A Remote Play session with a paired console, through chiaki-lib, its video
// decoded on the GPU into a mailbox.
class StreamSession
{
public:
    // Each is called at most once, on chiaki's threads.
    struct Events
    {
        std::function<void()> onConnected;
        std::function<void()> onFirstPicture;
        std::function<void(SessionEnd end, std::string detail)> onEnded;
    };

    [[nodiscard]] static std::expected<std::unique_ptr<StreamSession>, std::string> start(
        const render::GpuDevice& gpu, std::shared_ptr<render::FrameMailbox> mailbox,
        const SessionRequest& request, Events events);

    StreamSession(const StreamSession&) = delete;
    StreamSession& operator=(const StreamSession&) = delete;
    StreamSession(StreamSession&&) = delete;
    StreamSession& operator=(StreamSession&&) = delete;

    // Stops the session if it still runs and waits for chiaki's threads. Never
    // call it from an event.
    ~StreamSession();

private:
    struct State;

    explicit StreamSession(std::unique_ptr<State> state);

    std::unique_ptr<State> state_;
};

} // namespace lynx::remoteplay
