#include "remoteplay/StreamSession.h"

#include "media/VideoDecoder.h"
#include "remoteplay/ChiakiLogSink.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <span>
#include <utility>

#include <chiaki/session.h>
#include <spdlog/spdlog.h>

namespace lynx::remoteplay
{

namespace
{

using Clock = std::chrono::steady_clock;

// chiaki-ng's default: the packet loss reported to the console tops out here.
constexpr double kPacketLossMax = 0.05;
// A keyframe takes a round trip to arrive; asking more often only adds load.
constexpr auto kKeyframeRequestInterval = std::chrono::milliseconds(500);
// Two seconds of failures at 60 fps: the stream doesn't decode at all.
constexpr std::uint32_t kMaxDecodeFailuresInRow = 120;

struct ResolutionSize
{
    core::VideoResolution resolution;
    std::uint32_t width;
    std::uint32_t height;
};

constexpr std::array<ResolutionSize, 4> kResolutionSizes{{
    {core::VideoResolution::P360, 640, 360},
    {core::VideoResolution::P540, 960, 540},
    {core::VideoResolution::P720, 1280, 720},
    {core::VideoResolution::P1080, 1920, 1080},
}};

SessionEnd sessionEndOf(ChiakiQuitReason reason)
{
    switch (reason)
    {
    case CHIAKI_QUIT_REASON_STOPPED:
        return SessionEnd::Stopped;
    case CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_SHUTDOWN:
        return SessionEnd::EndedByConsole;
    case CHIAKI_QUIT_REASON_SESSION_REQUEST_CONNECTION_REFUSED:
    case CHIAKI_QUIT_REASON_CTRL_CONNECTION_REFUSED:
        return SessionEnd::ConnectionRefused;
    case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_IN_USE:
        return SessionEnd::InUse;
    case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_CRASH:
        return SessionEnd::ConsoleCrashed;
    case CHIAKI_QUIT_REASON_SESSION_REQUEST_RP_VERSION_MISMATCH:
        return SessionEnd::VersionMismatch;
    case CHIAKI_QUIT_REASON_SESSION_REQUEST_UNKNOWN:
    case CHIAKI_QUIT_REASON_CTRL_CONNECT_FAILED:
        return SessionEnd::ConsoleNotFound;
    case CHIAKI_QUIT_REASON_STREAM_CONNECTION_UNKNOWN:
    case CHIAKI_QUIT_REASON_STREAM_CONNECTION_REMOTE_DISCONNECTED:
        return SessionEnd::Disconnected;
    case CHIAKI_QUIT_REASON_NONE:
    case CHIAKI_QUIT_REASON_CTRL_UNKNOWN:
    case CHIAKI_QUIT_REASON_PSN_REGIST_FAILED:
        return SessionEnd::Unknown;
    }

    return SessionEnd::Unknown;
}

ChiakiConnectVideoProfile chiakiProfileOf(const VideoProfile& profile)
{
    ChiakiConnectVideoProfile chiaki{};
    chiaki.width = profile.width;
    chiaki.height = profile.height;
    chiaki.max_fps = profile.framesPerSecond;
    chiaki.bitrate = profile.bitrateKbps;
    chiaki.codec = profile.codec == core::VideoCodec::H265 ? CHIAKI_CODEC_H265 : CHIAKI_CODEC_H264;

    return chiaki;
}

} // namespace

VideoProfile videoProfileFor(const core::VideoSettings& settings)
{
    const auto size =
        std::ranges::find(kResolutionSizes, settings.resolution, &ResolutionSize::resolution);
    const ResolutionSize& chosen = size != kResolutionSizes.end() ? *size : kResolutionSizes.back();

    return {
        .width = chosen.width,
        .height = chosen.height,
        .framesPerSecond = static_cast<std::uint32_t>(settings.framesPerSecond),
        .bitrateKbps = settings.bitrateKbps,
        .codec = settings.codec,
    };
}

struct StreamSession::State
{
    Events events;
    std::string host;
    ChiakiLogSink logSink;
    std::unique_ptr<media::VideoDecoder> decoder;
    ChiakiSession session{};
    bool initialized = false;
    bool started = false;
    std::atomic<bool> showedPicture = false;
    // Why this PC stopped the session, reported instead of "stopped".
    std::atomic<bool> videoFailed = false;
    std::atomic<bool> loginPinRequested = false;
    // Only touched on chiaki's video thread.
    std::uint32_t decodeFailuresInRow = 0;
    Clock::time_point lastKeyframeRequest{};

    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;
    ~State();

    bool decode(std::span<const std::uint8_t> sample);
    void requestKeyframe();
    void handle(const ChiakiEvent& event);
    [[nodiscard]] SessionEnd endOf(ChiakiQuitReason reason) const;

