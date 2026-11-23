#include "core/Settings.h"

#include "core/Files.h"
#include "core/KeyboardBindings.h"

#include <algorithm>
#include <array>
#include <format>
#include <fstream>
#include <istream>
#include <optional>
#include <ostream>
#include <sstream>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace lynx::core
{

namespace
{

using Json = nlohmann::ordered_json;

constexpr int kJsonIndent = 2;

template <typename Value, std::size_t Size>
using Names = std::array<std::pair<std::string_view, Value>, Size>;

constexpr Names<VideoResolution, 4> kResolutionNames{{
    {"360p", VideoResolution::P360},
    {"540p", VideoResolution::P540},
    {"720p", VideoResolution::P720},
    {"1080p", VideoResolution::P1080},
}};

constexpr Names<VideoCodec, 2> kCodecNames{{
    {"h264", VideoCodec::H264},
    {"h265", VideoCodec::H265},
}};

constexpr Names<PresentMode, 2> kPresentModeNames{{
    {"vsync", PresentMode::VSync},
    {"lowestLatency", PresentMode::LowestLatency},
}};

constexpr Names<HdrMode, 3> kHdrNames{{
    {"off", HdrMode::Off},
    {"auto", HdrMode::Auto},
    {"rtx", HdrMode::RtxVideoHdr},
}};

constexpr Names<ReflexMode, 3> kReflexNames{{
    {"off", ReflexMode::Off},
    {"on", ReflexMode::On},
    {"boost", ReflexMode::Boost},
}};

constexpr Names<Upscaling, 3> kUpscalingNames{{
    {"shader", Upscaling::Shader},
    {"vsr", Upscaling::Vsr},
    {"dlss", Upscaling::Dlss},
}};

constexpr Names<FrameGenerationMode, 2> kFrameGenerationNames{{
    {"off", FrameGenerationMode::Off},
    {"2x", FrameGenerationMode::X2},
}};

template <typename Value, std::size_t Size>
std::string nameOf(const Names<Value, Size>& names, Value value)
{
    const auto found = std::ranges::find(names, value, &std::pair<std::string_view, Value>::second);

    return std::string(found != names.end() ? found->first : names.front().first);
}

// The value named at `key`, or `fallback` when the name is missing or unknown.
template <typename Value, std::size_t Size>
Value namedValue(const Names<Value, Size>& names, const Json& object, const std::string& key,
                 Value fallback)
{
    const auto entry = object.find(key);

    if (entry == object.end() || !entry->is_string())
    {
        return fallback;
    }

    const std::string name = entry->get<std::string>();
    const auto found = std::ranges::find(names, std::string_view(name),
                                         &std::pair<std::string_view, Value>::first);

    return found != names.end() ? found->second : fallback;
}

std::optional<std::int64_t> integerAt(const Json& object, const std::string& key)
{
    const auto entry = object.find(key);

    if (entry == object.end() || !entry->is_number_integer())
    {
        return std::nullopt;
    }

    return entry->get<std::int64_t>();
}

std::string stringAt(const Json& object, const std::string& key, std::string fallback)
{
    const auto entry = object.find(key);

    if (entry == object.end() || !entry->is_string())
    {
        return fallback;
    }

    return entry->get<std::string>();
}

bool booleanAt(const Json& object, const std::string& key, bool fallback)
{
    const auto entry = object.find(key);

    if (entry == object.end() || !entry->is_boolean())
    {
        return fallback;
    }

    return entry->get<bool>();
}

std::optional<double> numberAt(const Json& object, const std::string& key)
{
    const auto entry = object.find(key);

    if (entry == object.end() || !entry->is_number())
    {
        return std::nullopt;
    }

    return entry->get<double>();
}

VideoSettings readVideo(const Json& video)
{
    VideoSettings settings;

    if (!video.is_object())
    {
        return settings;
    }

    settings.resolution = namedValue(kResolutionNames, video, "resolution", settings.resolution);
    settings.codec = namedValue(kCodecNames, video, "codec", settings.codec);
    settings.presentMode =
        namedValue(kPresentModeNames, video, "presentMode", settings.presentMode);
    settings.hdr = namedValue(kHdrNames, video, "hdr", settings.hdr);

    const auto framesPerSecond = integerAt(video, "fps");

    if (framesPerSecond == 30 || framesPerSecond == 60)
    {
        settings.framesPerSecond = static_cast<int>(*framesPerSecond);
    }

    const auto bitrate = integerAt(video, "bitrateKbps");
    const bool bitrateInRange =
        bitrate && *bitrate >= kMinBitrateKbps && *bitrate <= kMaxBitrateKbps;

    if (bitrateInRange)
    {
        settings.bitrateKbps = static_cast<std::uint32_t>(*bitrate);
    }

    return settings;
}

AudioSettings readAudio(const Json& audio)
{
    AudioSettings settings;

    if (!audio.is_object())
    {
        return settings;
    }

    settings.outputDevice = stringAt(audio, "outputDevice", settings.outputDevice);
    settings.microphoneDevice = stringAt(audio, "microphoneDevice", settings.microphoneDevice);
    settings.microphoneMuted = booleanAt(audio, "microphoneMuted", settings.microphoneMuted);

    const auto volume = integerAt(audio, "volumePercent");

    if (volume && *volume >= 0 && *volume <= 100)
    {
        settings.volumePercent = static_cast<int>(*volume);
    }

    return settings;
}

ControllerSettings readController(const Json& controller)
{
    ControllerSettings settings;

    if (!controller.is_object())
    {
        return settings;
    }

    settings.preferred = stringAt(controller, "preferred", settings.preferred);

    return settings;
}

RtxSettings readRtx(const Json& rtx)
{
    RtxSettings settings;

    if (!rtx.is_object())
    {
        return settings;
    }

    settings.reflex = namedValue(kReflexNames, rtx, "reflex", settings.reflex);

    const auto upscaling = rtx.find("upscaling");

    if (upscaling != rtx.end() && upscaling->is_string())
    {
        settings.upscaling = namedValue(kUpscalingNames, rtx, "upscaling", settings.upscaling);
    }
    else if (booleanAt(rtx, "videoSuperResolution", false))
    {
        // The field before the selector existed: on meant VSR, off the shader.
        settings.upscaling = Upscaling::Vsr;
    }

    settings.opticalFlow = booleanAt(rtx, "opticalFlow", settings.opticalFlow);
    settings.frameGeneration =
        namedValue(kFrameGenerationNames, rtx, "frameGeneration", settings.frameGeneration);

    return settings;
}

OverlaySettings readOverlay(const Json& overlay)
{
    OverlaySettings settings;

    if (!overlay.is_object())
    {
        return settings;
    }

    settings.statisticsVisible =
        booleanAt(overlay, "statisticsVisible", settings.statisticsVisible);

    return settings;
}

KeyboardMouseSettings readKeyboardMouse(const Json& keyboardMouse)
{
    KeyboardMouseSettings settings;

    if (!keyboardMouse.is_object())
    {
        return settings;
    }

    if (const auto bindings = keyboardMouse.find("bindings");
        bindings != keyboardMouse.end() && bindings->is_object())
    {
        for (std::size_t index = 0; index < kKeyboardControlCount; ++index)
        {
            const auto entry = bindings->find(std::string(kKeyboardControlNames.at(index)));

            if (entry == bindings->end() || !entry->is_string())
            {
                continue;
            }

            const std::string name = entry->get<std::string>();

            if (keyboardBindingFromName(name))
            {
                settings.bindings.at(index) = name;
            }
        }
    }

    if (const auto sensitivity = numberAt(keyboardMouse, "mouseSensitivity");
        sensitivity && *sensitivity >= kMinMouseSensitivity &&
        *sensitivity <= kMaxMouseSensitivity)
    {
        settings.mouseSensitivity = *sensitivity;
    }

    settings.invertY = booleanAt(keyboardMouse, "invertY", settings.invertY);

    return settings;
}

PsnSettings readPsn(const Json& psn)
{
    PsnSettings settings;

    if (!psn.is_object())
    {
        return settings;
    }

    settings.signedIn = booleanAt(psn, "signedIn", settings.signedIn);
    settings.tokenExpiry = integerAt(psn, "tokenExpiry");

    return settings;
}

} // namespace

std::array<std::string, kKeyboardControlCount> defaultKeyboardBindings()
{
    return {
        "W",
        "S",
        "A",
        "D",
        "Mouse",
        "Space",
        "Q",
        "R",
        "F",
        "Shift",
        "E",
        "MouseRight",
        "MouseLeft",
        "Enter",
        "Tab",
    };
}

KeyboardMouseSettings assignKeyboardBinding(KeyboardMouseSettings settings, KeyboardControl control,
                                            std::string_view binding)
{
    const std::size_t index = static_cast<std::size_t>(control);

    if (index >= kKeyboardControlCount || binding.empty() || !keyboardBindingFromName(binding))
    {
        return settings;
    }

    const std::string replacement(binding);
    const std::string previous = settings.bindings.at(index);

    if (previous == replacement)
    {
        return settings;
    }

    for (std::size_t other = 0; other < kKeyboardControlCount; ++other)
    {
        if (other != index && settings.bindings.at(other) == replacement)
        {
            settings.bindings.at(other) = previous;
            break;
        }
    }

    settings.bindings.at(index) = replacement;
    return settings;
}

std::expected<Settings, std::string> readSettings(std::istream& input)
{
    const Json root = Json::parse(input, nullptr, false);

    if (root.is_discarded() || !root.is_object())
    {
        return std::unexpected(std::string("not JSON"));
    }

    const auto version = integerAt(root, "version");

    if (version != kSettingsVersion)
    {
        return std::unexpected(std::format("version {} instead of {}", version.value_or(0),
                                           kSettingsVersion));
    }

    Settings settings;

    if (const auto video = root.find("video"); video != root.end())
    {
        settings.video = readVideo(*video);
    }

    if (const auto audio = root.find("audio"); audio != root.end())
    {
        settings.audio = readAudio(*audio);
    }

    if (const auto controller = root.find("controller"); controller != root.end())
    {
        settings.controller = readController(*controller);
    }

    if (const auto rtx = root.find("rtx"); rtx != root.end())
    {
        settings.rtx = readRtx(*rtx);
    }

    if (const auto overlay = root.find("overlay"); overlay != root.end())
    {
        settings.overlay = readOverlay(*overlay);
    }

    if (const auto keyboardMouse = root.find("keyboardMouse");
        keyboardMouse != root.end())
    {
        settings.keyboardMouse = readKeyboardMouse(*keyboardMouse);
    }

    if (const auto psn = root.find("psn"); psn != root.end())
    {
        settings.psn = readPsn(*psn);
    }

    return settings;
}

void writeSettings(const Settings& settings, std::ostream& output)
{
    const VideoSettings& video = settings.video;
    const AudioSettings& audio = settings.audio;
    const ControllerSettings& controller = settings.controller;
    const RtxSettings& rtx = settings.rtx;
    const OverlaySettings& overlay = settings.overlay;
    const KeyboardMouseSettings& keyboardMouse = settings.keyboardMouse;
    const PsnSettings& psn = settings.psn;

    Json bindings = Json::object();

    for (std::size_t index = 0; index < kKeyboardControlCount; ++index)
    {
        bindings[std::string(kKeyboardControlNames.at(index))] = keyboardMouse.bindings.at(index);
    }

    Json psnEntry{
        {"signedIn", psn.signedIn},
    };

    if (psn.tokenExpiry)
    {
        psnEntry["tokenExpiry"] = *psn.tokenExpiry;
    }

    const Json root{
        {"version", kSettingsVersion},
        {"video",
         {
             {"resolution", nameOf(kResolutionNames, video.resolution)},
             {"fps", video.framesPerSecond},
             {"codec", nameOf(kCodecNames, video.codec)},
             {"bitrateKbps", video.bitrateKbps},
             {"presentMode", nameOf(kPresentModeNames, video.presentMode)},
             {"hdr", nameOf(kHdrNames, video.hdr)},
         }},
        {"audio",
         {
             {"outputDevice", audio.outputDevice},
             {"microphoneDevice", audio.microphoneDevice},
             {"microphoneMuted", audio.microphoneMuted},
             {"volumePercent", audio.volumePercent},
         }},
        {"controller",
         {
             {"preferred", controller.preferred},
         }},
        {"rtx",
         {
             {"reflex", nameOf(kReflexNames, rtx.reflex)},
             {"upscaling", nameOf(kUpscalingNames, rtx.upscaling)},
             {"opticalFlow", rtx.opticalFlow},
             {"frameGeneration", nameOf(kFrameGenerationNames, rtx.frameGeneration)},
         }},
        {"overlay",
         {
             {"statisticsVisible", overlay.statisticsVisible},
         }},
        {"keyboardMouse",
         {
             {"bindings", bindings},
             {"mouseSensitivity", keyboardMouse.mouseSensitivity},
             {"invertY", keyboardMouse.invertY},
         }},
        {"psn", psnEntry},
    };

    output << root.dump(kJsonIndent) << '\n';
}

SettingsStore::SettingsStore(std::filesystem::path path) : path_(std::move(path))
{
}

Settings SettingsStore::load() const
{
    std::ifstream input(path_, std::ios::binary);

    if (!input)
    {
        return {};
    }

    auto settings = readSettings(input);

    if (!settings)
    {
        spdlog::warn("{} is unreadable ({}); using the default settings", path_.string(),
                     settings.error());
        return {};
    }

    return *settings;
}

std::expected<void, std::string> SettingsStore::save(const Settings& settings) const
{
    std::ostringstream buffer;
    writeSettings(settings, buffer);

    return replaceFile(path_, buffer.str());
}

} // namespace lynx::core
