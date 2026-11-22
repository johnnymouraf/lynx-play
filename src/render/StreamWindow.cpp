#include "render/StreamWindow.h"

#include "core/Windows.h"

#include <mutex>

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

} // namespace

std::expected<std::unique_ptr<StreamWindow>, std::string> StreamWindow::open(
    std::wstring_view title)
{
    registerWindowClassOnce(&StreamWindow::windowProc);

    // The constructor is private, out of make_unique's reach.
    std::unique_ptr<StreamWindow> stream(new StreamWindow());

    RECT frame{0, 0, kInitialClientWidth, kInitialClientHeight};
    AdjustWindowRectEx(&frame, kWindowedStyle, FALSE, 0);

    const std::wstring titleText(title);
    const HWND window = CreateWindowExW(0, kWindowClassName, titleText.c_str(), kWindowedStyle,
                                       CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left,
                                       frame.bottom - frame.top, nullptr, nullptr,
                                       GetModuleHandleW(nullptr), stream.get());

    if (window == nullptr)
    {
        return std::unexpected(
            core::describeFailure("CreateWindowExW", HRESULT_FROM_WIN32(GetLastError())));
    }

    auto renderer = FrameRenderer::start(window, clientSize(window));

    if (!renderer)
    {
        stream->close();
        return std::unexpected(renderer.error());
    }

    stream->renderer_ = std::move(*renderer);
    ShowWindow(window, SW_SHOWNORMAL);

    return stream;
}

StreamWindow::~StreamWindow()
{
    close();
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
        close();
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
        close();
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

    const RECT& area = monitor.rcMonitor;
    SetWindowLongPtrW(window_, GWL_STYLE, kFullscreenStyle | WS_VISIBLE);
    SetWindowPos(window_, HWND_TOP, area.left, area.top, area.right - area.left,
                 area.bottom - area.top, SWP_FRAMECHANGED | SWP_NOOWNERZORDER);
    fullscreen_ = true;
}

void StreamWindow::leaveFullscreen()
{
    SetWindowLongPtrW(window_, GWL_STYLE, kWindowedStyle | WS_VISIBLE);
    SetWindowPlacement(window_, &windowedPlacement_);
    SetWindowPos(window_, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    fullscreen_ = false;
}

// The renderer goes first: its thread must stop presenting before the window
// it presents to is destroyed.
void StreamWindow::close()
{
    renderer_.reset();

    if (window_ != nullptr)
    {
        DestroyWindow(window_);
    }
}

} // namespace lynx::render
