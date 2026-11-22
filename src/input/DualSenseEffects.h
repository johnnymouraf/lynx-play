#pragma once

#include <chiaki/session.h>

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace lynx::input
{

// One adaptive-trigger effect, as the console sends it: a mode byte and ten
// parameters the console owns.
struct DualSenseTriggerEffect
{
    std::uint8_t type = 0;
    std::array<std::uint8_t, 10> parameters{};
};

// What to send to the controller after a console feedback event. SDL's own
// calls carry rumble, lightbar and player LEDs; triggers, the microphone LED
// and the intensity nibble travel in the 47-byte effect packet.
struct DualSenseCommand
{
    // SDL's 16-bit motor levels.
    std::optional<std::pair<std::uint16_t, std::uint16_t>> rumble;
    std::optional<DualSenseTriggerEffect> leftTrigger;
    std::optional<DualSenseTriggerEffect> rightTrigger;
    std::optional<std::array<std::uint8_t, 3>> led;
    std::optional<int> playerIndex;
    std::optional<bool> microphoneMuted;
    // chiaki-ng's convention for rgucUnknown1[4]: trigger intensity in the
    // high nibble and haptic intensity in the low one.
    std::optional<std::uint8_t> intensity;
    // Whether effect packets select classic rumble (bit 0x02), which also
    // disables the controller's audio haptics for that report.
    std::optional<bool> classicRumble;
};

// SDL's rumble range for a 0..255 level.
[[nodiscard]] constexpr std::uint16_t rumbleAmplitude(std::uint8_t level)
{
    return static_cast<std::uint16_t>(static_cast<std::uint16_t>(level) * 257u);
}

[[nodiscard]] constexpr std::uint8_t intensityNibble(ChiakiDualSenseEffectIntensity value,
                                                     bool trigger)
{
    switch (value)
    {
    case ::Off:
        return trigger ? 0xF0 : 0x0F;
    case ::Weak:
        return trigger ? 0x90 : 0x03;
    case ::Medium:
        return trigger ? 0x60 : 0x02;
    case ::Strong:
        return 0x00;
    }

    return 0x00;
}

[[nodiscard]] constexpr std::uint8_t intensityByte(ChiakiDualSenseEffectIntensity haptic,
                                                   ChiakiDualSenseEffectIntensity trigger)
{
    return static_cast<std::uint8_t>((intensityNibble(trigger, true) & 0xF0) |
                                     (intensityNibble(haptic, false) & 0x0F));
}

// How much of the console's haptic signal reaches the controller's actuators.
[[nodiscard]] constexpr float intensityScale(ChiakiDualSenseEffectIntensity value)
{
    switch (value)
    {
    case ::Off:
        return 0.0f;
    case ::Weak:
        return 0.4f;
    case ::Medium:
        return 0.7f;
    case ::Strong:
        return 1.0f;
    }

    return 1.0f;
}

[[nodiscard]] DualSenseCommand dualSenseCommand(const ChiakiRumbleEvent& event);
[[nodiscard]] DualSenseCommand dualSenseCommand(const ChiakiTriggerEffectsEvent& event);
[[nodiscard]] DualSenseCommand ledCommand(const std::uint8_t (&color)[3]);
[[nodiscard]] DualSenseCommand playerIndexCommand(std::uint8_t index);
[[nodiscard]] DualSenseCommand microphoneCommand(bool muted);

// The DualSense's 47-byte effect report. Applying a command merges it into
// the controller's current state, so every report carries every effect the
// console asked for, as chiaki-ng does.
class DualSenseEffects
{
public:
    static constexpr std::size_t kPacketSize = 47;
    static constexpr std::size_t kTriggerEffectOffset = 10;
    static constexpr std::size_t kLeftTriggerEffectOffset = 21;
    static constexpr std::size_t kIntensityOffset = 36;
    static constexpr std::size_t kLedOffset = 44;

    void apply(const DualSenseCommand& command);

    [[nodiscard]] std::array<std::uint8_t, kPacketSize> serialize() const;

private:
    std::array<std::uint8_t, kPacketSize> bytes_{};
};

} // namespace lynx::input
