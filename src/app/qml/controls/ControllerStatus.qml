import QtQuick
import QtQuick.Layouts
import MFPlay

// The design's ControllerStatus: the active controller, its state and how it
// is connected; without one, it says so.
Rectangle {
    id: card

    property bool connected: false
    property string name
    property string status
    property string connection
    property bool batteryLow: false

    readonly property color statusColor: card.batteryLow ? Theme.standby : Theme.ready

    implicitWidth: 240
    implicitHeight: Theme.sizeControlLg
    radius: Theme.radiusMd
    color: Theme.surface
    Accessible.role: Accessible.StaticText
    Accessible.name: card.connected
                     ? `${card.name}, ${card.status} ${card.connection}`
                     : "Nenhum controle conectado"

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 14
        anchors.rightMargin: Theme.space3
        spacing: 14

        IconTile {
            iconName: "gamepad-2"
            iconSize: 18
            iconColor: card.connected ? Theme.text : Theme.textSubtle
            color: Theme.surfaceRaised
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            AppText {
                Layout.fillWidth: true
                text: card.connected ? card.name : "Nenhum controle"
                textStyle: "labelSmall"
            }

            Row {
                Layout.fillWidth: true
                visible: card.connected
                spacing: Theme.space1

                AppText {
                    text: card.status
                    textStyle: "detail"
                    color: card.statusColor
                }

                AppText {
                    text: card.connection
                    textStyle: "detail"
                    color: Theme.textSubtle
                }
            }

            AppText {
                Layout.fillWidth: true
                visible: !card.connected
                text: "Conecte um DualSense"
                textStyle: "detail"
                color: Theme.textSubtle
            }
        }
    }
}
