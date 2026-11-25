#include "render/StreamWindow.h"

#include "core/Windows.h"
#include "input/KeyboardMouse.h"

#include <algorithm>
#include <array>
#include <mutex>
#include <utility>

#include <dwmapi.h>
#include <spdlog/spdlog.h>

namespace mfplay::render
{

namespace
{

constexpr auto kWindowClassName = L"MFPlayStreamWindow";
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;
constexpr DWORD kFullscreenStyle = WS_POPUP;

// The caption follows the app's dark theme (Theme.menu, Theme.text and
// Theme.border), as COLORREFs.
constexpr COLORREF kCaptionColor = RGB(0x12, 0x12, 0x12);
constexpr COLORREF kCaptionTextColor = RGB(0xF5, 0xF5, 0xF5);
constexpr COLORREF kBorderColor = RGB(0x2E, 0x2E, 0x2E);

// Posted by the renderer's thread when it stops for good.
constexpr UINT kRenderFailedMessage = WM_APP + 1;
// Posted when the overlay's menu opens or closes, from the renderer's thread.
constexpr UINT kOverlayMenuMessage = WM_APP + 2;

// A border drag reports its size once it settles; a programmatic resize
// (SetWindowPos) has no drag-end message, so the timer catches both.
constexpr UINT_PTR kSizeSettleTimer = 1;
constexpr UINT kSizeSettleMilliseconds = 500;

// Fullscreen hides the idle cursor this long after the last movement.
constexpr UINT_PTR kCursorHideTimer = 2;
constexpr UINT kCursorHideMilliseconds = 2000;

// Bit 29 of a key message's lParam: the Alt key was down.
constexpr LPARAM kAltDownFlag = LPARAM{1} << 29;

void registerWindowClass(WNDPROC procedure)
{
    const WNDCLASSEXW windowClass{
        .cbSize = sizeof(WNDCLASSEXW),
        .lpfnWndProc = procedure,
        .hInstance = GetModuleHandleW(nullptr),
        .hCursor = LoadCursorW(nullptr, IDC_ARROW),
        .lpszClassName = kWindowClassName,
    };

    RegisterClassExW(&windowClass);
}

void registerWindowClassOnce(WNDPROC procedure)
{
    static std::once_flag registered;
    std::call_once(registered, registerWindowClass, procedure);
}

void registerMouse(HWND window)
{
    const RAWINPUTDEVICE mouse{
        .usUsagePage = 0x01,
        .usUsage = 0x02,
        .dwFlags = 0,
        .hwndTarget = window,
    };

    if (!RegisterRawInputDevices(&mouse, 1, sizeof(mouse)))
    {
        spdlog::warn("{}", core::describeFailure("RegisterRawInputDevices",
                                                 HRESULT_FROM_WIN32(GetLastError())));
    }
}

void rawButton(const RAWMOUSE& mouse, USHORT downFlag, USHORT upFlag, std::uint16_t button,
               input::KeyboardMouseInput& keyboardMouse)
{
    if ((mouse.usButtonFlags & downFlag) != 0)
    {
        keyboardMouse.mouseButton(button, true);
    }

    if ((mouse.usButtonFlags & upFlag) != 0)
    {
        keyboardMouse.mouseButton(button, false);
    }
}

SurfaceSize clientSize(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);

