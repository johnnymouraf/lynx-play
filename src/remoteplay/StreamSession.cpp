#include "remoteplay/StreamSession.h"

#include "audio/AudioOutput.h"
#include "audio/Haptics.h"
#include "audio/Microphone.h"
#include "input/ControllerMapping.h"
#include "input/DualSenseEffects.h"
#include "input/GamepadManager.h"
#include "input/KeyboardMouse.h"
#include "media/VideoDecoder.h"
#include "remoteplay/ChiakiLogSink.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <mutex>
#include <optional>
#include <span>
#include <stop_token>
#include <thread>
#include <utility>

#include <chiaki/opusdecoder.h>
#include <chiaki/opusencoder.h>
#include <chiaki/packetstats.h>
#include <chiaki/session.h>
#include <spdlog/spdlog.h>

namespace lynx::remoteplay
{

namespace
{

using SteadyClock = std::chrono::steady_clock;

// chiaki-ng's default: the packet loss reported to the console tops out here.
constexpr double kPacketLossMax = 0.05;
// A keyframe takes a round trip to arrive; asking more often only adds load.
constexpr auto kKeyframeRequestInterval = std::chrono::milliseconds(500);
// Two seconds of failures at 60 fps: the stream doesn't decode at all.
constexpr std::uint32_t kMaxDecodeFailuresInRow = 120;
// chiaki-ng's controller tick; the feedback sender paces itself from here.
constexpr auto kControllerInterval = std::chrono::milliseconds(4);
constexpr std::uint32_t kMicrophoneRate = 48000;
constexpr std::uint8_t kMicrophoneChannels = 2;
constexpr std::uint32_t kMicrophoneFrameSize = 480;

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
    chiaki.codec = profile.hdr
                       ? CHIAKI_CODEC_H265_HDR
                       : (profile.codec == core::VideoCodec::H265 ? CHIAKI_CODEC_H265
                                                                  : CHIAKI_CODEC_H264);

    return chiaki;
}

} // namespace

VideoProfile videoProfileFor(const core::VideoSettings& settings, bool ps5)
{
    const auto size =
        std::ranges::find(kResolutionSizes, settings.resolution, &ResolutionSize::resolution);
    const ResolutionSize& chosen = size != kResolutionSizes.end() ? *size : kResolutionSizes.back();
    // HDR is HEVC-only and PS5-only; choosing H.264 keeps the stream SDR.
    const bool hdr = ps5 && settings.hdr == core::HdrMode::Auto &&
                     settings.codec == core::VideoCodec::H265;

    return {
        .width = chosen.width,
        .height = chosen.height,
        .framesPerSecond = static_cast<std::uint32_t>(settings.framesPerSecond),
        .bitrateKbps = settings.bitrateKbps,
        .codec = settings.codec,
        .hdr = hdr,
    };
}

struct StreamSession::State
{
    Events events;
    std::string host;
    ChiakiLogSink logSink;
    std::unique_ptr<media::VideoDecoder> decoder;
    std::shared_ptr<core::StreamMetrics> metrics;
    input::GamepadManager* gamepads = nullptr;
    input::KeyboardMouseInput* keyboardMouse = nullptr;
    std::unique_ptr<audio::AudioOutput> audioOutput;
    std::unique_ptr<audio::Microphone> microphone;
    std::unique_ptr<audio::Haptics> haptics;
    ChiakiOpusDecoder opusDecoder{};
    ChiakiOpusEncoder opusEncoder{};
    bool opusDecoderReady = false;
    bool opusEncoderReady = false;
    ChiakiSession session{};
    bool initialized = false;
    bool started = false;
    std::atomic<bool> showedPicture = false;
    // Why this PC stopped the session, reported instead of "stopped".
    std::atomic<bool> videoFailed = false;
    std::atomic<bool> loginPinRequested = false;
    // Only touched on chiaki's video thread.
    std::uint32_t decodeFailuresInRow = 0;
    SteadyClock::time_point lastKeyframeRequest{};
    // Cumulative packet counts from the session; the congestion control resets
    // them, so each delta starts from what is left after a reset.
    std::uint64_t lastPacketsReceived = 0;
    std::uint64_t lastPacketsLost = 0;
    // The end reaches the interface once, whichever thread sees it first.
    std::atomic<bool> endReported = false;

