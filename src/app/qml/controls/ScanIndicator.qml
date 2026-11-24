import QtQuick
import MFPlay

// A pulsing dot with a label, shown while the app searches the network.
Row {
    id: indicator

    property string text: "Procurando consoles na rede…"

    leftPadding: Theme.space1
    rightPadding: Theme.space1
    spacing: Theme.space2
    Accessible.role: Accessible.StaticText
    Accessible.name: indicator.text

    Rectangle {
        anchors.verticalCenter: parent.verticalCenter
        width: 20
        height: 20
        radius: Theme.radiusFull
        color: Theme.accentSubtle
        border.width: 1
        border.color: Theme.accent

        Rectangle {
            id: core

            anchors.centerIn: parent
            width: 8
            height: 8
            radius: Theme.radiusFull
            color: Theme.accent

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: indicator.visible

                NumberAnimation {
                    to: 0.35
                    duration: 700
                    easing.type: Easing.InOutQuad
                }

                NumberAnimation {
                    to: 1
                    duration: 700
                    easing.type: Easing.InOutQuad
                }
            }
        }
    }

    AppText {
        anchors.verticalCenter: parent.verticalCenter
        text: indicator.text
        textStyle: "bodyMedium"
        color: Theme.textMuted
    }
}