    return {
        .width = static_cast<std::uint32_t>(client.right - client.left),
        .height = static_cast<std::uint32_t>(client.bottom - client.top),
    };
}

// Rounded corners on Windows 11 while windowed; fullscreen covers the whole
// monitor and must stay square. Older systems reject the attribute.
void applyCorners(HWND window, DWM_WINDOW_CORNER_PREFERENCE corners)
{
    DwmSetWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
}

// Dark title bar and border. The immersive dark mode works from Windows 10
// 20H1 and the colors from Windows 11; older systems reject the attributes
// and keep their default look.
void applyDarkCaption(HWND window)
{
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(window, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
    DwmSetWindowAttribute(window, DWMWA_CAPTION_COLOR, &kCaptionColor, sizeof(kCaptionColor));
    DwmSetWindowAttribute(window, DWMWA_TEXT_COLOR, &kCaptionTextColor, sizeof(kCaptionTextColor));
    DwmSetWindowAttribute(window, DWMWA_BORDER_COLOR, &kBorderColor, sizeof(kBorderColor));
}

core::WindowSize windowSizeOf(HWND window)
{
    const SurfaceSize size = clientSize(window);

    return {
        .width = static_cast<int>(size.width),
        .height = static_cast<int>(size.height),
    };
}

RECT workAreaOf(HWND anchor)
{
    const HMONITOR monitor = anchor != nullptr
                                 ? MonitorFromWindow(anchor, MONITOR_DEFAULTTOPRIMARY)
                                 : MonitorFromPoint({.x = 0, .y = 0}, MONITOR_DEFAULTTOPRIMARY);

    MONITORINFO info{.cbSize = sizeof(MONITORINFO)};
    GetMonitorInfoW(monitor, &info);

    return info.rcWork;
}

core::WindowSize workAreaSizeOf(HWND anchor)
{
    const RECT area = workAreaOf(anchor);

    return {
        .width = static_cast<int>(area.right - area.left),
        .height = static_cast<int>(area.bottom - area.top),
    };
}

// The frame a windowed window needs around a client size.
SIZE frameSizeOf(core::WindowSize client)
{
    RECT frame{.left = 0, .top = 0, .right = client.width, .bottom = client.height};
    AdjustWindowRectEx(&frame, kWindowedStyle, FALSE, 0);

    return {.cx = frame.right - frame.left, .cy = frame.bottom - frame.top};
}

// The requested client size, clamped so that its frame fits the work area.
core::WindowSize fittingClientSize(core::WindowSize wanted, core::WindowSize workArea)
{
    const core::WindowSize clamped = core::clampWindowSize(wanted, workArea);
    const SIZE frame = frameSizeOf(clamped);
    const LONG availableWidth = std::max(1L, workArea.width - (frame.cx - clamped.width));
    const LONG availableHeight = std::max(1L, workArea.height - (frame.cy - clamped.height));

    return {
        .width = static_cast<int>(std::min<LONG>(clamped.width, availableWidth)),
        .height = static_cast<int>(std::min<LONG>(clamped.height, availableHeight)),
    };
}

// The window's frame for a client size, centered in the work area.
RECT initialFrame(const RECT& workArea, core::WindowSize client)
{
    const SIZE frame = frameSizeOf(client);
    const LONG left = workArea.left + std::max(0L, (workArea.right - workArea.left - frame.cx) / 2);
    const LONG top = workArea.top + std::max(0L, (workArea.bottom - workArea.top - frame.cy) / 2);

    return {.left = left, .top = top, .right = left + frame.cx, .bottom = top + frame.cy};
}

} // namespace

std::expected<std::unique_ptr<StreamWindow>, std::string> StreamWindow::open(
    const GpuDevice& gpu, std::shared_ptr<FrameMailbox> mailbox, StreamWindowOptions options)
{
    registerWindowClassOnce(&StreamWindow::windowProc);

    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<StreamWindow> stream(
        new StreamWindow(std::move(options.onClosed), std::move(options.onWindowModeChanged),
                         std::move(options.onWindowedSizeChanged)));

    const core::WindowSize client =
        fittingClientSize(options.windowedSize, workAreaSizeOf(options.monitorOf));
    const RECT frame = initialFrame(workAreaOf(options.monitorOf), client);

    stream->windowedSize_ = client;
    stream->reportedWindowedSize_ = client;

    const HWND window = CreateWindowExW(0, kWindowClassName, options.title.c_str(), kWindowedStyle,
                                       frame.left, frame.top, frame.right - frame.left,
                                       frame.bottom - frame.top, nullptr, nullptr,
                                       GetModuleHandleW(nullptr), stream.get());

    if (window == nullptr)
    {
        return std::unexpected(
            core::describeFailure("CreateWindowExW", HRESULT_FROM_WIN32(GetLastError())));
    }

    applyDarkCaption(window);
    applyCorners(window, DWMWCP_ROUND);
    registerMouse(window);
    stream->keyboardMouse_ = std::move(options.keyboardMouse);

    if (options.mode == core::WindowMode::Fullscreen)
    {
        stream->enterFullscreen();
    }

    FrameRenderer::Options rendererOptions{
        .timing = options.timing,
        .reflex = options.reflex,
        .upscaling = options.upscaling,
        .opticalFlow = options.opticalFlow,
        .streamFramesPerSecond = options.streamFramesPerSecond,
        .frameGeneration = options.frameGeneration,
        .hdr = options.hdr,
        .rtxVideoHdrAvailable = options.rtxVideoHdrAvailable,
        .overlayFonts = std::move(options.overlayFonts),
        .overlay = std::move(options.overlay),
        .overlayActions = std::move(options.overlayActions),
        .metrics = std::move(options.metrics),
        .gamepad = std::move(options.gamepad),
        .onFailure = [window] { PostMessageW(window, kRenderFailedMessage, 0, 0); },
    };

    // The menu's cursor belongs to the window's thread, which owns the
    // message loop.
    const std::function<void(bool)> userMenuChanged =
        std::move(rendererOptions.overlayActions.menuOpenChanged);
    rendererOptions.overlayActions.menuOpenChanged = [window, userMenuChanged](bool open)
    {
        PostMessageW(window, kOverlayMenuMessage, open ? 1 : 0, 0);

        if (userMenuChanged)
        {
            userMenuChanged(open);
        }
    };

    auto renderer = FrameRenderer::start(gpu, window, clientSize(window), std::move(mailbox),
                                         std::move(rendererOptions));

    if (!renderer)
    {
        stream->onClosed_ = nullptr;
        stream->close(CloseReason::RenderFailed);
        return std::unexpected(renderer.error());
    }

    stream->renderer_ = std::move(*renderer);
    stream->updateOverlayLayout();
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);
    stream->applyCursor();

