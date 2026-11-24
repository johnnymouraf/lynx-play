import QtQuick
import QtQuick.Layouts
import MFPlay

// The design's controller hints: the buttons and what they do on this
// screen. Shown only while a controller is detected.
Rectangle {
    id: bar

    property var hints: [
        { icon: "x", label: "Jogar" },
        { icon: "triangle", label: "Opções do console" },
        { icon: "square", label: "Adicionar pelo IP" },
        { icon: "menu", label: "Configurações" }
    ]

    implicitHeight: row.implicitHeight + 2 * Theme.space3
    color: Theme.bg
    Accessible.role: Accessible.StaticText
    Accessible.name: bar.hints.map(hint => hint.label).join(", ")

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Theme.border
    }

    RowLayout {
        id: row

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Theme.space8
        anchors.rightMargin: Theme.space8
        spacing: Theme.space6

        Repeater {
            model: bar.hints

            RowLayout {
                required property var modelData

                spacing: Theme.space2

                Rectangle {
                    implicitWidth: 26
                    implicitHeight: 26
                    radius: Theme.radiusSm
                    color: Theme.surfaceHigh
                    border.width: 1
                    border.color: Theme.borderStrong

                    Icon {
                        anchors.centerIn: parent
                        name: modelData.icon
                        size: 14
                        color: Theme.text
                    }
                }

                AppText {
                    text: modelData.label
                    textStyle: "bodyMedium"
                    color: Theme.textMuted
                }
            }
        }

        Item {
            Layout.fillWidth: true
        }
    }
}
