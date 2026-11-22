#pragma once

#include "core/ReflexMode.h"
#include "render/FrameMailbox.h"
#include "render/FrameRenderer.h"
#include "render/GpuDevice.h"
#include "render/SwapChain.h"

#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <windows.h>

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
    WindowMode mode = WindowMode::Windowed;
    // The window opens on the monitor this other window is on (the app's main
    // window); null opens it on the primary monitor.
    HWND monitorOf = nullptr;
    // The font the statistics overlay draws with; empty uses Dear ImGui's.
    std::shared_ptr<const std::vector<std::uint8_t>> overlayFont;
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

private:
    explicit StreamWindow(std::function<void(CloseReason)> onClosed);

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void handleKey(WPARAM key);
    void toggleFullscreen();
    void enterFullscreen();
    void leaveFullscreen();
    void close(CloseReason reason);

    HWND window_ = nullptr;
    std::unique_ptr<FrameRenderer> renderer_;
    std::function<void(CloseReason)> onClosed_;
    bool fullscreen_ = false;
    WINDOWPLACEMENT windowedPlacement_{.length = sizeof(WINDOWPLACEMENT)};
};

} // namespace lynx::render
