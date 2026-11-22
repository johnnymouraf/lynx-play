import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LynxPlay

ApplicationWindow {
    id: window

    required property AppController controller

    property string section: "consoles"

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Lynx Play"
    color: Theme.bg
    font.family: Theme.fontBody
    font.pixelSize: 14

    Component.onCompleted: consolesPage.focusFirstControl()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        AppRail {
            Layout.fillHeight: true
            section: window.section
            onSectionRequested: requested => window.section = requested
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: window.section === "consoles" ? 0 : 1

            ConsolesPage {
                id: consolesPage
            }

            SettingsPage {
                controller: window.controller
            }
        }
    }
}
