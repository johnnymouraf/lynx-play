import QtQuick
import QtQuick.Templates as T
import LynxPlay

// An entry of the app rail or of a section list.
T.Button {
    id: control

    property string iconName
    property bool selected: false

    implicitWidth: 216
    implicitHeight: 48
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    spacing: Theme.space3
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.PageTab
    Accessible.selected: control.selected

    contentItem: Row {
        spacing: control.spacing

        Icon {
            id: icon

            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: 22
            color: Theme.textMuted
        }

        AppText {
            id: label

            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            textStyle: "labelLarge"
            color: Theme.textMuted
        }
    }

    background: Rectangle {
        id: fill

        radius: Theme.radiusMd
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
                target: icon
                color: Theme.accent
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
                target: icon
                color: Theme.text
            }

            PropertyChanges {
                target: label
                color: Theme.text
            }
        }
    ]
}
