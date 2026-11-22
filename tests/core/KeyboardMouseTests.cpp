#include "core/KeyboardBindings.h"
#include "core/Settings.h"

#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

using namespace lynx::core;

namespace
{

std::expected<Settings, std::string> read(const std::string& text)
{
    std::istringstream input(text);
    return readSettings(input);
}

} // namespace

TEST_CASE("The default keyboard and mouse bindings follow the design")
{
    const KeyboardMouseSettings defaults;

    CHECK(defaults.bindings.at(0) == "W");
    CHECK(defaults.bindings.at(1) == "S");
    CHECK(defaults.bindings.at(2) == "A");
    CHECK(defaults.bindings.at(3) == "D");
    CHECK(defaults.bindings.at(4) == "Mouse");
    CHECK(defaults.bindings.at(5) == "Space");
    CHECK(defaults.bindings.at(6) == "Q");
    CHECK(defaults.bindings.at(7) == "R");
    CHECK(defaults.bindings.at(8) == "F");
    CHECK(defaults.bindings.at(9) == "Shift");
    CHECK(defaults.bindings.at(10) == "E");
    CHECK(defaults.bindings.at(11) == "MouseRight");
    CHECK(defaults.bindings.at(12) == "MouseLeft");
    CHECK(defaults.bindings.at(13) == "Enter");
    CHECK(defaults.bindings.at(14) == "Tab");
    CHECK(defaults.mouseSensitivity == 1.0);
    CHECK_FALSE(defaults.invertY);
}

TEST_CASE("Keyboard bindings survive their settings names")
{
    for (const std::string_view name : {"A", "Space", "Shift", "Mouse", "MouseRight", "F12"})
    {
        const auto binding = keyboardBindingFromName(name);

        REQUIRE(binding.has_value());
        CHECK(keyboardBindingName(*binding) == std::string(name));
    }

    CHECK_FALSE(keyboardBindingFromName("Nope").has_value());
}

TEST_CASE("Remapping a key keeps every control bound")
{
    KeyboardMouseSettings settings;
    const auto crossIndex = static_cast<std::size_t>(KeyboardControl::Cross);
    const auto squareIndex = static_cast<std::size_t>(KeyboardControl::Square);

    const KeyboardMouseSettings remapped =
        assignKeyboardBinding(settings, KeyboardControl::Cross, "R");

    CHECK(remapped.bindings.at(crossIndex) == "R");
    CHECK(remapped.bindings.at(squareIndex) == "Space");
}

TEST_CASE("Remapping to a free key leaves the other controls alone")
{
    const KeyboardMouseSettings settings;
    const KeyboardMouseSettings remapped =
        assignKeyboardBinding(settings, KeyboardControl::L1, "G");

    CHECK(remapped.bindings.at(static_cast<std::size_t>(KeyboardControl::L1)) == "G");
    CHECK(remapped.bindings.at(static_cast<std::size_t>(KeyboardControl::Cross)) == "Space");
}

TEST_CASE("An invalid remap keeps the mapping")
{
    const KeyboardMouseSettings settings = assignKeyboardBinding(
        KeyboardMouseSettings{}, KeyboardControl::Cross, "Nope");

    CHECK(settings.bindings.at(static_cast<std::size_t>(KeyboardControl::Cross)) == "Space");
    CHECK(settings.bindings.at(static_cast<std::size_t>(KeyboardControl::Circle)) == "Q");
}

TEST_CASE("Keyboard and mouse settings survive a write and a read")
{
    Settings settings;
    settings.keyboardMouse.mouseSensitivity = 2.5;
    settings.keyboardMouse.invertY = true;
    settings.keyboardMouse.bindings.at(static_cast<std::size_t>(KeyboardControl::Cross)) = "G";
    std::ostringstream output;

    writeSettings(settings, output);
    const auto readBack = read(output.str());

    REQUIRE(readBack.has_value());
    CHECK(readBack->keyboardMouse == settings.keyboardMouse);
}

TEST_CASE("An invalid keyboard binding falls back to its default")
{
    const auto settings = read(R"({
        "version": 1,
        "keyboardMouse": {
            "bindings": {
                "cross": "Nope",
                "circle": "G"
            },
            "mouseSensitivity": 99,
            "invertY": "yes"
        }
    })");

    REQUIRE(settings.has_value());
    CHECK(settings->keyboardMouse.bindings.at(static_cast<std::size_t>(KeyboardControl::Cross)) ==
          "Space");
    CHECK(settings->keyboardMouse.bindings.at(static_cast<std::size_t>(KeyboardControl::Circle)) ==
          "G");
    CHECK(settings->keyboardMouse.mouseSensitivity == 1.0);
    CHECK_FALSE(settings->keyboardMouse.invertY);
}