    return stream;
}

StreamWindow::StreamWindow(std::function<void(CloseReason)> onClosed,
                           std::function<void(core::WindowMode)> onWindowModeChanged,
                           std::function<void(core::WindowSize)> onWindowedSizeChanged)
    : onClosed_(std::move(onClosed)),
      onWindowModeChanged_(std::move(onWindowModeChanged)),
      onWindowedSizeChanged_(std::move(onWindowedSizeChanged))
{
}

StreamWindow::~StreamWindow()
{
    onClosed_ = nullptr;
    close(CloseReason::User);
}

bool StreamWindow::isOpen() const
{
    return window_ != nullptr;
}

void StreamWindow::bringToFront()
{
    if (window_ == nullptr)
    {
        return;
    }

    ShowWindow(window_, SW_RESTORE);
    SetForegroundWindow(window_);
}

void StreamWindow::setReflexMode(core::ReflexMode mode)
{
    if (renderer_ != nullptr)
    {
        renderer_->setReflexMode(mode);
    }
}

void StreamWindow::setUpscaling(core::Upscaling upscaling)
{
    if (renderer_ != nullptr)
    {
        renderer_->setUpscaling(upscaling);
    }
}

void StreamWindow::setOpticalFlow(bool enabled)
{
    if (renderer_ != nullptr)
    {
        renderer_->setOpticalFlow(enabled);
    }
}

void StreamWindow::setFrameGeneration(core::FrameGenerationMode mode)
{
    if (renderer_ != nullptr)
    {
        renderer_->setFrameGeneration(mode);
    }
}

void StreamWindow::setHdrMode(core::HdrMode mode)
{
    if (renderer_ != nullptr)
    {
        renderer_->setHdrMode(mode);
    }
}

void StreamWindow::close()
{
    close(CloseReason::User);
}

LRESULT CALLBACK StreamWindow::windowProc(HWND window, UINT message, WPARAM wParam,
                                          LPARAM lParam)
{
    if (message == WM_NCCREATE)
    {
        const auto* creation = reinterpret_cast<const CREATESTRUCTW*>(lParam);
        auto* self = static_cast<StreamWindow*>(creation->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }

    auto* self = reinterpret_cast<StreamWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));

    if (self == nullptr)
    {
        return DefWindowProcW(window, message, wParam, lParam);
    }

    if (message == WM_NCDESTROY)
    {
        SetWindowLongPtrW(window, GWLP_USERDATA, 0);
        self->window_ = nullptr;
        return DefWindowProcW(window, message, wParam, lParam);
    }

    return self->handleMessage(message, wParam, lParam);
}

