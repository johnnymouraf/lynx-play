#pragma once

#include "core/KeyboardBindings.h"
#include "core/Settings.h"
#include "input/ControllerMapping.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>

namespace mfplay::input
{

// The keyboard and mouse state the stream window records, turned into a
// controller sample. The window's thread writes it; the session's input
// thread reads it. Mouse movement becomes the right stick.
class KeyboardMouseInput
{
public:
    explicit KeyboardMouseInput(const core::KeyboardMouseSettings& settings);

    // Safe from any thread; the next sample uses the new mapping.
    void setSettings(const core::KeyboardMouseSettings& settings);
    void setOverlayOpen(bool open);

    // Keys and mouse buttons are Windows virtual-key codes and 0 (left),
    // 1 (right) or 2 (middle).
    void keyDown(std::uint16_t virtualKey);
    void keyUp(std::uint16_t virtualKey);
    void mouseButton(std::uint16_t button, bool down);
    void mouseMoved(int deltaX, int deltaY);
    void releaseAll();

    // What the console should see now; consumes the mouse movement since the
    // previous call.
    [[nodiscard]] GamepadSample sample();

private:
    struct Binding
    {
        core::KeyboardBindingKind kind = core::KeyboardBindingKind::Key;
        std::uint16_t code = 0;
    };

    void rebuildBindings();
    [[nodiscard]] bool pressed(const Binding& binding) const;

    mutable std::mutex mutex_;
    core::KeyboardMouseSettings settings_;
    std::array<Binding, core::kKeyboardControlCount> bindings_{};
    std::array<bool, 256> keys_{};
    std::array<bool, 3> mouseButtons_{};
    double deltaX_ = 0.0;
    double deltaY_ = 0.0;
    std::atomic<bool> overlayOpen_{false};
};

} // namespace mfplay::input
