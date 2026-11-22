import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// Configurações: its sections in a column of their own, the chosen one beside.
Item {
    id: page

    required property AppController controller
    required property SettingsController settings

    property string section: "video"

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.preferredWidth: 232
            Layout.fillHeight: true
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: Theme.space8
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                anchors.bottomMargin: Theme.space8
                spacing: Theme.space1

                AppText {
                    Layout.leftMargin: Theme.space1
                    text: "Configurações"
                    textStyle: "titleMedium"
                    Accessible.role: Accessible.Heading
                }

                NavItem {
                    Layout.fillWidth: true
                    text: "Vídeo"
                    iconName: "monitor"
                    selected: page.section === "video"
                    onClicked: page.section = "video"
                }

                NavItem {
                    Layout.fillWidth: true
                    text: "Sobre"
                    iconName: "info"
                    selected: page.section === "about"
                    onClicked: page.section = "about"
                }

                Item {
                    Layout.fillHeight: true
                }
            }

            Rectangle {
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                width: 1
                color: Theme.border
            }
        }

        Flickable {
            id: scroller

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: sections.children[sections.currentIndex].implicitHeight + 2 * Theme.space8
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            T.ScrollBar.vertical: ThinScrollBar {}

            StackLayout {
                id: sections

                x: Theme.space10
                y: Theme.space8
                width: scroller.width - 2 * Theme.space10
                currentIndex: page.section === "video" ? 0 : 1

                VideoSettings {
                    settings: page.settings
                }

                AboutSettings {
                    controller: page.controller
                }
            }
        }
    }
}