LRESULT StreamWindow::handleMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (renderer_ != nullptr)
    {
        renderer_->handleOverlayMessage(window_, message, wParam, lParam);
    }

    switch (message)
    {
    case WM_SIZE:
        handleWindowSize(wParam);
        return 0;

    case WM_TIMER:
        if (wParam == kSizeSettleTimer)
        {
            KillTimer(window_, kSizeSettleTimer);
            reportWindowedSize();
            return 0;
        }

        if (wParam == kCursorHideTimer)
        {
            KillTimer(window_, kCursorHideTimer);

            if (!overlayMenuOpen_)
            {
                cursorAwake_ = false;
                applyCursor();
            }

            return 0;
        }
        break;

    case WM_SETCURSOR:
        // While hidden, DefWindowProc would put the class arrow back on every
        // movement; keep the null cursor until the player moves again.
        if (cursorHidden_)
        {
            SetCursor(nullptr);
            return TRUE;
        }
        break;

    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
        noteMouseActivity();
        break;

    case WM_EXITSIZEMOVE:
        if (mode_ == core::WindowMode::Windowed && !applyingLayout_)
        {
            KillTimer(window_, kSizeSettleTimer);
            windowedSize_ = windowSizeOf(window_);
            updateOverlayLayout();
            reportWindowedSize();
        }
        break;

    case WM_KEYDOWN:
        if (keyboardMouse_ != nullptr && wParam != VK_F1 && wParam != VK_F2 &&
            wParam != VK_F11)
        {
            keyboardMouse_->keyDown(static_cast<std::uint16_t>(wParam));
        }

        handleKey(wParam);
        return 0;

    case WM_KEYUP:
        if (keyboardMouse_ != nullptr)
        {
            keyboardMouse_->keyUp(static_cast<std::uint16_t>(wParam));
        }

        return 0;

    case WM_SYSKEYDOWN:
        if (keyboardMouse_ != nullptr)
        {
            keyboardMouse_->keyDown(static_cast<std::uint16_t>(wParam));
        }

        if (wParam == VK_RETURN && (lParam & kAltDownFlag) != 0)
        {
            toggleFullscreen();
            return 0;
        }
        break;

    case WM_SYSKEYUP:
        if (keyboardMouse_ != nullptr)
        {
            keyboardMouse_->keyUp(static_cast<std::uint16_t>(wParam));
        }
        break;

    case WM_KILLFOCUS:
        if (keyboardMouse_ != nullptr)
        {
            keyboardMouse_->releaseAll();
        }
        break;

    case WM_INPUT:
        handleRawInput(lParam);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        close(CloseReason::User);
        return 0;

    case kRenderFailedMessage:
        close(CloseReason::RenderFailed);
        return 0;

    case kOverlayMenuMessage:
        overlayMenuOpen_ = wParam != 0;
        cursorAwake_ = true;

        if (overlayMenuOpen_)
        {
            KillTimer(window_, kCursorHideTimer);
        }
        else
        {
            SetTimer(window_, kCursorHideTimer, kCursorHideMilliseconds, nullptr);
        }

        applyCursor();
        return 0;

    default:
        break;
    }

    return DefWindowProcW(window_, message, wParam, lParam);
}

void StreamWindow::handleKey(WPARAM key)
{
    switch (key)
    {
    case VK_ESCAPE:
        if (renderer_ != nullptr && renderer_->overlayMenuOpen())
        {
            renderer_->toggleOverlayMenu();
            break;
        }

        close(CloseReason::User);
        break;

    case VK_F1:
        if (renderer_ != nullptr)
        {
            renderer_->toggleOverlayMenu();
        }
        break;

    case VK_F2:
        if (renderer_ != nullptr)
        {
            renderer_->toggleOverlayStatistics();
        }
        break;

    case VK_F11:
        toggleFullscreen();
        break;

    default:
        break;
    }
}

void StreamWindow::applyCursor()
{
    if (window_ == nullptr)
    {
        return;
    }

    const bool hidden = mode_ == core::WindowMode::Fullscreen && !overlayMenuOpen_ && !cursorAwake_;

    if (hidden == cursorHidden_)
    {
        return;
    }

    cursorHidden_ = hidden;
    SetCursor(hidden ? nullptr : LoadCursorW(nullptr, IDC_ARROW));
}

void StreamWindow::noteMouseActivity()
{
    if (window_ == nullptr || mode_ != core::WindowMode::Fullscreen)
    {
        return;
    }

    cursorAwake_ = true;
    applyCursor();

    if (!overlayMenuOpen_)
    {
        SetTimer(window_, kCursorHideTimer, kCursorHideMilliseconds, nullptr);
    }
}

