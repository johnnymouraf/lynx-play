import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's NavItem: an entry of the app rail or of a section list, with
// its icon in a tile.
T.Button {
    id: control

    property string iconName
    property bool selected: false

    implicitWidth: 200
    implicitHeight: Theme.sizeControlLg
    leftPadding: 10
    rightPadding: 10
    spacing: Theme.space3
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Accessible.role: Accessible.PageTab
    Accessible.selected: control.selected

    contentItem: Row {
        spacing: control.spacing

        IconTile {
            id: tile

            anchors.verticalCenter: parent.verticalCenter
            iconName: control.iconName
            iconColor: Theme.textMuted
            color: Theme.surfaceRaised
        }

        AppText {
            id: label

            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            textStyle: "labelLarge"
        }
    }

    background: Rectangle {
        id: fill

        radius: Theme.radiusMd
        color: Theme.surface

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
                target: tile
                color: Theme.accentPressed
                iconColor: Theme.accentText
            }

            PropertyChanges {
                target: label
                color: Theme.accentText
            }
        },
        State {
            name: "hovered"
            when: control.hovered

            PropertyChanges {
                target: fill
                color: Theme.surfaceHover
            }

            PropertyChanges {
                target: tile
                iconColor: Theme.text
            }
        },
        State {
            name: "focused"
            when: control.visualFocus

            PropertyChanges {
                target: tile
                iconColor: Theme.text
            }
        }
    ]
}
