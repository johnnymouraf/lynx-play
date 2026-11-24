import QtQuick
import QtQuick.Templates as T
import MFPlay

// A square button with only an icon. `text` is not shown; it names the button
// for screen readers and UI Automation.
T.Button {
    id: control

    property string iconName: "x"
    property int iconSize: Theme.sizeIcon
    property color restingFill: "transparent"

    implicitWidth: Theme.sizeControl
    implicitHeight: Theme.sizeControl
    padding: 0
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: control.iconSize
            color: control.enabled ? Theme.textMuted : Theme.textDisabled
        }
    }

    background: Rectangle {
        id: fill

        radius: Theme.radiusMd
        color: control.restingFill

        FocusRing {
            targetRadius: fill.radius
            visible: control.visualFocus
        }
    }

    states: [
        State {
            name: "pressed"
            when: control.down

            PropertyChanges {
                target: fill
                color: Theme.surfaceHover
            }
        },
        State {
            name: "highlighted"
            when: control.hovered || control.visualFocus

            PropertyChanges {
                target: fill
                color: Theme.surfaceHigh
            }
        }
    ]
}
