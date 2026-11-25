#include "app/WindowFrame.h"

#include <QCoreApplication>

#include <array>

#include <dwmapi.h>
#include <shellapi.h>
#include <windowsx.h>

namespace mfplay::app
{

namespace
{

struct FrameThickness
{
    int horizontal;
    int vertical;
};

// The width Windows gives the sizing frame at the window's DPI.
FrameThickness frameThickness(HWND handle)
{
    const UINT dpi = GetDpiForWindow(handle);
    const int padding = GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);

    return {
        .horizontal = GetSystemMetricsForDpi(SM_CXFRAME, dpi) + padding,
        .vertical = GetSystemMetricsForDpi(SM_CYFRAME, dpi) + padding,
    };
}

// Picks the left, right or middle code from where the point sits horizontally.
LRESULT edgeOrCorner(bool onLeft, bool onRight, LRESULT leftCode, LRESULT rightCode,
                     LRESULT middleCode)
{
    if (onLeft)
    {
        return leftCode;
    }

    return onRight ? rightCode : middleCode;
}

// Which sizing border or corner the screen point is on; HTCLIENT when none,
// which is always the case while maximized.
LRESULT resizeBorderAt(HWND handle, POINT screenPoint)
{
    if (IsZoomed(handle))
    {
        return HTCLIENT;
    }

    RECT bounds{};
    GetWindowRect(handle, &bounds);

    const auto [horizontal, vertical] = frameThickness(handle);
    const bool onLeft = screenPoint.x < bounds.left + horizontal;
    const bool onRight = screenPoint.x >= bounds.right - horizontal;
    const bool onTop = screenPoint.y < bounds.top + vertical;
    const bool onBottom = screenPoint.y >= bounds.bottom - vertical;

    if (onTop)
    {
        return edgeOrCorner(onLeft, onRight, HTTOPLEFT, HTTOPRIGHT, HTTOP);
    }

    if (onBottom)
    {
        return edgeOrCorner(onLeft, onRight, HTBOTTOMLEFT, HTBOTTOMRIGHT, HTBOTTOM);
    }

    return edgeOrCorner(onLeft, onRight, HTLEFT, HTRIGHT, HTCLIENT);
}

// A maximized window overhangs the monitor by the sizing frame; the client
// area takes back exactly that. An auto-hide taskbar needs a one pixel strip
// to be summoned from, or the window would cover the edge it hides on.
void fitMaximizedClient(HWND handle, RECT& client)
{
    const auto [horizontal, vertical] = frameThickness(handle);
    client.left += horizontal;
    client.right -= horizontal;
    client.top += vertical;
    client.bottom -= vertical;

    MONITORINFO monitor{.cbSize = sizeof(MONITORINFO)};
    GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor);

    struct Edge
    {
        UINT appBarEdge;
        LONG RECT::* side;
        LONG inset;
    };

    constexpr std::array edges{
        Edge{ABE_LEFT, &RECT::left, 1},
        Edge{ABE_TOP, &RECT::top, 1},
        Edge{ABE_RIGHT, &RECT::right, -1},
        Edge{ABE_BOTTOM, &RECT::bottom, -1},
    };

    for (const Edge& edge : edges)
    {
        APPBARDATA appBar{.cbSize = sizeof(APPBARDATA), .uEdge = edge.appBarEdge};
        appBar.rc = monitor.rcMonitor;

        if (SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &appBar) != 0)
        {
            client.*edge.side += edge.inset;
        }
    }
}

} // namespace

WindowFrame::WindowFrame(QObject* parent) : QObject(parent)
{
    QCoreApplication::instance()->installNativeEventFilter(this);
}

WindowFrame::~WindowFrame()
{
    QCoreApplication::instance()->removeNativeEventFilter(this);
}

QQuickWindow* WindowFrame::window() const
{
    return window_;
}

void WindowFrame::setWindow(QQuickWindow* window)
{
    if (window_ == window)
    {
        return;
    }

    window_ = window;
    framed_ = false;

    if (window_ != nullptr && window_->handle() != nullptr)
    {
        adoptFrame(reinterpret_cast<HWND>(window_->winId()));
    }

    emit windowChanged();
}

QQuickItem* WindowFrame::titleBar() const
{
    return titleBar_;
}

void WindowFrame::setTitleBar(QQuickItem* item)
{
    titleBar_ = item;
}

QQuickItem* WindowFrame::minimizeButton() const
{
    return minimizeButton_;
}

void WindowFrame::setMinimizeButton(QQuickItem* item)
{
    minimizeButton_ = item;
}

QQuickItem* WindowFrame::maximizeButton() const
{
    return maximizeButton_;
}

void WindowFrame::setMaximizeButton(QQuickItem* item)
{
    maximizeButton_ = item;
}

QQuickItem* WindowFrame::closeButton() const
{
    return closeButton_;
}

void WindowFrame::setCloseButton(QQuickItem* item)
{
    closeButton_ = item;
}

bool WindowFrame::maximizeHovered() const
{
    return maximizeHovered_;
}

bool WindowFrame::maximizePressed() const
{
    return maximizePressed_;
}

