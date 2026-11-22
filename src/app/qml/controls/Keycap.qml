import QtQuick
import LynxPlay

// A keycap in the Controles screen; clicking it starts capturing the key or
// mouse button that should replace the binding.
Rectangle {
    id: keycap

    property string text
    property bool editable: true
    property bool capturing

    signal activated
    signal mouseButton(int button)

    implicitWidth: Math.max(24, label.implicitWidth + 2 * Theme.space2)
    implicitHeight: 26
    radius: Theme.radiusSm
    color: Theme.surfaceHigh
    border.width: capturing ? Theme.focusRingWidth : 1
    border.color: capturing ? Theme.focus : Theme.borderStrong
    opacity: editable ? 1 : 0.6

    AppText {
        id: label

        anchors.centerIn: parent
        text: keycap.capturing ? "…" : keycap.text
        textStyle: "monoMedium"
        color: Theme.text
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 2
        radius: Theme.radiusSm
        color: keycap.capturing ? Theme.focus : Theme.borderStrong
    }

    MouseArea {
        anchors.fill: parent
        enabled: keycap.editable
        acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
        cursorShape: Qt.PointingHandCursor

        onClicked: keycap.activated()

        onPressed: mouse => {
            if (keycap.capturing && mouse.button !== Qt.LeftButton) {
                keycap.mouseButton(mouse.button);
            }
        }
    }
}
