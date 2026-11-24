import QtQuick
import QtQuick.Templates as T
import MFPlay

// One choice of a SegmentedControl.
T.Button {
    id: control

    property bool selected: false
    // A choice the control offers but can't take right now.
    property bool optionEnabled: true

    enabled: optionEnabled
    opacity: enabled ? 1 : 0.5

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
                color: Theme.accent
            }

            PropertyChanges {
                target: label
                color: Theme.onAccent
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