bool WindowFrame::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType != "windows_generic_MSG" || window_ == nullptr || window_->handle() == nullptr)
    {
        return false;
    }

    const auto& native = *static_cast<MSG*>(message);
    const auto handle = reinterpret_cast<HWND>(window_->winId());

    if (native.hwnd != handle)
    {
        return false;
    }

    adoptFrame(handle);

    switch (native.message)
    {
    case WM_NCCALCSIZE:
    {
        *result = 0;

        if (native.wParam != FALSE && IsZoomed(handle))
        {
            auto& parameters = *reinterpret_cast<NCCALCSIZE_PARAMS*>(native.lParam);
            fitMaximizedClient(handle, parameters.rgrc[0]);
        }

        return true;
    }
    case WM_NCHITTEST:
    {
        const POINT screenPoint{.x = GET_X_LPARAM(native.lParam), .y = GET_Y_LPARAM(native.lParam)};
        *result = hitTest(handle, screenPoint);

        return true;
    }
    case WM_GETMINMAXINFO:
    {
        // Qt adds the caption it assumes from the window styles to the
        // minimum size, but the client area is the whole window here.
        const qreal scale = window_->devicePixelRatio();
        auto& limits = *reinterpret_cast<MINMAXINFO*>(native.lParam);
        limits.ptMinTrackSize.x = qRound(window_->minimumWidth() * scale);
        limits.ptMinTrackSize.y = qRound(window_->minimumHeight() * scale);
        *result = 0;

        return true;
    }
    default:
        return handleMaximizeButton(handle, native, result);
    }
}

// Windows resizes and moves by itself once the hit test says where the borders
// and the caption are; a new frame has to be requested once the styles and the
// handler are both in place.
void WindowFrame::adoptFrame(HWND handle)
{
    if (framed_)
    {
        return;
    }

    framed_ = true;

    // Windows 11 rounds the corners of a window with a frame, and DWM squares
    // them while maximized. Windows 10 rejects the attribute and stays square.
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(handle, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));

    SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                 SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

LRESULT WindowFrame::hitTest(HWND handle, POINT screenPoint) const
{
    POINT clientPoint = screenPoint;
    ScreenToClient(handle, &clientPoint);

    const bool overMaximize = isOver(maximizeButton_, clientPoint);
    const bool overClientButton =
        isOver(minimizeButton_, clientPoint) || isOver(closeButton_, clientPoint);
    const LRESULT border = resizeBorderAt(handle, screenPoint);

    // The buttons win over the top border; the corners still resize.
    const bool buttonOnTopBorder = border == HTTOP && (overMaximize || overClientButton);

    if (border != HTCLIENT && !buttonOnTopBorder)
    {
        return border;
    }

    if (overMaximize)
    {
        return HTMAXBUTTON;
    }

    if (overClientButton)
    {
        return HTCLIENT;
    }

    return isOver(titleBar_, clientPoint) ? HTCAPTION : HTCLIENT;
}

bool WindowFrame::isOver(const QQuickItem* item, POINT clientPoint) const
{
    if (item == nullptr || !item->isVisible())
    {
        return false;
    }

    const qreal scale = window_->devicePixelRatio();
    const QRectF bounds = item->mapRectToScene(item->boundingRect());
    const QRectF pixels(bounds.x() * scale, bounds.y() * scale, bounds.width() * scale,
                        bounds.height() * scale);

    return pixels.contains(QPointF(clientPoint.x, clientPoint.y));
}

// The maximize button's clicks arrive as non-client messages. Handling them
// here, instead of by DefWindowProc, keeps Windows from drawing its own button
// over the QML one.
bool WindowFrame::handleMaximizeButton(HWND handle, const MSG& message, qintptr* result)
{
    const bool onButton = message.wParam == HTMAXBUTTON;

    switch (message.message)
    {
    case WM_NCMOUSEMOVE:
    {
        if (onButton)
        {
            TRACKMOUSEEVENT tracking{.cbSize = sizeof(TRACKMOUSEEVENT),
                                     .dwFlags = TME_LEAVE | TME_NONCLIENT,
                                     .hwndTrack = handle};
            TrackMouseEvent(&tracking);
        }

        setMaximizeState(onButton, onButton && maximizePressed_);

        return false;
    }
    case WM_NCMOUSELEAVE:
    case WM_MOUSEMOVE:
        setMaximizeState(false, false);

        return false;
    case WM_LBUTTONUP:
        setMaximizeState(maximizeHovered_, false);

        return false;
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
    {
        if (!onButton)
        {
            return false;
        }

        setMaximizeState(true, true);
        *result = 0;

        return true;
    }
    case WM_NCLBUTTONUP:
    {
        if (!onButton)
        {
            return false;
        }

        const bool wasPressed = maximizePressed_;
        setMaximizeState(true, false);
        *result = 0;

        if (wasPressed)
        {
            const WPARAM command = IsZoomed(handle) ? SC_RESTORE : SC_MAXIMIZE;
            PostMessageW(handle, WM_SYSCOMMAND, command, 0);
        }

        return true;
    }
    default:
        return false;
    }
}

void WindowFrame::setMaximizeState(bool hovered, bool pressed)
{
    if (maximizeHovered_ == hovered && maximizePressed_ == pressed)
    {
        return;
    }

    maximizeHovered_ = hovered;
    maximizePressed_ = pressed;
    emit maximizeStateChanged();
}

} // namespace mfplay::app
