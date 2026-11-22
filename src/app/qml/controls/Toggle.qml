import QtQuick
import QtQuick.Templates as T
import LynxPlay

// The design's Toggle: an amber track with a dark thumb when on.
T.Switch {
    id: control

    implicitWidth: indicator.implicitWidth
    implicitHeight: indicator.implicitHeight
    padding: 0
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus

    indicator: Rectangle {
        id: track

        x: control.leftPadding
        y: control.topPadding + (control.availableHeight - height) / 2
        implicitWidth: 48
        implicitHeight: 28
        width: implicitWidth
        height: implicitHeight
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.surfaceHigh
        border.width: control.checked ? 0 : 1
        border.color: Theme.borderControl

        Rectangle {
            y: 3
            width: 22
            height: 22
            radius: width / 2
            color: control.checked ? Theme.textOnAccent : Theme.textMuted
            x: control.checked ? track.width - width - 3 : 3

            Behavior on x {
                NumberAnimation {
                    duration: 120
                    easing.type: Easing.OutCubic
                }
            }
        }

        FocusRing {
            targetRadius: track.radius
            visible: control.visualFocus
        }
    }

    contentItem: Item {}
}
