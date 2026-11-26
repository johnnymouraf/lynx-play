import QtQuick
import QtQuick.Layouts
import MFPlay

// The window's title bar, in place of the system's. It shares the rail's and
// the content's backgrounds so the window reads as one surface; the empty bar
// drags the window and the buttons are the system's three controls. WindowFrame
// tells Windows where each one is, which keeps Snap, the Snap Layouts flyout
// and the system menu working.
Item {
    id: bar

    readonly property Window appWindow: Window.window

    readonly property bool maximized: bar.appWindow.visibility === Window.Maximized

    implicitHeight: Theme.sizeControlSm

    WindowFrame {
        id: frame

        window: bar.appWindow
        titleBar: bar
        minimizeButton: minimizeControl
        maximizeButton: maximizeControl
        closeButton: closeControl
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bg
    }

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: Theme.sizeRail
        color: Theme.menu
    }

    RowLayout {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        spacing: 0

        CaptionButton {
            id: minimizeControl

            Layout.fillHeight: true
            glyph: "\uE921"
            tooltip: "Minimizar"
            onClicked: bar.appWindow.showMinimized()
        }

        CaptionButton {
            id: maximizeControl

            Layout.fillHeight: true
            glyph: bar.maximized ? "\uE923" : "\uE922"
            tooltip: bar.maximized ? "Restaurar" : "Maximizar"
            nativeHovered: frame.maximizeHovered
            nativePressed: frame.maximizePressed
        }

        CaptionButton {
            id: closeControl

            Layout.fillHeight: true
            glyph: "\uE8BB"
            tooltip: "Fechar"
            danger: true
            onClicked: bar.appWindow.close()
        }
    }
}
