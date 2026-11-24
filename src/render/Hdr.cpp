#include "render/Hdr.h"

#include "core/Windows.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

namespace mfplay::render
{

namespace
{

// SMPTE ST.2084's constants.
constexpr double kPqM1 = 2610.0 / 16384.0;
constexpr double kPqM2 = 2523.0 / 4096.0 * 128.0;
constexpr double kPqC1 = 3424.0 / 4096.0;
constexpr double kPqC2 = 2413.0 / 4096.0 * 32.0;
constexpr double kPqC3 = 2392.0 / 4096.0 * 32.0;
constexpr double kPqPeakNits = 10000.0;

// DisplayConfig reports 80-nit units for SDR white; below 200 the tone map
// would come out too dark, and above 480 it would wash the picture out.
constexpr double kMinSdrWhiteNits = 200.0;
constexpr double kMaxSdrWhiteNits = 480.0;
constexpr double kDefaultSdrWhiteNits = 200.0;
constexpr UINT32 kDisplayConfigAttempts = 2;

std::optional<DISPLAYCONFIG_PATH_INFO> activePathFor(std::wstring_view gdiDeviceName)
{
    for (UINT32 attempt = 0; attempt < kDisplayConfigAttempts; ++attempt)
    {
        UINT32 pathCount = 0;
        UINT32 modeCount = 0;
        constexpr UINT32 kFlags = QDC_ONLY_ACTIVE_PATHS | QDC_VIRTUAL_MODE_AWARE;

        if (GetDisplayConfigBufferSizes(kFlags, &pathCount, &modeCount) != ERROR_SUCCESS)
        {
            return std::nullopt;
        }

        std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
        const LONG result = QueryDisplayConfig(kFlags, &pathCount, paths.data(), &modeCount,
                                               modes.data(), nullptr);

        if (result == ERROR_INSUFFICIENT_BUFFER)
        {
            continue;
        }

        if (result != ERROR_SUCCESS)
        {
            return std::nullopt;
        }

        paths.resize(pathCount);

        for (const DISPLAYCONFIG_PATH_INFO& path : paths)
        {
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{};
            source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
            source.header.size = static_cast<UINT32>(sizeof(source));
            source.header.adapterId = path.sourceInfo.adapterId;
            source.header.id = path.sourceInfo.id;

            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS)
            {
                continue;
            }

            if (std::wstring_view(source.viewGdiDeviceName) == gdiDeviceName)
            {
                return path;
            }
        }

        return std::nullopt;
    }

    return std::nullopt;
}

std::string displayNameOf(const DISPLAYCONFIG_PATH_INFO& path, std::wstring_view fallback)
{
    DISPLAYCONFIG_TARGET_DEVICE_NAME target{};
    target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    target.header.size = static_cast<UINT32>(sizeof(target));
    target.header.adapterId = path.targetInfo.adapterId;
    target.header.id = path.targetInfo.id;

    const bool named = DisplayConfigGetDeviceInfo(&target.header) == ERROR_SUCCESS &&
                       target.monitorFriendlyDeviceName[0] != L'\0';

    return core::toUtf8(named ? std::wstring_view(target.monitorFriendlyDeviceName) : fallback);
}

} // namespace

