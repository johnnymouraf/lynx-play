import QtQuick
import QtQuick.Templates as T
import MFPlay

// An accent text button with a leading icon, for links and light actions.
T.Button {
    id: control

    property string iconName: ""

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: Theme.sizeControlSm
    leftPadding: Theme.space2
    rightPadding: Theme.space2
    spacing: Theme.space2
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    contentItem: Row {
        spacing: control.spacing

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            visible: control.iconName.length > 0
            name: control.iconName
            size: 18
            color: control.down ? Theme.accentPressed : Theme.accent
        }

        AppText {
            id: label

            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            textStyle: "labelMedium"
            color: Theme.accent
        }
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
            name: "pressed"
            when: control.down

            PropertyChanges {
                target: fill
                color: Theme.surfaceHigh
            }

            PropertyChanges {
                target: label
                color: Theme.accentPressed
            }
        },
        State {
            name: "hovered"
            when: control.hovered

            PropertyChanges {
                target: fill
                color: Theme.surfaceHigh
            }

            PropertyChanges {
                target: label
                color: Theme.accentHover
            }
        },
        State {
            name: "focused"
            when: control.visualFocus

            PropertyChanges {
                target: fill
                color: Theme.surfaceHigh
            }
        }
    ]
}
