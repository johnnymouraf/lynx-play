#pragma once

#include "core/RtxCapabilities.h"
#include "core/Settings.h"
#include "render/VideoFrame.h"

#include <string>
#include <string_view>

#include <windows.h>

namespace mfplay::render
{

using core::HdrDisplayState;

// Queries DISPLAYCONFIG_DEVICE_INFO_GET_ADVANCED_COLOR_INFO_2 for the monitor
// that shows `window` (the primary monitor when it is null). Never fails: a
// display it can't query reads as SDR.
[[nodiscard]] HdrDisplayState detectHdrDisplay(HWND window);

// What the renderer does with the stream on this display.
struct HdrPresentation
{
    // R10G10B10A2_UNORM with the HDR10 color space, rather than R8G8B8A8.
    bool hdrSwapChain = false;
    // The stream is PQ and the surface is SDR: the scaling pass converts it.
    bool toneMap = false;
    // RTX Video HDR converts this SDR stream on the video processor.
    bool rtxVideoHdr = false;
    // The stream itself carries PQ and BT.2020 primaries.
    bool hdrStream = false;
};

[[nodiscard]] HdrPresentation hdrPresentationFor(core::HdrMode mode,
                                                 const HdrDisplayState& display,
                                                 TransferFunction stream,
                                                 bool rtxHdrAvailable);

// The scaling pass's tone map: PQ in, SDR gamma out.
struct ToneMapParameters
{
    bool enabled = false;
    float contentPeakNits = 1000.0F;
    float sdrWhiteNits = 200.0F;
};

// SMPTE ST.2084's transfer, exposed for the shader's constants and the tests.
[[nodiscard]] double pqDecodeNits(double code);
[[nodiscard]] double pqEncodeNits(double nits);

// The tone map's curve: linear light in nits to linear light where 1.0 is
// `sdrWhiteNits`, with `contentPeakNits` mapped to white. Extended Reinhard.
[[nodiscard]] double toneMapLinear(double nits, double contentPeakNits, double sdrWhiteNits);

// An sRGB-encoded channel from 0 to 1 to a PQ code from 0 to 1, placing SDR
// white at `sdrWhiteNits` in the HDR range.
[[nodiscard]] double srgbToPq(double channel, double sdrWhiteNits);

[[nodiscard]] std::string_view transferName(TransferFunction transfer);
[[nodiscard]] std::string_view primariesName(ColorPrimaries primaries);

} // namespace mfplay::render
