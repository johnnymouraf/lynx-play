import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's Button: primary, secondary, ghost or danger, 52 px tall, with
// the icon in a tile before the label.
T.Button {
    id: control

    property string variant: "secondary"
    property string iconName: ""

    readonly property var tones: ({
        primary: {
            fill: Theme.accent,
            hoverFill: Theme.accentHover,
            pressedFill: Theme.accentPressed,
            disabledFill: Theme.surfaceHigh,
            content: Theme.accentText,
            hoverContent: Theme.accentText,
            tile: Theme.tileOnAccent
        },
        secondary: {
            fill: Theme.surfaceHigh,
            hoverFill: Theme.surfaceHover,
            pressedFill: Theme.surfaceHover,
            disabledFill: Theme.surfaceRaised,
            content: Theme.text,
            hoverContent: Theme.text,
            tile: Theme.tileOnSurface
        },
        ghost: {
            fill: "transparent",
            hoverFill: Theme.surfaceHigh,
            pressedFill: Theme.surfaceHover,
            disabledFill: "transparent",
            content: Theme.text,
            hoverContent: Theme.text,
            tile: Theme.tileOnSurface
        },
        danger: {
            fill: Theme.dangerSubtle,
            hoverFill: Theme.danger,
            pressedFill: Theme.danger,
            disabledFill: Theme.surfaceRaised,
            content: Theme.danger,
            hoverContent: Theme.dangerText,
            tile: Theme.tileOnAccent
        }
    })
    readonly property var tone: tones[variant]

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.sizeControlLg
    leftPadding: Theme.space4
    rightPadding: Theme.space4
    spacing: Theme.space3
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row

            anchors.centerIn: parent
            spacing: control.spacing

            IconTile {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
                iconName: control.iconName
                iconColor: label.color
                color: control.tone.tile
            }

            AppText {
                id: label

                anchors.verticalCenter: parent.verticalCenter
                text: control.text
                textStyle: "labelButton"
                color: control.tone.content
            }
        }
    }

    background: Rectangle {
        id: fill

        implicitHeight: Theme.sizeControlLg
        radius: Theme.radiusMd
        color: control.tone.fill

        FocusRing {
            targetRadius: fill.radius
            visible: control.visualFocus
        }
    }

    states: [
        State {
            name: "disabled"
            when: !control.enabled

            PropertyChanges {
                target: fill
                color: control.tone.disabledFill
            }

            PropertyChanges {
                target: label
                color: Theme.textDisabled
            }
        },
        State {
            name: "pressed"
            when: control.down

            PropertyChanges {
                target: fill
                color: control.tone.pressedFill
            }

            PropertyChanges {
                target: label
                color: control.tone.hoverContent
            }
        },
        State {
            name: "hovered"
            when: control.hovered

            PropertyChanges {
                target: fill
                color: control.tone.hoverFill
            }

            PropertyChanges {
                target: label
                color: control.tone.hoverContent
            }
        },
        State {
            name: "focused"
            when: control.visualFocus && control.variant === "ghost"

            PropertyChanges {
                target: fill
                color: Theme.surfaceHigh
            }
        }
    ]
}