void StreamWindow::handleRawInput(LPARAM lParam)
{
    if (keyboardMouse_ == nullptr)
    {
        return;
    }

    UINT size = 0;

    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &size,
                        sizeof(RAWINPUTHEADER)) != 0 ||
        size == 0)
    {
        return;
    }

    // RAWMOUSE and RAWINPUTHEADER fit here; a bigger HID report is not mouse
    // input the stream uses.
    alignas(RAWINPUT) std::array<std::uint8_t, 128> buffer{};

    if (size > buffer.size())
    {
        return;
    }

    if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, buffer.data(), &size,
                        sizeof(RAWINPUTHEADER)) != size)
    {
        return;
    }

    const auto* const raw = reinterpret_cast<const RAWINPUT*>(buffer.data());

    if (raw->header.dwType != RIM_TYPEMOUSE)
    {
        return;
    }

    const RAWMOUSE& mouse = raw->data.mouse;

    if ((mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
    {
        keyboardMouse_->mouseMoved(mouse.lLastX, mouse.lLastY);
    }
    else
    {
        // A remote desktop or drawing tablet reports the cursor's place in a
        // 0..65535 square instead of the movement.
        if (haveAbsoluteMouse_)
        {
            const LONG width = std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN));
            const LONG height = std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN));
            const int deltaX =
                static_cast<int>((mouse.lLastX - lastAbsoluteX_) * width / 65535);
            const int deltaY =
                static_cast<int>((mouse.lLastY - lastAbsoluteY_) * height / 65535);
            keyboardMouse_->mouseMoved(deltaX, deltaY);
        }

        lastAbsoluteX_ = mouse.lLastX;
        lastAbsoluteY_ = mouse.lLastY;
        haveAbsoluteMouse_ = true;
    }

    rawButton(mouse, RI_MOUSE_BUTTON_1_DOWN, RI_MOUSE_BUTTON_1_UP, 0, *keyboardMouse_);
    rawButton(mouse, RI_MOUSE_BUTTON_2_DOWN, RI_MOUSE_BUTTON_2_UP, 1, *keyboardMouse_);
    rawButton(mouse, RI_MOUSE_BUTTON_3_DOWN, RI_MOUSE_BUTTON_3_UP, 2, *keyboardMouse_);
}

void StreamWindow::handleWindowSize(WPARAM sizeCommand)
{
    if (window_ == nullptr || sizeCommand == SIZE_MINIMIZED)
    {
        return;
    }

    if (renderer_ != nullptr)
    {
        renderer_->resize(clientSize(window_));
    }

    if (mode_ != core::WindowMode::Windowed || applyingLayout_)
    {
        return;
    }

    const core::WindowSize size = windowSizeOf(window_);

    if (size.width <= 0 || size.height <= 0)
    {
        return;
    }

    windowedSize_ = size;
    SetTimer(window_, kSizeSettleTimer, kSizeSettleMilliseconds, nullptr);
    updateOverlayLayout();
}

void StreamWindow::toggleFullscreen()
{
    if (mode_ == core::WindowMode::Fullscreen)
    {
        leaveFullscreen();
    }
    else
    {
        enterFullscreen();
    }

    if (onWindowModeChanged_)
    {
        onWindowModeChanged_(mode_);
    }

    updateOverlayLayout();
}

void StreamWindow::applyWindowLayout(core::WindowMode mode, core::WindowSize size)
{
    if (window_ == nullptr)
    {
        return;
    }

    const core::WindowSize fitted = fittingClientSize(size, workAreaSizeOf(window_));
    const bool sizeChanged = fitted != windowedSize_;
    const bool modeChanged = mode != mode_;

    if (!sizeChanged && !modeChanged)
    {
        return;
    }

    if (sizeChanged)
    {
        windowedSize_ = fitted;
    }

    if (modeChanged)
    {
        if (mode == core::WindowMode::Fullscreen)
        {
            enterFullscreen();
        }
        else
        {
            leaveFullscreen();
        }
    }
    else if (sizeChanged && mode_ == core::WindowMode::Windowed)
    {
        applyingLayout_ = true;
        resizeWindowed(fitted);
        applyingLayout_ = false;
    }

    updateOverlayLayout();
}

void StreamWindow::enterFullscreen()
{
    if (window_ == nullptr || mode_ == core::WindowMode::Fullscreen)
    {
        return;
    }

    // A minimized window would otherwise remember a minimized placement and
    // come back minimized when fullscreen ends.
    if (IsIconic(window_) != FALSE)
    {
        ShowWindow(window_, SW_RESTORE);
    }

    MONITORINFO monitor{.cbSize = sizeof(MONITORINFO)};

    if (!GetWindowPlacement(window_, &windowedPlacement_) ||
        !GetMonitorInfoW(MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST), &monitor))
    {
        spdlog::warn("Cannot enter fullscreen: window or monitor information unavailable");
        return;
    }

    const LONG_PTR visible = GetWindowLongPtrW(window_, GWL_STYLE) & WS_VISIBLE;

    // A window still hidden (opening in fullscreen) must come back shown.
    if (visible == 0)
    {
        windowedPlacement_.showCmd = SW_SHOWNORMAL;
    }

    KillTimer(window_, kSizeSettleTimer);

    const RECT& area = monitor.rcMonitor;
    applyingLayout_ = true;
    SetWindowLongPtrW(window_, GWL_STYLE, kFullscreenStyle | visible);
    SetWindowPos(window_, HWND_TOP, area.left, area.top, area.right - area.left,
                 area.bottom - area.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    applyingLayout_ = false;
    applyCorners(window_, DWMWCP_DONOTROUND);
    mode_ = core::WindowMode::Fullscreen;

    // The cursor stays for a moment, in case the player only wanted the
    // bigger picture, and then hides until the next movement.
    cursorAwake_ = true;
    SetTimer(window_, kCursorHideTimer, kCursorHideMilliseconds, nullptr);
    applyCursor();
}

