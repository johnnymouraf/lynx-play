import QtQuick
import QtQuick.Templates as T
import LynxPlay

// One choice of a SegmentedControl.
T.Button {
    id: control

    property bool selected: false

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: 36
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.RadioButton
    Accessible.checked: control.selected

    contentItem: AppText {
        id: label

        text: control.text
        textStyle: "labelMedium"
        color: Theme.textMuted
        horizontalAlignment: Text.AlignHCenter
    }

    background: Rectangle {
        id: fill

        radius: Theme.radiusSm
        color: "transparent"

        FocusRing {
            targetRadius: fill.radius
            visible: control.visualFocus
        }
    }

    states: [
        State {
            name: "selected"
            when: control.selected

            PropertyChanges {
                target: fill
                color: Theme.accentSubtle
            }

            PropertyChanges {
                target: label
                color: Theme.accent
            }
        },
        State {
            name: "highlighted"
            when: control.hovered || control.visualFocus

            PropertyChanges {
                target: fill
                color: Theme.surfaceHigh
            }

            PropertyChanges {
                target: label
                color: Theme.text
            }
        }
    ]
}
