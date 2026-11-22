import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// Configurações: its sections in a column of their own, the chosen one beside.
Item {
    id: page

    required property AppController controller
    required property SettingsController settings
    required property InputController input

    property string section: "video"
    // The sections the StackLayout below holds, in order.
    readonly property var sectionIds: ["video", "audio", "controls", "rtx", "about"]

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
                    text: "Áudio e microfone"
                    iconName: "volume-2"
                    selected: page.section === "audio"
                    onClicked: page.section = "audio"
                }

                NavItem {
                    Layout.fillWidth: true
                    text: "Controles"
                    iconName: "gamepad-2"
                    selected: page.section === "controls"
                    onClicked: page.section = "controls"
                }

                NavItem {
                    Layout.fillWidth: true
                    text: "RTX"
                    iconName: "sparkles"
                    selected: page.section === "rtx"
                    onClicked: page.section = "rtx"
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
            contentHeight: sections.children[sections.currentIndex].implicitHeight
                           + 2 * Theme.space8
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            T.ScrollBar.vertical: ThinScrollBar {}

            StackLayout {
                id: sections

                x: Theme.space10
                y: Theme.space8
                width: scroller.width - 2 * Theme.space10
                currentIndex: Math.max(0, page.sectionIds.indexOf(page.section))

                VideoSettings {
                    settings: page.settings
                }

                AudioSettings {
                    settings: page.settings
                }

                ControllerSettings {
                    settings: page.settings
                    input: page.input
                }

                RtxSettings {
                    settings: page.settings
                    gpuName: page.controller.gpuName
                }

                AboutSettings {
                    controller: page.controller
                }
            }
        }
    }
}
