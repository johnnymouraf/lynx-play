import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's Toggle: an accent track with a white thumb when on.
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
        color: Theme.surfaceHigh
        border.width: 1
        border.color: Theme.borderControl

        Rectangle {
            id: thumb

            y: 3
            width: 22
            height: 22
            radius: width / 2
            color: Theme.textMuted
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

    states: [
        State {
            name: "disabled"
            when: !control.enabled

            PropertyChanges {
                target: track
                color: Theme.surfaceRaised
                border.width: 0
            }

            PropertyChanges {
                target: thumb
                color: Theme.textDisabled
            }
        },
        State {
            name: "checked"
            when: control.checked

            PropertyChanges {
                target: track
                color: Theme.accent
                border.width: 0
            }

            PropertyChanges {
                target: thumb
                color: Theme.onAccent
            }
        }
    ]
}