HdrDisplayState detectHdrDisplay(HWND window)
{
    HdrDisplayState state;
    state.sdrWhiteNits = kDefaultSdrWhiteNits;

    const HMONITOR monitor =
        window != nullptr
            ? MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST)
            : MonitorFromPoint(POINT{.x = 0, .y = 0}, MONITOR_DEFAULTTOPRIMARY);
    // In C++ the extended struct derives from MONITORINFO, so the fields are
    // inherited and can't be designated.
    MONITORINFOEXW monitorInfo{};
    monitorInfo.cbSize = static_cast<DWORD>(sizeof(MONITORINFOEXW));

    if (!GetMonitorInfoW(monitor, &monitorInfo))
    {
        state.name = "unknown";
        spdlog::debug("HDR display: the monitor under the window could not be read");
        return state;
    }

    const auto path = activePathFor(monitorInfo.szDevice);

    if (!path)
    {
        state.name = core::toUtf8(monitorInfo.szDevice);
        spdlog::debug("HDR display: DisplayConfig has no active path for {}", state.name);
        return state;
    }

    state.name = displayNameOf(*path, monitorInfo.szDevice);

    DISPLAYCONFIG_GET_ADVANCED_COLOR_INFO_2 color{};
    color.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2;
    color.header.size = static_cast<UINT32>(sizeof(color));
    color.header.adapterId = path->targetInfo.adapterId;
    color.header.id = path->targetInfo.id;

    if (DisplayConfigGetDeviceInfo(&color.header) == ERROR_SUCCESS)
    {
        state.hdrSupported = color.highDynamicRangeSupported != 0;
        state.hdrActive = color.highDynamicRangeUserEnabled != 0 &&
                          color.activeColorMode == DISPLAYCONFIG_ADVANCED_COLOR_MODE_HDR;
    }
    else
    {
        spdlog::debug("HDR display: DisplayConfig could not read the advanced color state");
    }

    DISPLAYCONFIG_SDR_WHITE_LEVEL white{};
    white.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL;
    white.header.size = static_cast<UINT32>(sizeof(white));
    white.header.adapterId = path->targetInfo.adapterId;
    white.header.id = path->targetInfo.id;

    if (DisplayConfigGetDeviceInfo(&white.header) == ERROR_SUCCESS && white.SDRWhiteLevel > 0)
    {
        const double nits = static_cast<double>(white.SDRWhiteLevel) / 1000.0 * 80.0;
        state.sdrWhiteNits = std::clamp(nits, kMinSdrWhiteNits, kMaxSdrWhiteNits);
    }

    return state;
}

HdrPresentation hdrPresentationFor(core::HdrMode mode, const HdrDisplayState& display,
                                   TransferFunction stream, bool rtxHdrAvailable)
{
    const bool hdrStream = stream == TransferFunction::Pq;
    const bool rtxVideoHdr = mode == core::HdrMode::RtxVideoHdr && !hdrStream &&
                             display.hdrActive && rtxHdrAvailable;
    const bool hdrSwapChain =
        display.hdrActive && mode != core::HdrMode::Off && (hdrStream || rtxVideoHdr);

    return {
        .hdrSwapChain = hdrSwapChain,
        .toneMap = hdrStream && !hdrSwapChain,
        .rtxVideoHdr = rtxVideoHdr,
        .hdrStream = hdrStream,
    };
}

double pqDecodeNits(double code)
{
    const double coded = std::pow(std::clamp(code, 0.0, 1.0), 1.0 / kPqM2);
    const double numerator = std::max(coded - kPqC1, 0.0);
    const double denominator = kPqC2 - kPqC3 * coded;

    if (denominator <= 0.0)
    {
        return kPqPeakNits;
    }

    return kPqPeakNits * std::pow(numerator / denominator, 1.0 / kPqM1);
}

double pqEncodeNits(double nits)
{
    const double normalized = std::clamp(nits, 0.0, kPqPeakNits) / kPqPeakNits;
    const double power = std::pow(normalized, kPqM1);

    return std::pow((kPqC1 + kPqC2 * power) / (1.0 + kPqC3 * power), kPqM2);
}

double toneMapLinear(double nits, double contentPeakNits, double sdrWhiteNits)
{
    const double white = std::max(sdrWhiteNits, 1.0);
    const double peak = std::max(contentPeakNits, white) / white;
    const double linear = std::max(nits, 0.0) / white;
    const double mapped = linear * (1.0 + linear / (peak * peak)) / (1.0 + linear);

    return std::clamp(mapped, 0.0, 1.0);
}

double srgbToPq(double channel, double sdrWhiteNits)
{
    const double value = std::clamp(channel, 0.0, 1.0);
    const double linear = value <= 0.04045 ? value / 12.92
                                           : std::pow((value + 0.055) / 1.055, 2.4);

    return pqEncodeNits(linear * std::max(sdrWhiteNits, 1.0));
}

std::string_view transferName(TransferFunction transfer)
{
    switch (transfer)
    {
    case TransferFunction::Sdr:
        return "SDR (BT.709)";
    case TransferFunction::Pq:
        return "PQ (SMPTE ST.2084)";
    }

    return "SDR (BT.709)";
}

std::string_view primariesName(ColorPrimaries primaries)
{
    switch (primaries)
    {
    case ColorPrimaries::Bt709:
        return "BT.709";
    case ColorPrimaries::Bt2020:
        return "BT.2020";
    }

    return "BT.709";
}

} // namespace mfplay::render
