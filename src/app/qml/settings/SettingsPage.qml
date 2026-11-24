import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// Configurações: its sections in a column of their own, the chosen one beside.
Item {
    id: page

    required property AppController controller
    required property ConsolesController consoles
    required property SettingsController settings
    required property InputController input
    // The page's right margin, set by the shell for the window's width.
    property int sideMargin: Theme.space12

    property string section: "video"
    // The sections the StackLayout below holds, in order.
    readonly property var sectionIds: ["video", "audio", "controls", "rtx", "network"]

    onSectionChanged: scroller.contentY = 0

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.preferredWidth: 240
            Layout.fillHeight: true

            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 26
                anchors.leftMargin: Theme.space4
                anchors.bottomMargin: Theme.space4
                radius: Theme.radiusMd
                color: Theme.surface

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space2
                    spacing: 6

                    AppText {
                        Layout.leftMargin: Theme.space2
                        Layout.topMargin: Theme.space2
                        Layout.bottomMargin: 9
                        text: "Configurações"
                        textStyle: "overline"
                        color: Theme.textSubtle
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
                        text: "Rede"
                        iconName: "wifi"
                        selected: page.section === "network"
                        onClicked: page.section = "network"
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }

        Flickable {
            id: scroller

            Layout.fillWidth: true
            Layout.fillHeight: true
            contentWidth: width
            contentHeight: sections.children[sections.currentIndex].implicitHeight + 42
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            T.ScrollBar.vertical: ThinScrollBar {}

            StackLayout {
                id: sections

                x: Theme.space4
                y: 26
                width: scroller.width - Theme.space4 - page.sideMargin
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

                NetworkSettings {
                    settings: page.settings
                    consoles: page.consoles
                }
            }
        }
    }
}
