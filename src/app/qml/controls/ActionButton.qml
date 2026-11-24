import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's Button: primary, secondary, ghost or danger, with an optional
// leading icon.
T.Button {
    id: control

    property string variant: "secondary"
    property string iconName: ""

    readonly property var tones: ({
        primary: {
            fill: Theme.accent,
            hoverFill: Theme.accentHover,
            pressedFill: Theme.accentPressed,
            focusFill: Theme.accent,
            disabledFill: Theme.surfaceHigh,
            content: Theme.textOnAccent,
            hoverContent: Theme.textOnAccent,
            border: "transparent",
            pressedBorder: "transparent",
            disabledBorder: "transparent"
        },
        secondary: {
            fill: Theme.surfaceHigh,
            hoverFill: Theme.surfaceHover,
            pressedFill: Theme.surfaceHover,
            focusFill: Theme.surfaceHigh,
            disabledFill: Theme.surfaceRaised,
            content: Theme.text,
            hoverContent: Theme.text,
            border: Theme.borderStrong,
            pressedBorder: Theme.accentPressed,
            disabledBorder: Theme.border
        },
        ghost: {
            fill: "transparent",
            hoverFill: Theme.surfaceHigh,
            pressedFill: Theme.surfaceHover,
            focusFill: Theme.surfaceHigh,
            disabledFill: "transparent",
            content: Theme.text,
            hoverContent: Theme.text,
            border: "transparent",
            pressedBorder: "transparent",
            disabledBorder: "transparent"
        },
        danger: {
            fill: Theme.dangerSubtle,
            hoverFill: Theme.danger,
            pressedFill: Theme.danger,
            focusFill: Theme.dangerSubtle,
            disabledFill: Theme.surfaceRaised,
            content: Theme.danger,
            hoverContent: Theme.textOnDanger,
            border: "transparent",
            pressedBorder: "transparent",
            disabledBorder: "transparent"
        }
    })
    readonly property var tone: tones[variant]

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Theme.control
    leftPadding: Theme.space5
    rightPadding: Theme.space5
    spacing: Theme.space2
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true

    contentItem: Item {
        implicitWidth: row.implicitWidth
        implicitHeight: row.implicitHeight

        Row {
            id: row

            anchors.centerIn: parent
            spacing: control.spacing

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                visible: control.iconName.length > 0
                name: control.iconName
                color: label.color
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

        implicitHeight: Theme.control
        radius: Theme.radiusMd
        color: control.tone.fill
        border.width: control.variant === "secondary" ? 1 : 0
        border.color: control.tone.border

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
                border.color: control.tone.disabledBorder
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
                border.color: control.tone.pressedBorder
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
            when: control.visualFocus

            PropertyChanges {
                target: fill
                color: control.tone.focusFill
            }
        }
    ]
}
