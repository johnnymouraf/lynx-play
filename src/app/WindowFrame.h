#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtQmlIntegration>

#include <windows.h>

namespace mfplay::app
{

// Lets QML draw a Qt Quick window's title bar while Windows keeps treating the
// window as a captioned, resizable one: Aero Snap, the Windows 11 Snap Layouts
// flyout, the shadow, the animations and the system menu all keep working.
//
// The window keeps its caption and sizing frame styles, but the client area
// takes the whole window (WM_NCCALCSIZE) and QML tells Windows where its title
// bar and buttons are (WM_NCHITTEST). The maximize button lives in the
// non-client area, so QML never sees the mouse over it; the hover and pressed
// state come from here.
class WindowFrame : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QQuickWindow* window READ window WRITE setWindow NOTIFY windowChanged)
    // The bar's items: the title bar drags the window, except over the
    // buttons, which QML handles itself (minimize, close) or Windows does
    // (maximize).
    Q_PROPERTY(QQuickItem* titleBar READ titleBar WRITE setTitleBar)
    Q_PROPERTY(QQuickItem* minimizeButton READ minimizeButton WRITE setMinimizeButton)
    Q_PROPERTY(QQuickItem* maximizeButton READ maximizeButton WRITE setMaximizeButton)
    Q_PROPERTY(QQuickItem* closeButton READ closeButton WRITE setCloseButton)
    Q_PROPERTY(bool maximizeHovered READ maximizeHovered NOTIFY maximizeStateChanged)
    Q_PROPERTY(bool maximizePressed READ maximizePressed NOTIFY maximizeStateChanged)

public:
    explicit WindowFrame(QObject* parent = nullptr);
    ~WindowFrame() override;

    [[nodiscard]] QQuickWindow* window() const;
    void setWindow(QQuickWindow* window);
    [[nodiscard]] QQuickItem* titleBar() const;
    void setTitleBar(QQuickItem* item);
    [[nodiscard]] QQuickItem* minimizeButton() const;
    void setMinimizeButton(QQuickItem* item);
    [[nodiscard]] QQuickItem* maximizeButton() const;
    void setMaximizeButton(QQuickItem* item);
    [[nodiscard]] QQuickItem* closeButton() const;
    void setCloseButton(QQuickItem* item);
    [[nodiscard]] bool maximizeHovered() const;
    [[nodiscard]] bool maximizePressed() const;

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void windowChanged();
    void maximizeStateChanged();

private:
    void adoptFrame(HWND handle);
    LRESULT hitTest(HWND handle, POINT screenPoint) const;
    [[nodiscard]] bool isOver(const QQuickItem* item, POINT clientPoint) const;
    bool handleMaximizeButton(HWND handle, const MSG& message, qintptr* result);
    void setMaximizeState(bool hovered, bool pressed);

    QPointer<QQuickWindow> window_;
    QPointer<QQuickItem> titleBar_;
    QPointer<QQuickItem> minimizeButton_;
    QPointer<QQuickItem> maximizeButton_;
    QPointer<QQuickItem> closeButton_;
    bool framed_ = false;
    bool maximizeHovered_ = false;
    bool maximizePressed_ = false;
};

} // namespace mfplay::app
