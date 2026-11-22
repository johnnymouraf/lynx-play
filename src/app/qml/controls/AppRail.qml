import QtQuick
import QtQuick.Layouts
import LynxPlay

// The left rail: brand and the app's sections.
Rectangle {
    id: rail

    property string section: "consoles"

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
    }
}