    std::atomic<std::uint32_t> audioChannels{2};
    std::mutex microphoneMutex;
    bool microphoneMuted = false;
    bool microphoneConnected = false;
    std::atomic<bool> controlConnected{false};
    std::atomic<bool> microphoneActive{false};
    // Haptics are audible until the console says otherwise; both strengths
    // start at chiaki-ng's strong default.
    std::atomic<int> hapticIntensity{::Strong};
    std::atomic<int> triggerIntensity{::Strong};
    std::jthread inputThread;

    State() = default;
    State(const State&) = delete;
    State& operator=(const State&) = delete;
    State(State&&) = delete;
    State& operator=(State&&) = delete;
    ~State();

    bool decode(std::span<const std::uint8_t> sample);
    void requestKeyframe();
    void handle(const ChiakiEvent& event);
    void reportEnd(SessionEnd end, std::string detail);
    void reportNotice(std::string title, std::string message);
    [[nodiscard]] SessionEnd endOf(ChiakiQuitReason reason) const;

    void startInput();
    void inputLoop(const std::stop_token& stop);
    void connectMicrophone();
    void applyMicrophoneState();
    void handleMicrophoneLost();
    void encodeMicrophone(std::span<const std::int16_t> frames);
    void applyIntensity();

    static bool onVideoSample(std::uint8_t* buffer, std::size_t size, std::int32_t framesLost,
                              bool frameRecovered, void* user);
    static void onEvent(ChiakiEvent* event, void* user);
    static void onAudioSettings(std::uint32_t channels, std::uint32_t rate, void* user);
    static void onAudioFrame(std::int16_t* buffer, std::size_t samplesCount, void* user);
    static void onHapticsFrame(std::uint8_t* buffer, std::size_t size, void* user);
};

StreamSession::State::~State()
{
    inputThread.request_stop();
    inputThread = std::jthread{};

    if (gamepads != nullptr)
    {
        input::DualSenseTriggerEffect clear;
        clear.type = 0x05;
        gamepads->submit({.rumble = std::pair<std::uint16_t, std::uint16_t>{0, 0},
                           .leftTrigger = clear,
                           .rightTrigger = clear});
    }

    // The capture thread sends through the session's Opus encoder; stop it
    // before the session goes away.
    microphone.reset();

    if (started)
    {
        chiaki_session_stop(&session);
        chiaki_session_join(&session);
        started = false;
    }

    audioOutput.reset();
    haptics.reset();

    if (initialized)
    {
        chiaki_session_fini(&session);
        initialized = false;
    }

    if (opusEncoderReady)
    {
        chiaki_opus_encoder_fini(&opusEncoder);
        opusEncoderReady = false;
    }

    if (opusDecoderReady)
    {
        chiaki_opus_decoder_fini(&opusDecoder);
        opusDecoderReady = false;
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
    if (metrics != nullptr)
    {
        std::uint64_t received = 0;
        std::uint64_t lost = 0;
        chiaki_packet_stats_get(&session.stream_connection.packet_stats, false, &received, &lost);

        const std::uint64_t receivedDelta =
            received >= lastPacketsReceived ? received - lastPacketsReceived : received;
        const std::uint64_t lostDelta = lost >= lastPacketsLost ? lost - lastPacketsLost : lost;
        lastPacketsReceived = received;
        lastPacketsLost = lost;

        metrics->addPacketStats(SteadyClock::now(), receivedDelta, lostDelta);
    }

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
        // Stopping here would race chiaki's own shutdown on its video thread;
        // the interface stops the session after this report.
        reportEnd(SessionEnd::VideoFailed, "the video doesn't decode");
    }

    return false;
}

