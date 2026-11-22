#pragma once

#include "render/FrameRenderer.h"

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include <windows.h>

namespace lynx::render
{

// The top-level window the stream plays in, outside Qt so presentation keeps
// the display to itself. F11 or Alt+Enter toggles borderless fullscreen and
// Esc closes it. Lives on the thread that runs the message loop (Qt's).
class StreamWindow
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<StreamWindow>, std::string> open(
        std::wstring_view title);

    StreamWindow(const StreamWindow&) = delete;
    StreamWindow& operator=(const StreamWindow&) = delete;
    StreamWindow(StreamWindow&&) = delete;
    StreamWindow& operator=(StreamWindow&&) = delete;
    ~StreamWindow();

    [[nodiscard]] bool isOpen() const;
    void bringToFront();

private:
    StreamWindow() = default;

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void handleKey(WPARAM key);
    void toggleFullscreen();
    void enterFullscreen();
    void leaveFullscreen();
    void close();

    HWND window_ = nullptr;
    std::unique_ptr<FrameRenderer> renderer_;
    bool fullscreen_ = false;
    WINDOWPLACEMENT windowedPlacement_{.length = sizeof(WINDOWPLACEMENT)};
};

} // namespace lynx::render
