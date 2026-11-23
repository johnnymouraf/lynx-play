#include "input/DualSenseEffects.h"

#include <catch2/catch_test_macros.hpp>

using namespace lynx::input;

namespace
{

constexpr std::size_t kEnableBits1 = 0;
constexpr std::size_t kEnableBits2 = 1;
constexpr std::size_t kMicrophoneLight = 8;
constexpr std::size_t kAudioMute = 9;
constexpr std::size_t kRightTrigger = DualSenseEffects::kTriggerEffectOffset;
constexpr std::size_t kLeftTrigger = DualSenseEffects::kLeftTriggerEffectOffset;
constexpr std::size_t kIntensity = DualSenseEffects::kIntensityOffset;
constexpr std::size_t kEnableBits3 = DualSenseEffects::kEnableBits3Offset;
constexpr std::size_t kRumbleRight = DualSenseEffects::kRumbleRightOffset;
constexpr std::size_t kRumbleLeft = DualSenseEffects::kRumbleLeftOffset;

} // namespace

TEST_CASE("A fresh DualSense effect packet is all zero")
{
    const auto packet = DualSenseEffects{}.serialize();

    for (const std::uint8_t byte : packet)
    {
        CHECK(byte == 0);
    }
}

TEST_CASE("Rumble events map to SDL's 16-bit range and back")
{
    const ChiakiRumbleEvent event{.unknown = 0, .left = 200, .right = 32};
    const DualSenseCommand command = dualSenseCommand(event);

    REQUIRE(command.rumble.has_value());
    CHECK(command.rumble->first == 51400);
    CHECK(command.rumble->second == 8224);
    CHECK(rumbleAmplitude(0) == 0);
    CHECK(rumbleAmplitude(200) == 51400);
    CHECK(rumbleAmplitude(255) == 65535);

    CHECK(rumbleLevel(0) == 0);
    CHECK(rumbleLevel(257) == 1);
    CHECK(rumbleLevel(51400) == 200);
    CHECK(rumbleLevel(65535) == 255);
}

TEST_CASE("The DualSense rumble encoding follows the firmware, as chiaki-ng's")
{
    CHECK(rumbleEncoding(false, 0) == DualSenseRumbleEncoding::Current);
    CHECK(rumbleEncoding(false, 0x0223) == DualSenseRumbleEncoding::Legacy);
    CHECK(rumbleEncoding(false, 0x0224) == DualSenseRumbleEncoding::Current);
    CHECK(rumbleEncoding(false, 0x0300) == DualSenseRumbleEncoding::Current);
    CHECK(rumbleEncoding(true, 0x0100) == DualSenseRumbleEncoding::Current);
}

TEST_CASE("A rumble packet carries the levels in chiaki-ng's bits")
{
    const auto current =
        dualSenseRumblePacket({.left = 200, .right = 32}, DualSenseRumbleEncoding::Current, 0x92);

    CHECK(current[kEnableBits1] == 0x02);
    CHECK(current[kEnableBits2] == 0x40);
    CHECK(current[kEnableBits3] == 0x04);
    CHECK(current[kRumbleLeft] == 200);
    CHECK(current[kRumbleRight] == 32);
    CHECK(current[kIntensity] == 0x92);
    CHECK(current.size() == DualSenseEffects::kPacketSize);

    const auto legacy =
        dualSenseRumblePacket({.left = 200, .right = 32}, DualSenseRumbleEncoding::Legacy, 0);

    CHECK(legacy[kEnableBits1] == 0x03);
    CHECK(legacy[kEnableBits3] == 0x00);
    CHECK(legacy[kRumbleLeft] == 100);
    CHECK(legacy[kRumbleRight] == 16);
    CHECK(legacy[kIntensity] == 0x00);
}

TEST_CASE("Rumble uses the effect packet on a DualSense and SDL only without audio haptics")
{
    CHECK(rumblePath(true, false) == RumblePath::DualSenseEffect);
    CHECK(rumblePath(true, true) == RumblePath::DualSenseEffect);
    CHECK(rumblePath(false, false) == RumblePath::SdlRumble);
    CHECK(rumblePath(false, true) == RumblePath::None);
}

TEST_CASE("Trigger effects keep the console's mode and ten parameters")
{
    ChiakiTriggerEffectsEvent event{};
    event.type_left = 0x21;
    event.type_right = 0x05;

    for (std::size_t i = 0; i < 10; ++i)
    {
        event.left[i] = static_cast<std::uint8_t>(i);
        event.right[i] = static_cast<std::uint8_t>(10 + i);
    }

    DualSenseEffects effects;
    effects.apply(dualSenseCommand(event));
    const auto packet = effects.serialize();

    CHECK(packet[kEnableBits1] == 0x0C);
    CHECK(packet[kEnableBits2] == 0x40);
    CHECK(packet[kRightTrigger] == 0x05);
    CHECK(packet[kLeftTrigger] == 0x21);

    for (std::size_t i = 0; i < 10; ++i)
    {
        CHECK(packet[kRightTrigger + 1 + i] == 10 + i);
        CHECK(packet[kLeftTrigger + 1 + i] == i);
    }
}

TEST_CASE("The microphone LED and mute routing follow the mute state")
{
    DualSenseEffects effects;
    effects.apply(microphoneCommand(true));
    auto packet = effects.serialize();

    CHECK((packet[kEnableBits2] & 0x03) == 0x03);
    CHECK(packet[kMicrophoneLight] == 0x01);
    CHECK(packet[kAudioMute] == 0x08);

    effects.apply(microphoneCommand(false));
    packet = effects.serialize();

    CHECK(packet[kMicrophoneLight] == 0x00);
    CHECK(packet[kAudioMute] == 0x00);
}

TEST_CASE("The lightbar and player LED commands carry the console's values")
{
    const std::uint8_t color[3] = {0x12, 0x34, 0x56};
    const DualSenseCommand led = ledCommand(color);

    REQUIRE(led.led.has_value());
    CHECK((*led.led)[0] == 0x12);
    CHECK((*led.led)[1] == 0x34);
    CHECK((*led.led)[2] == 0x56);

    const DualSenseCommand player = playerIndexCommand(3);
    REQUIRE(player.playerIndex.has_value());
    CHECK(*player.playerIndex == 3);
}

TEST_CASE("Intensity nibbles follow chiaki-ng's convention")
{
    CHECK(intensityByte(::Strong, ::Strong) == 0x00);
    CHECK(intensityByte(::Medium, ::Weak) == 0x92);
    CHECK(intensityByte(::Off, ::Strong) == 0x0F);
    CHECK(intensityByte(::Strong, ::Off) == 0xF0);
    CHECK(intensityScale(::Off) == 0.0f);
    CHECK(intensityScale(::Strong) == 1.0f);
}

TEST_CASE("Effect packets keep or clear the classic-rumble bit")
{
    DualSenseEffects effects;
    effects.apply({.classicRumble = true});
    CHECK((effects.serialize()[kEnableBits1] & 0x02) == 0x02);

    effects.apply({.classicRumble = false});
    CHECK((effects.serialize()[kEnableBits1] & 0x02) == 0x00);
}

TEST_CASE("The intensity byte reaches rgucUnknown1[4]")
{
    DualSenseEffects effects;
    effects.apply({.intensity = 0x92});
    const auto packet = effects.serialize();

    CHECK(packet[kIntensity] == 0x92);
    CHECK(packet[kEnableBits2] == 0x40);
}
