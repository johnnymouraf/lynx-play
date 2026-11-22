#include "render/Hdr.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

using namespace lynx;
using namespace lynx::render;

namespace
{

HdrDisplayState sdrDisplay()
{
    return {
        .name = "SDR panel",
        .hdrSupported = false,
        .hdrActive = false,
        .sdrWhiteNits = 200.0,
    };
}

HdrDisplayState hdrDisplay(bool active)
{
    return {
        .name = "HDR panel",
        .hdrSupported = true,
        .hdrActive = active,
        .sdrWhiteNits = 200.0,
    };
}

} // namespace

TEST_CASE("A PQ stream on an SDR display keeps the SDR chain and tone-maps")
{
    const HdrPresentation presentation =
        hdrPresentationFor(core::HdrMode::Auto, sdrDisplay(), TransferFunction::Pq, true);

    CHECK_FALSE(presentation.hdrSwapChain);
    CHECK(presentation.toneMap);
    CHECK_FALSE(presentation.rtxVideoHdr);
    CHECK(presentation.hdrStream);
}

TEST_CASE("An SDR stream keeps the SDR chain and is not tone-mapped")
{
    const HdrPresentation presentation =
        hdrPresentationFor(core::HdrMode::Auto, sdrDisplay(), TransferFunction::Sdr, true);

    CHECK_FALSE(presentation.hdrSwapChain);
    CHECK_FALSE(presentation.toneMap);
    CHECK_FALSE(presentation.rtxVideoHdr);
    CHECK_FALSE(presentation.hdrStream);
}

TEST_CASE("A PQ stream on an HDR display uses the HDR chain")
{
    const HdrPresentation presentation =
        hdrPresentationFor(core::HdrMode::Auto, hdrDisplay(true), TransferFunction::Pq, false);

    CHECK(presentation.hdrSwapChain);
    CHECK_FALSE(presentation.toneMap);
    CHECK_FALSE(presentation.rtxVideoHdr);
}

TEST_CASE("HDR off keeps the SDR chain even on an HDR display")
{
    const HdrPresentation presentation =
        hdrPresentationFor(core::HdrMode::Off, hdrDisplay(true), TransferFunction::Pq, true);

    CHECK_FALSE(presentation.hdrSwapChain);
    CHECK(presentation.toneMap);
}

TEST_CASE("RTX Video HDR converts an SDR stream on an HDR display")
{
    const HdrPresentation presentation = hdrPresentationFor(core::HdrMode::RtxVideoHdr,
                                                            hdrDisplay(true), TransferFunction::Sdr,
                                                            true);

    CHECK(presentation.hdrSwapChain);
    CHECK(presentation.rtxVideoHdr);
    CHECK_FALSE(presentation.toneMap);
}

TEST_CASE("RTX Video HDR needs an HDR display and the driver extension")
{
    const HdrPresentation noDisplay = hdrPresentationFor(
        core::HdrMode::RtxVideoHdr, sdrDisplay(), TransferFunction::Sdr, true);
    const HdrPresentation noExtension = hdrPresentationFor(
        core::HdrMode::RtxVideoHdr, hdrDisplay(true), TransferFunction::Sdr, false);

    CHECK_FALSE(noDisplay.hdrSwapChain);
    CHECK_FALSE(noDisplay.rtxVideoHdr);
    CHECK_FALSE(noExtension.hdrSwapChain);
    CHECK_FALSE(noExtension.rtxVideoHdr);
}

TEST_CASE("PQ decodes from black to the 10000-nit peak")
{
    CHECK(pqDecodeNits(0.0) == Catch::Approx(0.0).margin(0.001));
    CHECK(pqDecodeNits(1.0) == Catch::Approx(10000.0).epsilon(0.001));
    CHECK(pqDecodeNits(pqEncodeNits(100.0)) == Catch::Approx(100.0).epsilon(0.01));
    CHECK(pqDecodeNits(pqEncodeNits(1000.0)) == Catch::Approx(1000.0).epsilon(0.01));
}

TEST_CASE("The PQ curve rises with the code")
{
    double previous = -1.0;

    for (int step = 0; step <= 20; ++step)
    {
        const double nits = pqDecodeNits(static_cast<double>(step) / 20.0);
        CHECK(nits > previous);
        previous = nits;
    }
}

TEST_CASE("The tone map sends the content peak to white and black to black")
{
    CHECK(toneMapLinear(0.0, 1000.0, 200.0) == Catch::Approx(0.0));
    CHECK(toneMapLinear(1000.0, 1000.0, 200.0) == Catch::Approx(1.0).epsilon(0.001));
    CHECK(toneMapLinear(4000.0, 1000.0, 200.0) == Catch::Approx(1.0));
    CHECK(toneMapLinear(200.0, 1000.0, 200.0) < 1.0);
    CHECK(toneMapLinear(200.0, 1000.0, 200.0) > 0.0);
}

TEST_CASE("The tone map is monotonic")
{
    double previous = -1.0;

    for (int step = 0; step <= 40; ++step)
    {
        const double mapped = toneMapLinear(static_cast<double>(step) * 100.0, 1000.0, 200.0);
        CHECK(mapped > previous);
        previous = mapped;
    }
}

TEST_CASE("The overlay's white lands at SDR white in PQ and its black stays black")
{
    const double white = srgbToPq(1.0, 200.0);

    CHECK(white > 0.0);
    CHECK(white < 1.0);
    CHECK(pqDecodeNits(white) == Catch::Approx(200.0).epsilon(0.02));
    CHECK(srgbToPq(0.0, 200.0) == Catch::Approx(0.0).margin(0.0001));
    CHECK(srgbToPq(0.5, 200.0) > srgbToPq(0.25, 200.0));
}