void StreamSession::State::requestKeyframe()
{
    const SteadyClock::time_point now = SteadyClock::now();

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

void StreamSession::State::onAudioSettings(std::uint32_t channels, std::uint32_t rate, void* user)
{
    auto& state = *static_cast<State*>(user);
    state.audioChannels = channels;
    spdlog::info("Stream audio: {} channels at {} Hz", channels, rate);

    if (state.audioOutput && !state.audioOutput->open(channels, rate))
    {
        state.reportNotice("Sem som no jogo",
                           "Nenhuma saída de áudio abriu. Confira a saída em Configurações › "
                           "Áudio e microfone.");
    }
}

void StreamSession::State::onAudioFrame(std::int16_t* buffer, std::size_t samplesCount, void* user)
{
    auto& state = *static_cast<State*>(user);

    if (state.audioOutput != nullptr)
    {
        state.audioOutput->push(buffer, samplesCount, state.audioChannels.load());
    }
}

void StreamSession::State::onHapticsFrame(std::uint8_t* buffer, std::size_t size, void* user)
{
    auto& state = *static_cast<State*>(user);

    if (state.haptics != nullptr)
    {
        state.haptics->push({buffer, size});
    }
}

void StreamSession::State::startInput()
{
    inputThread = std::jthread([this](const std::stop_token& stop) { inputLoop(stop); });
}

void StreamSession::State::inputLoop(const std::stop_token& stop)
{
    while (!stop.stop_requested())
    {
        // chiaki-ng combines every source through chiaki_controller_state_or,
        // so the controller and the keyboard never overwrite each other's
        // touches or sensor block.
        ChiakiControllerState controllerState;
        chiaki_controller_state_set_idle(&controllerState);

        if (gamepads != nullptr && !gamepads->overlayOpen())
        {
            if (const std::optional<ChiakiControllerState> state = gamepads->gamepadState();
                state)
            {
                ChiakiControllerState fromGamepad = *state;
                chiaki_controller_state_or(&controllerState, &controllerState, &fromGamepad);
            }
        }

        if (keyboardMouse != nullptr)
        {
            ChiakiControllerState fromKeyboard =
                input::toChiakiControllerState(keyboardMouse->sample());
            chiaki_controller_state_or(&controllerState, &controllerState, &fromKeyboard);
        }

        chiaki_session_set_controller_state(&session, &controllerState);

        const bool present = gamepads != nullptr && gamepads->activeName().has_value();
        const bool wired = gamepads != nullptr &&
                           gamepads->connection() == input::GamepadConnection::Wired;
        const audio::HapticsRoute previous = haptics->route();
        haptics->setController(present, wired);
        const audio::HapticsRoute route = haptics->route();

        if (route != previous && gamepads != nullptr)
        {
            gamepads->submit(
                {.classicRumble = route != audio::HapticsRoute::Audio});
        }

        std::this_thread::sleep_for(kControllerInterval);
    }
}

void StreamSession::State::connectMicrophone()
{
    const std::scoped_lock lock(microphoneMutex);
    controlConnected = true;

    if (!microphoneConnected &&
        chiaki_session_connect_microphone(&session) == CHIAKI_ERR_SUCCESS)
    {
        microphoneConnected = true;
        spdlog::info("Microphone connected to the console");
    }

    applyMicrophoneState();
}

void StreamSession::State::applyMicrophoneState()
{
    bool shouldMute = microphoneMuted;

    if (shouldMute)
    {
        microphone->setMuted(true);
    }
    else if (!microphone->open())
    {
        shouldMute = true;
        reportNotice(
            "Microfone indisponível",
            "Nenhum microfone abriu. Confira Configurações do Windows › Privacidade e segurança › "
            "Microfone.");
    }
    else
    {
        microphone->setMuted(false);
    }

    if (shouldMute != microphoneMuted)
    {
        microphoneMuted = shouldMute;

        if (events.onMicrophoneMuted)
        {
            events.onMicrophoneMuted(shouldMute);
        }
    }

    microphoneActive = controlConnected && !shouldMute;

    if (!controlConnected)
    {
        return;
    }

    if (const ChiakiErrorCode result = chiaki_session_toggle_microphone(&session, shouldMute);
        result != CHIAKI_ERR_SUCCESS)
    {
        spdlog::warn("Toggling the microphone failed: {}", chiaki_error_string(result));
    }
}

void StreamSession::State::handleMicrophoneLost()
{
    const std::scoped_lock lock(microphoneMutex);
    microphoneActive = false;

    if (microphoneMuted)
    {
        return;
    }

    microphoneMuted = true;

    if (controlConnected)
    {
        chiaki_session_toggle_microphone(&session, true);
    }

    if (events.onMicrophoneMuted)
    {
        events.onMicrophoneMuted(true);
    }

    reportNotice("Microfone desconectado",
                 "O microfone foi removido e o chat ficou mudo até ele voltar.");
}

void StreamSession::State::encodeMicrophone(std::span<const std::int16_t> frames)
{
    if (!microphoneActive.load() || frames.size() < kMicrophoneChannels * kMicrophoneFrameSize)
    {
        return;
    }

    chiaki_opus_encoder_frame(const_cast<std::int16_t*>(frames.data()), &opusEncoder);
}

void StreamSession::State::applyIntensity()
{
    const auto haptic = static_cast<ChiakiDualSenseEffectIntensity>(hapticIntensity.load());
    const auto trigger = static_cast<ChiakiDualSenseEffectIntensity>(triggerIntensity.load());

    if (gamepads != nullptr)
    {
        gamepads->submit({.intensity = input::intensityByte(haptic, trigger)});
    }

    if (haptics != nullptr)
    {
        haptics->setAmplitude(input::intensityScale(haptic));
    }
}

void StreamSession::State::reportNotice(std::string title, std::string message)
{
    spdlog::warn("{}: {}", title, message);

    if (events.onNotice)
    {
        events.onNotice(std::move(title), std::move(message));
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
        connectMicrophone();
        events.onConnected();
        return;

    case CHIAKI_EVENT_RUMBLE:
        if (gamepads != nullptr && hapticIntensity.load() != ::Off)
        {
            gamepads->submit(input::dualSenseCommand(event.rumble));
        }

        return;

    case CHIAKI_EVENT_TRIGGER_EFFECTS:
        if (gamepads != nullptr && triggerIntensity.load() != ::Off)
        {
            gamepads->submit(input::dualSenseCommand(event.trigger_effects));
        }

        return;

    case CHIAKI_EVENT_LED_COLOR:
        if (gamepads != nullptr)
        {
            gamepads->submit(input::ledCommand(event.led_state));
        }

        return;

    case CHIAKI_EVENT_PLAYER_INDEX:
        if (gamepads != nullptr)
        {
            gamepads->submit(input::playerIndexCommand(event.player_index));
        }

        return;

    case CHIAKI_EVENT_HAPTIC_INTENSITY:
        hapticIntensity = static_cast<int>(event.intensity);
        applyIntensity();
        return;

    case CHIAKI_EVENT_TRIGGER_INTENSITY:
        triggerIntensity = static_cast<int>(event.intensity);
        applyIntensity();
        return;

    case CHIAKI_EVENT_MOTION_RESET:
        if (gamepads != nullptr)
        {
            gamepads->resetMotion();
        }

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
        reportEnd(endOf(event.quit.reason), detail);
        return;
    }

    default:
        return;
    }
}