void StreamWindow::leaveFullscreen()
{
    if (window_ == nullptr || mode_ != core::WindowMode::Fullscreen)
    {
        return;
    }

    const LONG_PTR visible = GetWindowLongPtrW(window_, GWL_STYLE) & WS_VISIBLE;
    applyingLayout_ = true;
    SetWindowLongPtrW(window_, GWL_STYLE, kWindowedStyle | visible);
    SetWindowPlacement(window_, &windowedPlacement_);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    mode_ = core::WindowMode::Windowed;
    applyCorners(window_, DWMWCP_ROUND);
    // The remembered size may have changed while fullscreen (from the Vídeo
    // screen or the menu); the placement's own size can be stale.
    resizeWindowed(windowedSize_);
    applyingLayout_ = false;
    KillTimer(window_, kCursorHideTimer);
    applyCursor();
}

// Resizes the windowed window to a client size, keeping it inside the work
// area of the monitor it is on.
void StreamWindow::resizeWindowed(core::WindowSize size)
{
    if (window_ == nullptr || mode_ != core::WindowMode::Windowed)
    {
        return;
    }

    const RECT workArea = workAreaOf(window_);
    const core::WindowSize monitor{.width = static_cast<int>(workArea.right - workArea.left),
                                    .height = static_cast<int>(workArea.bottom - workArea.top)};
    const core::WindowSize fitted = fittingClientSize(size, monitor);
    const SIZE frame = frameSizeOf(fitted);

    RECT current{};
    GetWindowRect(window_, &current);

    const LONG left = std::clamp(current.left, workArea.left,
                                 std::max(workArea.left, workArea.right - frame.cx));
    const LONG top = std::clamp(current.top, workArea.top,
                                std::max(workArea.top, workArea.bottom - frame.cy));
    const bool wasApplying = applyingLayout_;

    applyingLayout_ = true;
    SetWindowPos(window_, nullptr, left, top, frame.cx, frame.cy,
                 SWP_NOZORDER | SWP_NOACTIVATE);
    applyingLayout_ = wasApplying;

    // A minimized window reports a 0×0 client; keep the size it will restore
    // to instead.
    const core::WindowSize actual = windowSizeOf(window_);
    windowedSize_ = actual.width > 0 && actual.height > 0 ? actual : fitted;
    reportedWindowedSize_ = windowedSize_;
}

void StreamWindow::reportWindowedSize()
{
    if (mode_ != core::WindowMode::Windowed || windowedSize_ == reportedWindowedSize_)
    {
        return;
    }

    reportedWindowedSize_ = windowedSize_;
    spdlog::info("Windowed size: {}x{}", windowedSize_.width, windowedSize_.height);

    if (onWindowedSizeChanged_)
    {
        onWindowedSizeChanged_(windowedSize_);
    }
}

void StreamWindow::updateOverlayLayout()
{
    if (renderer_ != nullptr)
    {
        renderer_->setWindowLayout(mode_, windowedSize_);
    }
}

// The renderer goes first: its thread must stop presenting before the window
// it presents to is destroyed.
void StreamWindow::close(CloseReason reason)
{
    if (keyboardMouse_ != nullptr)
    {
        keyboardMouse_->releaseAll();
    }

    KillTimer(window_, kSizeSettleTimer);
    KillTimer(window_, kCursorHideTimer);
    cursorHidden_ = false;
    SetCursor(LoadCursorW(nullptr, IDC_ARROW));
    renderer_.reset();

    if (window_ != nullptr)
    {
        DestroyWindow(window_);
    }

    if (onClosed_)
    {
        const auto onClosed = std::exchange(onClosed_, nullptr);
        onClosed(reason);
    }
}

} // namespace mfplay::render
