#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <windows.h>

namespace mfplay::core
{

enum class KeyboardBindingKind
{
    Key,
    MouseButton,
    MouseMotion,
};

inline constexpr std::uint16_t kMouseLeft = 0;
inline constexpr std::uint16_t kMouseRight = 1;
inline constexpr std::uint16_t kMouseMiddle = 2;

// One keyboard key or mouse control, in a form settings.json can hold.
struct KeyboardBinding
{
    KeyboardBindingKind kind = KeyboardBindingKind::Key;
    std::uint16_t code = 0;

    bool operator==(const KeyboardBinding&) const = default;
};

// "A", "Space", "LeftShift", "MouseLeft" or "Mouse"; the inverse writes
// the same names, with "VK_XX" for a key without one.
[[nodiscard]] std::optional<KeyboardBinding> keyboardBindingFromName(std::string_view name);
[[nodiscard]] std::string keyboardBindingName(KeyboardBinding binding);

} // namespace mfplay::core