    static bool onVideoSample(std::uint8_t* buffer, std::size_t size, std::int32_t framesLost,
                              bool frameRecovered, void* user);
    static void onEvent(ChiakiEvent* event, void* user);
};

StreamSession::State::~State()
{
    if (started)
    {
        chiaki_session_stop(&session);
        chiaki_session_join(&session);
    }

    if (initialized)
    {
        chiaki_session_fini(&session);
    }
}

bool StreamSession::State::onVideoSample(std::uint8_t* buffer, std::size_t size, std::int32_t,
                                         bool, void* user)
{
    auto& state = *static_cast<State*>(user);
    return state.decode({buffer, size});
}

bool StreamSession::State::decode(std::span<const std::uint8_t> sample)
{
    const auto decoded = decoder->decode(sample);

    if (decoded)
    {
        decodeFailuresInRow = 0;

        if (!showedPicture.exchange(true))
        {
            events.onFirstPicture();
        }

        return true;
    }

    if (decodeFailuresInRow == 0)
    {
        spdlog::warn("Video decoding failed: {}", decoded.error());
    }

    requestKeyframe();

    if (++decodeFailuresInRow == kMaxDecodeFailuresInRow)
    {
        spdlog::error("The video doesn't decode; ending the session");
        videoFailed = true;
        chiaki_session_stop(&session);
    }

    return false;
}

void StreamSession::State::requestKeyframe()
{
    const Clock::time_point now = Clock::now();

    if (now - lastKeyframeRequest < kKeyframeRequestInterval)
    {
        return;
    }

    lastKeyframeRequest = now;

    if (const ChiakiErrorCode result = chiaki_session_request_idr(&session);
        result != CHIAKI_ERR_SUCCESS)
    {
        spdlog::warn("Asking for a keyframe failed: {}", chiaki_error_string(result));
    }
}

void StreamSession::State::onEvent(ChiakiEvent* event, void* user)
{
    static_cast<State*>(user)->handle(*event);
}

void StreamSession::State::handle(const ChiakiEvent& event)
{
    switch (event.type)
    {
    case CHIAKI_EVENT_CONNECTED:
        spdlog::info("Session connected to {}", host);
        events.onConnected();
        return;

    case CHIAKI_EVENT_LOGIN_PIN_REQUEST:
        spdlog::warn("The console asks for a login PIN; ending the session");
        loginPinRequested = true;
        chiaki_session_stop(&session);
        return;

    case CHIAKI_EVENT_QUIT:
    {
        const std::string detail = event.quit.reason_str != nullptr ? event.quit.reason_str : "";
        spdlog::info("Session ended: {}{}{}", chiaki_quit_reason_string(event.quit.reason),
                     detail.empty() ? "" : ", ", detail);
        events.onEnded(endOf(event.quit.reason), detail);
        return;
    }

    default:
        return;
    }
}

SessionEnd StreamSession::State::endOf(ChiakiQuitReason reason) const
{
    if (videoFailed)
    {
        return SessionEnd::VideoFailed;
    }

    if (loginPinRequested)
    {
        return SessionEnd::LoginPinRequired;
    }

    return sessionEndOf(reason);
}

std::expected<std::unique_ptr<StreamSession>, std::string> StreamSession::start(
    const render::GpuDevice& gpu, std::shared_ptr<render::FrameMailbox> mailbox,
    const SessionRequest& request, Events events)
{
    auto state = std::make_unique<State>();
    state->events = std::move(events);
    state->host = request.host;

    auto decoder = media::VideoDecoder::create(gpu, request.video.codec, std::move(mailbox));

    if (!decoder)
    {
        return std::unexpected(decoder.error());
    }

    state->decoder = std::move(*decoder);

    ChiakiConnectInfo info{};
    info.ps5 = request.ps5;
    info.host = state->host.c_str();
    std::memcpy(info.regist_key, request.credentials.registKey.data(), sizeof(info.regist_key));
    std::memcpy(info.morning, request.credentials.rpKey.data(), sizeof(info.morning));
    info.video_profile = chiakiProfileOf(videoProfileFor(request.video));
    info.video_profile_auto_downgrade = true;
    info.audio_video_disabled = CHIAKI_NONE_DISABLED;
    info.packet_loss_max = kPacketLossMax;
    info.enable_idr_on_fec_failure = true;

    if (const ChiakiErrorCode result =
            chiaki_session_init(&state->session, &info, state->logSink.log());
        result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_session_init failed: ") +
                               chiaki_error_string(result));
    }

    state->initialized = true;
    chiaki_session_set_video_sample_cb(&state->session, &State::onVideoSample, state.get());
    chiaki_session_set_event_cb(&state->session, &State::onEvent, state.get());

    if (const ChiakiErrorCode result = chiaki_session_start(&state->session);
        result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_session_start failed: ") +
                               chiaki_error_string(result));
    }

    state->started = true;
    const VideoProfile profile = videoProfileFor(request.video);
    spdlog::info("Session started with {}: {}x{} at {} fps, {}, {} kbps", request.host,
                 profile.width, profile.height, profile.framesPerSecond,
                 core::codecName(profile.codec), profile.bitrateKbps);

    return std::unique_ptr<StreamSession>(new StreamSession(std::move(state)));
}

StreamSession::StreamSession(std::unique_ptr<State> state) : state_(std::move(state))
{
}

StreamSession::~StreamSession() = default;

} // namespace lynx::remoteplay
