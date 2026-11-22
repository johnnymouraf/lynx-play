import QtQuick
import QtQuick.Layouts
import LynxPlay

// The left rail: brand and the app's sections.
Rectangle {
    id: rail

    property string section: "consoles"
    property bool controllerConnected: false
    property string controllerTitle
    property string controllerDetail
    property bool controllerBatteryLow: false

    signal sectionRequested(string section)

    implicitWidth: Theme.railWidth
    color: Theme.surface

    Rectangle {
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: 1
        color: Theme.border
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: Theme.space6
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        anchors.bottomMargin: Theme.space5
        spacing: Theme.space8

        BrandLogo {
            Layout.leftMargin: Theme.space2
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Theme.space1

            NavItem {
                Layout.fillWidth: true
                text: "Consoles"
                iconName: "tv-minimal-play"
                selected: rail.section === "consoles"
                onClicked: rail.sectionRequested("consoles")
            }

            NavItem {
                Layout.fillWidth: true
                text: "Configurações"
                iconName: "settings"
                selected: rail.section === "settings"
                onClicked: rail.sectionRequested("settings")
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.fillWidth: true
            visible: rail.controllerConnected
            spacing: Theme.space3
            Accessible.role: Accessible.StaticText
            Accessible.name: `${rail.controllerTitle}, ${rail.controllerDetail}`

            Icon {
                Layout.alignment: Qt.AlignVCenter
                name: "gamepad-2"
                size: 20
                color: Theme.textMuted
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                AppText {
                    Layout.fillWidth: true
                    text: rail.controllerTitle
                    textStyle: "labelMedium"
                    elide: Text.ElideRight
                }

                AppText {
                    Layout.fillWidth: true
                    text: rail.controllerDetail
                    textStyle: "caption"
                    color: Theme.textMuted
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 8
                implicitHeight: 8
                radius: 4
                color: rail.controllerBatteryLow ? Theme.standby : Theme.ready
            }
        }
    }
}