void StreamSession::State::reportEnd(SessionEnd end, std::string detail)
{
    if (endReported.exchange(true))
    {
        return;
    }

    events.onEnded(end, std::move(detail));
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
    state->gamepads = request.gamepads;
    state->keyboardMouse = request.keyboardMouse;
    state->metrics = request.metrics;
    state->microphoneMuted = request.audio.microphoneMuted;

    const VideoProfile profile = videoProfileFor(request.video, request.ps5);
    auto decoder = media::VideoDecoder::create(gpu, profile.codec, std::move(mailbox),
                                               request.metrics, profile.hdr);

    if (!decoder)
    {
        return std::unexpected(decoder.error());
    }

    state->decoder = std::move(*decoder);

    State* raw = state.get();
    state->audioOutput =
        std::make_unique<audio::AudioOutput>(request.audio.outputDevice, request.audio.volumePercent);
    state->audioOutput->setOnLost(
        [raw]
        {
            raw->reportNotice("O áudio voltou para a saída padrão",
                              "O dispositivo de áudio escolhido foi desconectado.");
        });

    state->haptics = std::make_unique<audio::Haptics>();
    state->haptics->setOnRumble(
        [raw](std::uint16_t left, std::uint16_t right)
        {
            if (raw->gamepads != nullptr)
            {
                raw->gamepads->submit({.rumble = std::pair{left, right}});
            }
        });

    state->microphone = std::make_unique<audio::Microphone>(
        request.audio.microphoneDevice,
        [raw](std::span<const std::int16_t> frames) { raw->encodeMicrophone(frames); });
    state->microphone->setOnLost([raw] { raw->handleMicrophoneLost(); });

    ChiakiConnectInfo info{};
    info.ps5 = request.ps5;
    info.host = state->host.c_str();
    std::memcpy(info.regist_key, request.credentials.registKey.data(), sizeof(info.regist_key));
    std::memcpy(info.morning, request.credentials.rpKey.data(), sizeof(info.morning));
    info.video_profile = chiakiProfileOf(profile);
    info.video_profile_auto_downgrade = true;
    info.enable_dualsense = true;
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

    chiaki_opus_decoder_init(&state->opusDecoder, state->logSink.log());
    state->opusDecoderReady = true;
    chiaki_opus_decoder_set_cb(&state->opusDecoder, &State::onAudioSettings, &State::onAudioFrame,
                                raw);
    ChiakiAudioSink audioSink{};
    chiaki_opus_decoder_get_sink(&state->opusDecoder, &audioSink);
    chiaki_session_set_audio_sink(&state->session, &audioSink);

    ChiakiAudioSink hapticsSink{};
    hapticsSink.user = raw;
    hapticsSink.frame_cb = &State::onHapticsFrame;
    chiaki_session_set_haptics_sink(&state->session, &hapticsSink);

    chiaki_opus_encoder_init(&state->opusEncoder, state->logSink.log());
    state->opusEncoderReady = true;
    ChiakiAudioHeader microphoneHeader{};
    chiaki_audio_header_set(&microphoneHeader, kMicrophoneChannels,
                            static_cast<std::uint8_t>(16), kMicrophoneRate,
                            kMicrophoneFrameSize);
    chiaki_opus_encoder_header(&microphoneHeader, &state->opusEncoder, &state->session);

    chiaki_session_set_video_sample_cb(&state->session, &State::onVideoSample, raw);
    chiaki_session_set_event_cb(&state->session, &State::onEvent, raw);

    if (const ChiakiErrorCode result = chiaki_session_start(&state->session);
        result != CHIAKI_ERR_SUCCESS)
    {
        return std::unexpected(std::string("chiaki_session_start failed: ") +
                               chiaki_error_string(result));
    }

    state->started = true;
    state->startInput();
    spdlog::info("Input: the controller state is sampled right before each send, every 4 ms");
    spdlog::info("Session started with {}: {}x{} at {} fps, {}, {} kbps", request.host,
                 profile.width, profile.height, profile.framesPerSecond,
                 core::codecName(profile.codec), profile.bitrateKbps);

    if (profile.hdr)
    {
        spdlog::info("HDR: the session requests CHIAKI_CODEC_H265_HDR (the PS5 sends 10-bit "
                     "P010)");
    }
    else if (request.video.hdr == core::HdrMode::Auto && request.ps5)
    {
        spdlog::info("HDR: automatic, but the stream stays SDR ({})",
                     request.video.codec == core::VideoCodec::H264
                         ? "HDR needs H.265"
                         : "HDR is off for this console");
    }

    return std::unique_ptr<StreamSession>(new StreamSession(std::move(state)));
}

StreamSession::StreamSession(std::unique_ptr<State> state) : state_(std::move(state))
{
}

StreamSession::~StreamSession() = default;

void StreamSession::setMicrophoneMuted(bool muted)
{
    const std::scoped_lock lock(state_->microphoneMutex);

    if (state_->microphoneMuted == muted)
    {
        return;
    }

    state_->microphoneMuted = muted;
    state_->applyMicrophoneState();
}

void StreamSession::setVolumePercent(int percent)
{
    if (state_->audioOutput != nullptr)
    {
        state_->audioOutput->setVolumePercent(percent);
    }
}

} // namespace lynx::remoteplay
