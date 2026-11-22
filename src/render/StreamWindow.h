#pragma once

#include "core/ReflexMode.h"
#include "core/Settings.h"
#include "render/FrameMailbox.h"
#include "render/FrameRenderer.h"
#include "render/GpuDevice.h"
#include "render/Overlay.h"
#include "render/SwapChain.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

namespace lynx::input
{
class KeyboardMouseInput;
}

namespace lynx::render
{

enum class WindowMode
{
    Windowed,
    // Borderless, covering the whole monitor.
    Fullscreen,
};

enum class CloseReason
{
    // Esc or the window's close button.
    User,
    RenderFailed,
};

struct StreamWindowOptions
{
    std::wstring title;
    PresentTiming timing = PresentTiming::VSync;
    core::ReflexMode reflex = core::ReflexMode::Off;
    bool videoSuperResolution = false;
    bool opticalFlow = true;
    core::FrameGenerationMode frameGeneration = core::FrameGenerationMode::Off;
    core::HdrMode hdr = core::HdrMode::Auto;
    bool rtxVideoHdrAvailable = false;
    WindowMode mode = WindowMode::Windowed;
    // The window opens on the monitor this other window is on (the app's main
    // window); null opens it on the primary monitor.
    HWND monitorOf = nullptr;
    // The overlay's fonts and its initial state and actions.
    OverlayFonts overlayFonts;
    OverlayOptions overlay;
    OverlayActions overlayActions;
    // The counters the overlay reads; shared with the decoder.
    std::shared_ptr<core::StreamMetrics> metrics;
    // The keyboard and mouse state the window records for the session.
    std::shared_ptr<input::KeyboardMouseInput> keyboardMouse;
    // The controller the overlay's menu navigates with; may be empty.
    std::function<OverlayGamepad()> gamepad;
    // Called once when the window closes by itself, never when its owner
    // destroys it. It runs inside the window's message handling, so it must
    // not destroy the StreamWindow; defer that to the event loop.
    std::function<void(CloseReason)> onClosed;
};

// The top-level window the stream plays in, outside Qt so presentation keeps
// the display to itself. It shows the frames published into its mailbox. F11
// or Alt+Enter toggles borderless fullscreen and Esc closes it. Lives on the
// thread that runs the message loop (Qt's).
class StreamWindow
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<StreamWindow>, std::string> open(
        const GpuDevice& gpu, std::shared_ptr<FrameMailbox> mailbox, StreamWindowOptions options);

    StreamWindow(const StreamWindow&) = delete;
    StreamWindow& operator=(const StreamWindow&) = delete;
    StreamWindow(StreamWindow&&) = delete;
    StreamWindow& operator=(StreamWindow&&) = delete;
    ~StreamWindow();

    [[nodiscard]] bool isOpen() const;
    void bringToFront();
    void setReflexMode(core::ReflexMode mode);
    void setVideoSuperResolution(bool enabled);
    void setOpticalFlow(bool enabled);
    void setFrameGeneration(core::FrameGenerationMode mode);
    void setHdrMode(core::HdrMode mode);
    // Ends the stream as the player would with Esc.
    void close();

private:
    explicit StreamWindow(std::function<void(CloseReason)> onClosed);

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void handleKey(WPARAM key);
    void handleRawInput(LPARAM lParam);
    void toggleFullscreen();
    void enterFullscreen();
    void leaveFullscreen();
    void setCursorVisible(bool visible);
    void close(CloseReason reason);

    HWND window_ = nullptr;
    std::unique_ptr<FrameRenderer> renderer_;
    std::function<void(CloseReason)> onClosed_;
    std::shared_ptr<input::KeyboardMouseInput> keyboardMouse_;
    bool fullscreen_ = false;
    bool cursorVisible_ = true;
    bool haveAbsoluteMouse_ = false;
    LONG lastAbsoluteX_ = 0;
    LONG lastAbsoluteY_ = 0;
    WINDOWPLACEMENT windowedPlacement_{.length = sizeof(WINDOWPLACEMENT)};
};

} // namespace lynx::render
