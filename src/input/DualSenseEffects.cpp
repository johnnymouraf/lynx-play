#include "input/DualSenseEffects.h"

#include <algorithm>
#include <iterator>

namespace lynx::input
{

namespace
{

constexpr std::size_t kEnableBits1 = 0;
constexpr std::size_t kEnableBits2 = 1;
constexpr std::size_t kMicrophoneLight = 8;
constexpr std::size_t kAudioMute = 9;

// chiaki-ng sets this on every effect report it sends; its meaning is not
// documented by Sony.
constexpr std::uint8_t kEffectsEnabled = 0x40;

constexpr std::uint8_t kClassicRumble = 0x02;
constexpr std::uint8_t kLeftTrigger = 0x04;
constexpr std::uint8_t kRightTrigger = 0x08;

constexpr std::uint8_t kLegacyRumble = 0x01;
constexpr std::uint8_t kCurrentRumble = 0x04;

constexpr std::uint8_t kMicrophoneLightEnabled = 0x01;
constexpr std::uint8_t kMicrophoneEnabled = 0x02;

void applyTrigger(std::array<std::uint8_t, DualSenseEffects::kPacketSize>& bytes,
                  std::size_t offset, const DualSenseTriggerEffect& effect)
{
    bytes[offset] = effect.type;

    for (std::size_t i = 0; i < effect.parameters.size(); ++i)
    {
        bytes[offset + 1 + i] = effect.parameters[i];
    }
}

} // namespace

DualSenseCommand dualSenseCommand(const ChiakiRumbleEvent& event)
{
    return {.rumble = std::pair{rumbleAmplitude(event.left), rumbleAmplitude(event.right)}};
}

std::array<std::uint8_t, kDualSensePacketSize> dualSenseRumblePacket(
    DualSenseRumble rumble, DualSenseRumbleEncoding encoding, std::uint8_t intensity)
{
    std::array<std::uint8_t, kDualSensePacketSize> bytes{};
    bytes[DualSenseEffects::kEnableBits2] = kEffectsEnabled;
    bytes[DualSenseEffects::kEnableBits1] = kClassicRumble;
    bytes[DualSenseEffects::kIntensityOffset] = intensity;

    if (encoding == DualSenseRumbleEncoding::Legacy)
    {
        bytes[DualSenseEffects::kEnableBits1] |= kLegacyRumble;
        bytes[DualSenseEffects::kRumbleRightOffset] = static_cast<std::uint8_t>(rumble.right >> 1);
        bytes[DualSenseEffects::kRumbleLeftOffset] = static_cast<std::uint8_t>(rumble.left >> 1);
        return bytes;
    }

    bytes[DualSenseEffects::kEnableBits3Offset] = kCurrentRumble;
    bytes[DualSenseEffects::kRumbleRightOffset] = rumble.right;
    bytes[DualSenseEffects::kRumbleLeftOffset] = rumble.left;
    return bytes;
}

DualSenseCommand dualSenseCommand(const ChiakiTriggerEffectsEvent& event)
{
    DualSenseTriggerEffect left;
    left.type = event.type_left;
    std::copy(std::begin(event.left), std::end(event.left), left.parameters.begin());

    DualSenseTriggerEffect right;
    right.type = event.type_right;
    std::copy(std::begin(event.right), std::end(event.right), right.parameters.begin());

    return {.leftTrigger = left, .rightTrigger = right};
}

DualSenseCommand ledCommand(const std::uint8_t (&color)[3])
{
    return {.led = std::array<std::uint8_t, 3>{color[0], color[1], color[2]}};
}

DualSenseCommand playerIndexCommand(std::uint8_t index)
{
    return {.playerIndex = static_cast<int>(index)};
}

DualSenseCommand microphoneCommand(bool muted)
{
    return {.microphoneMuted = muted};
}

void DualSenseEffects::apply(const DualSenseCommand& command)
{
    if (command.leftTrigger)
    {
        bytes_[kEnableBits1] |= kLeftTrigger;
        applyTrigger(bytes_, kLeftTriggerEffectOffset, *command.leftTrigger);
    }

    if (command.rightTrigger)
    {
        bytes_[kEnableBits1] |= kRightTrigger;
        applyTrigger(bytes_, kTriggerEffectOffset, *command.rightTrigger);
    }

    if (command.microphoneMuted)
    {
        bytes_[kEnableBits2] |=
            static_cast<std::uint8_t>(kMicrophoneLightEnabled | kMicrophoneEnabled);
        bytes_[kMicrophoneLight] = *command.microphoneMuted ? 0x01 : 0x00;
        bytes_[kAudioMute] = *command.microphoneMuted ? 0x08 : 0x00;
    }

    if (command.intensity)
    {
        bytes_[kEnableBits2] |= kEffectsEnabled;
        bytes_[kIntensityOffset] = *command.intensity;
    }

    if (command.classicRumble)
    {
        if (*command.classicRumble)
        {
            bytes_[kEnableBits1] |= kClassicRumble;
        }
        else
        {
            bytes_[kEnableBits1] &= static_cast<std::uint8_t>(~kClassicRumble);
        }
    }

    if (command.leftTrigger || command.rightTrigger || command.intensity)
    {
        bytes_[kEnableBits2] |= kEffectsEnabled;
    }
}

std::array<std::uint8_t, DualSenseEffects::kPacketSize> DualSenseEffects::serialize() const
{
    return bytes_;
}

} // namespace lynx::input
