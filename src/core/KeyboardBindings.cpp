#include "core/KeyboardBindings.h"

#include <algorithm>
#include <array>
#include <format>

namespace mfplay::core
{

namespace
{

using NamedBinding = std::pair<std::string_view, KeyboardBinding>;

constexpr KeyboardBinding key(std::uint16_t code)
{
    return {.kind = KeyboardBindingKind::Key, .code = code};
}

constexpr KeyboardBinding mouseButton(std::uint16_t code)
{
    return {.kind = KeyboardBindingKind::MouseButton, .code = code};
}

constexpr KeyboardBinding mouseMotion()
{
    return {.kind = KeyboardBindingKind::MouseMotion, .code = 0};
}

constexpr std::array<NamedBinding, 81> kBindings{{
    {"A", key('A')},
    {"B", key('B')},
    {"C", key('C')},
    {"D", key('D')},
    {"E", key('E')},
    {"F", key('F')},
    {"G", key('G')},
    {"H", key('H')},
    {"I", key('I')},
    {"J", key('J')},
    {"K", key('K')},
    {"L", key('L')},
    {"M", key('M')},
    {"N", key('N')},
    {"O", key('O')},
    {"P", key('P')},
    {"Q", key('Q')},
    {"R", key('R')},
    {"S", key('S')},
    {"T", key('T')},
    {"U", key('U')},
    {"V", key('V')},
    {"W", key('W')},
    {"X", key('X')},
    {"Y", key('Y')},
    {"Z", key('Z')},
    {"0", key('0')},
    {"1", key('1')},
    {"2", key('2')},
    {"3", key('3')},
    {"4", key('4')},
    {"5", key('5')},
    {"6", key('6')},
    {"7", key('7')},
    {"8", key('8')},
    {"9", key('9')},
    {"Space", key(VK_SPACE)},
    {"Enter", key(VK_RETURN)},
    {"Tab", key(VK_TAB)},
    {"Backspace", key(VK_BACK)},
    {"Escape", key(VK_ESCAPE)},
    {"Shift", key(VK_SHIFT)},
    {"Ctrl", key(VK_CONTROL)},
    {"Alt", key(VK_MENU)},
    {"Up", key(VK_UP)},
    {"Down", key(VK_DOWN)},
    {"Left", key(VK_LEFT)},
    {"Right", key(VK_RIGHT)},
    {"Home", key(VK_HOME)},
    {"End", key(VK_END)},
    {"PageUp", key(VK_PRIOR)},
    {"PageDown", key(VK_NEXT)},
    {"Insert", key(VK_INSERT)},
    {"Delete", key(VK_DELETE)},
    {"Semicolon", key(VK_OEM_1)},
    {"Plus", key(VK_OEM_PLUS)},
    {"Comma", key(VK_OEM_COMMA)},
    {"Minus", key(VK_OEM_MINUS)},
    {"Period", key(VK_OEM_PERIOD)},
    {"Slash", key(VK_OEM_2)},
    {"Backtick", key(VK_OEM_3)},
    {"LeftBracket", key(VK_OEM_4)},
    {"Backslash", key(VK_OEM_5)},
    {"RightBracket", key(VK_OEM_6)},
    {"Quote", key(VK_OEM_7)},
    {"F1", key(VK_F1)},
    {"F2", key(VK_F2)},
    {"F3", key(VK_F3)},
    {"F4", key(VK_F4)},
    {"F5", key(VK_F5)},
    {"F6", key(VK_F6)},
    {"F7", key(VK_F7)},
    {"F8", key(VK_F8)},
    {"F9", key(VK_F9)},
    {"F10", key(VK_F10)},
    {"F11", key(VK_F11)},
    {"F12", key(VK_F12)},
    {"Mouse", mouseMotion()},
    {"MouseLeft", mouseButton(kMouseLeft)},
    {"MouseRight", mouseButton(kMouseRight)},
    {"MouseMiddle", mouseButton(kMouseMiddle)},
}};

} // namespace

std::optional<KeyboardBinding> keyboardBindingFromName(std::string_view name)
{
    const auto found = std::ranges::find(kBindings, name, &NamedBinding::first);

    if (found != kBindings.end())
    {
        return found->second;
    }

    constexpr std::string_view kPrefix = "VK_";

    if (name.starts_with(kPrefix) && name.size() == kPrefix.size() + 2)
    {
        std::uint16_t code = 0;

        for (const char digit : name.substr(kPrefix.size()))
        {
            code <<= 4;

            if (digit >= '0' && digit <= '9')
            {
                code = static_cast<std::uint16_t>(code + (digit - '0'));
            }
            else if (digit >= 'A' && digit <= 'F')
            {
                code = static_cast<std::uint16_t>(code + (digit - 'A' + 10));
            }
            else
            {
                return std::nullopt;
            }
        }

        return key(code);
    }

    return std::nullopt;
}

std::string keyboardBindingName(KeyboardBinding binding)
{
    const auto found = std::ranges::find(kBindings, binding, &NamedBinding::second);

    if (found != kBindings.end())
    {
        return std::string(found->first);
    }

    if (binding.kind == KeyboardBindingKind::Key)
    {
        return std::format("VK_{:02X}", binding.code);
    }

    return "VK_00";
}

} // namespace mfplay::core
