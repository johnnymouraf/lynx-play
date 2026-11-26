#include "core/KeyboardBindings.h"

#include <algorithm>
#include <array>
#include <format>

namespace mfplay::core
{

namespace
{

// Windows virtual-key codes, the values the binding names stand for.
constexpr std::uint16_t kVkBack = 0x08;
constexpr std::uint16_t kVkTab = 0x09;
constexpr std::uint16_t kVkReturn = 0x0D;
constexpr std::uint16_t kVkShift = 0x10;
constexpr std::uint16_t kVkControl = 0x11;
constexpr std::uint16_t kVkMenu = 0x12;
constexpr std::uint16_t kVkEscape = 0x1B;
constexpr std::uint16_t kVkSpace = 0x20;
constexpr std::uint16_t kVkPrior = 0x21;
constexpr std::uint16_t kVkNext = 0x22;
constexpr std::uint16_t kVkEnd = 0x23;
constexpr std::uint16_t kVkHome = 0x24;
constexpr std::uint16_t kVkLeft = 0x25;
constexpr std::uint16_t kVkUp = 0x26;
constexpr std::uint16_t kVkRight = 0x27;
constexpr std::uint16_t kVkDown = 0x28;
constexpr std::uint16_t kVkInsert = 0x2D;
constexpr std::uint16_t kVkDelete = 0x2E;
constexpr std::uint16_t kVkF1 = 0x70;
constexpr std::uint16_t kVkF2 = 0x71;
constexpr std::uint16_t kVkF3 = 0x72;
constexpr std::uint16_t kVkF4 = 0x73;
constexpr std::uint16_t kVkF5 = 0x74;
constexpr std::uint16_t kVkF6 = 0x75;
constexpr std::uint16_t kVkF7 = 0x76;
constexpr std::uint16_t kVkF8 = 0x77;
constexpr std::uint16_t kVkF9 = 0x78;
constexpr std::uint16_t kVkF10 = 0x79;
constexpr std::uint16_t kVkF11 = 0x7A;
constexpr std::uint16_t kVkF12 = 0x7B;
constexpr std::uint16_t kVkOem1 = 0xBA;
constexpr std::uint16_t kVkOemPlus = 0xBB;
constexpr std::uint16_t kVkOemComma = 0xBC;
constexpr std::uint16_t kVkOemMinus = 0xBD;
constexpr std::uint16_t kVkOemPeriod = 0xBE;
constexpr std::uint16_t kVkOem2 = 0xBF;
constexpr std::uint16_t kVkOem3 = 0xC0;
constexpr std::uint16_t kVkOem4 = 0xDB;
constexpr std::uint16_t kVkOem5 = 0xDC;
constexpr std::uint16_t kVkOem6 = 0xDD;
constexpr std::uint16_t kVkOem7 = 0xDE;

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
    {"Space", key(kVkSpace)},
    {"Enter", key(kVkReturn)},
    {"Tab", key(kVkTab)},
    {"Backspace", key(kVkBack)},
    {"Escape", key(kVkEscape)},
    {"Shift", key(kVkShift)},
    {"Ctrl", key(kVkControl)},
    {"Alt", key(kVkMenu)},
    {"Up", key(kVkUp)},
    {"Down", key(kVkDown)},
    {"Left", key(kVkLeft)},
    {"Right", key(kVkRight)},
    {"Home", key(kVkHome)},
    {"End", key(kVkEnd)},
    {"PageUp", key(kVkPrior)},
    {"PageDown", key(kVkNext)},
    {"Insert", key(kVkInsert)},
    {"Delete", key(kVkDelete)},
    {"Semicolon", key(kVkOem1)},
    {"Plus", key(kVkOemPlus)},
    {"Comma", key(kVkOemComma)},
    {"Minus", key(kVkOemMinus)},
    {"Period", key(kVkOemPeriod)},
    {"Slash", key(kVkOem2)},
    {"Backtick", key(kVkOem3)},
    {"LeftBracket", key(kVkOem4)},
    {"Backslash", key(kVkOem5)},
    {"RightBracket", key(kVkOem6)},
    {"Quote", key(kVkOem7)},
    {"F1", key(kVkF1)},
    {"F2", key(kVkF2)},
    {"F3", key(kVkF3)},
    {"F4", key(kVkF4)},
    {"F5", key(kVkF5)},
    {"F6", key(kVkF6)},
    {"F7", key(kVkF7)},
    {"F8", key(kVkF8)},
    {"F9", key(kVkF9)},
    {"F10", key(kVkF10)},
    {"F11", key(kVkF11)},
    {"F12", key(kVkF12)},
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
