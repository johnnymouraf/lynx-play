#include "render/StreamWindow.h"

#include "core/Windows.h"

#include <algorithm>
#include <mutex>
#include <utility>

#include <spdlog/spdlog.h>

namespace lynx::render
{

namespace
{

constexpr auto kWindowClassName = L"LynxPlayStreamWindow";
constexpr LONG kInitialClientWidth = 1280;
constexpr LONG kInitialClientHeight = 720;
constexpr DWORD kWindowedStyle = WS_OVERLAPPEDWINDOW;
constexpr DWORD kFullscreenStyle = WS_POPUP;

// Posted by the renderer's thread when it stops for good.
constexpr UINT kRenderFailedMessage = WM_APP + 1;

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

SurfaceSize clientSize(HWND window)
{
    RECT client{};
    GetClientRect(window, &client);

    return {
        .width = static_cast<std::uint32_t>(client.right - client.left),
        .height = static_cast<std::uint32_t>(client.bottom - client.top),
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

// The window's frame for the initial client size, centered in the work area.
RECT initialFrame(const RECT& workArea)
{
    RECT frame{.left = 0, .top = 0, .right = kInitialClientWidth, .bottom = kInitialClientHeight};
    AdjustWindowRectEx(&frame, kWindowedStyle, FALSE, 0);

    const LONG width = frame.right - frame.left;
    const LONG height = frame.bottom - frame.top;
    const LONG left = workArea.left + std::max(0L, (workArea.right - workArea.left - width) / 2);
    const LONG top = workArea.top + std::max(0L, (workArea.bottom - workArea.top - height) / 2);

    return {.left = left, .top = top, .right = left + width, .bottom = top + height};
}

} // namespace

std::expected<std::unique_ptr<StreamWindow>, std::string> StreamWindow::open(
    const GpuDevice& gpu, std::shared_ptr<FrameMailbox> mailbox, StreamWindowOptions options)
{
    registerWindowClassOnce(&StreamWindow::windowProc);

    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<StreamWindow> stream(new StreamWindow(std::move(options.onClosed)));

    const RECT frame = initialFrame(workAreaOf(options.monitorOf));
    const HWND window = CreateWindowExW(0, kWindowClassName, options.title.c_str(), kWindowedStyle,
                                       frame.left, frame.top, frame.right - frame.left,
                                       frame.bottom - frame.top, nullptr, nullptr,
                                       GetModuleHandleW(nullptr), stream.get());

    if (window == nullptr)
    {
        return std::unexpected(
            core::describeFailure("CreateWindowExW", HRESULT_FROM_WIN32(GetLastError())));
    }

    if (options.mode == WindowMode::Fullscreen)
    {
        stream->enterFullscreen();
    }

    FrameRenderer::Options rendererOptions{
        .timing = options.timing,
        .onFailure = [window] { PostMessageW(window, kRenderFailedMessage, 0, 0); },
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
    ShowWindow(window, SW_SHOW);
    SetForegroundWindow(window);

    return stream;
}

StreamWindow::StreamWindow(std::function<void(CloseReason)> onClosed)
    : onClosed_(std::move(onClosed))
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
    switch (message)
    {
    case WM_SIZE:
        if (renderer_ != nullptr && wParam != SIZE_MINIMIZED)
        {
            renderer_->resize(clientSize(window_));
        }
        return 0;

    case WM_KEYDOWN:
        handleKey(wParam);
        return 0;

    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & kAltDownFlag) != 0)
        {
            toggleFullscreen();
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_CLOSE:
        close(CloseReason::User);
        return 0;

    case kRenderFailedMessage:
        close(CloseReason::RenderFailed);
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
        close(CloseReason::User);
        break;

    case VK_F11:
        toggleFullscreen();
        break;

    default:
        break;
    }
}

void StreamWindow::toggleFullscreen()
{
    if (fullscreen_)
    {
        leaveFullscreen();
        return;
    }

    enterFullscreen();
}

void StreamWindow::enterFullscreen()
{
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

    const RECT& area = monitor.rcMonitor;
    SetWindowLongPtrW(window_, GWL_STYLE, kFullscreenStyle | visible);
    SetWindowPos(window_, HWND_TOP, area.left, area.top, area.right - area.left,
                 area.bottom - area.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    fullscreen_ = true;
}

void StreamWindow::leaveFullscreen()
{
    const LONG_PTR visible = GetWindowLongPtrW(window_, GWL_STYLE) & WS_VISIBLE;
    SetWindowLongPtrW(window_, GWL_STYLE, kWindowedStyle | visible);
    SetWindowPlacement(window_, &windowedPlacement_);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    fullscreen_ = false;
}

// The renderer goes first: its thread must stop presenting before the window
// it presents to is destroyed.
void StreamWindow::close(CloseReason reason)
{
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

} // namespace lynx::render
