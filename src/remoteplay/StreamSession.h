#pragma once

#include "core/Settings.h"
#include "core/StreamMetrics.h"
#include "remoteplay/Console.h"
#include "render/FrameMailbox.h"
#include "render/GpuDevice.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>

namespace mfplay::input
{
class GamepadManager;
class KeyboardMouseInput;
}

namespace mfplay::remoteplay
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
    // The stream asks for HDR through CHIAKI_CODEC_H265_HDR, not a flag.
    bool hdr = false;

    bool operator==(const VideoProfile&) const = default;
};

[[nodiscard]] VideoProfile videoProfileFor(const core::VideoSettings& settings, bool ps5 = true);

struct SessionRequest
{
    // The console's address on the local network.
    std::string host;
    bool ps5 = true;
    Credentials credentials;
    core::VideoSettings video;
    core::AudioSettings audio;
    // The controllers that drive the console; owned by the app. May be null.
    input::GamepadManager* gamepads = nullptr;
    // The keyboard and mouse the stream window records. May be null.
    input::KeyboardMouseInput* keyboardMouse = nullptr;
    // The decoder and session counters the overlay reads.
    std::shared_ptr<core::StreamMetrics> metrics;
};

// A Remote Play session with a paired console, through chiaki-lib, its video
// decoded on the GPU into a mailbox.
class StreamSession
{
public:
    // Called from the session's threads; the interface posts them to its own.
    struct Events
    {
        std::function<void()> onConnected;
        std::function<void()> onFirstPicture;
        std::function<void(SessionEnd end, std::string detail)> onEnded;
        // The microphone changed state on its own (it was lost, or could not
        // open); the interface keeps its toggle in sync.
        std::function<void(bool muted)> onMicrophoneMuted;
        // A problem worth telling the player about, without ending the stream.
        std::function<void(std::string title, std::string message)> onNotice;
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

    // Applies the interface's microphone toggle to the running console
    // connection.
    void setMicrophoneMuted(bool muted);
    void setVolumePercent(int percent);

    // The frame rate the console streams at, as chiaki holds it after the
    // console's answer. Valid once connected.
    [[nodiscard]] double framesPerSecond() const;

private:
    struct State;

    explicit StreamSession(std::unique_ptr<State> state);

    std::unique_ptr<State> state_;
};

} // namespace mfplay::remoteplay
