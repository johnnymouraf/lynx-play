import QtQuick
import QtQuick.Layouts
import MFPlay

// The design's AppRail: the brand, the app's sections and Sobre at the
// bottom.
Rectangle {
    id: rail

    // "consoles", "settings" or "about".
    property string section: "consoles"

    signal sectionRequested(string section)

    implicitWidth: Theme.sizeRail
    color: Theme.menu

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 28
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        anchors.bottomMargin: Theme.space4
        spacing: 0

        BrandLogo {
            Layout.leftMargin: Theme.space2
        }

        AppText {
            Layout.topMargin: Theme.space6
            Layout.leftMargin: 10
            text: "Menu"
            textStyle: "overline"
            color: Theme.textSubtle
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space2
            spacing: 6

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

        NavItem {
            Layout.fillWidth: true
            text: "Sobre"
            iconName: "info"
            selected: rail.section === "about"
            onClicked: rail.sectionRequested("about")
        }
    }
}
