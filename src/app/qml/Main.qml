import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LynxPlay

ApplicationWindow {
    id: window

    required property AppController controller
    required property ConsolesController consoles
    required property SettingsController settings
    required property StreamController stream

    property string section: "consoles"

    readonly property bool connecting: window.stream.phase !== StreamController.Idle
                                        && window.stream.phase !== StreamController.Streaming

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

    Connections {
        target: window.consoles

        function onNotice(tone, title, message) {
            toast.show(tone, title, message);
        }
    }

    Connections {
        target: window.settings

        function onNotice(tone, title, message) {
            toast.show(tone, title, message);
        }
    }

    Connections {
        target: window.stream

        function onNotice(tone, title, message) {
            toast.show(tone, title, message);
        }

        function onChanged() {
            if (window.stream.phase === StreamController.Idle) {
                consolesPage.focusFirstControl();
            }
        }
    }

    Item {
        anchors.fill: parent

        RowLayout {
            anchors.fill: parent
            spacing: 0

            AppRail {
                Layout.fillHeight: true
                section: window.section
                onSectionRequested: requested => window.section = requested
            }

            StackLayout {
                id: sections

                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: window.section === "consoles" ? 0 : 1

                ConsolesPage {
                    id: consolesPage

                    consoles: window.consoles
                    onToastRequested: (tone, title, message) => toast.show(tone, title, message)
                }

                SettingsPage {
                    controller: window.controller
                    settings: window.settings
                }
            }
        }

        ConnectingPage {
            anchors.fill: parent
            visible: window.connecting
            stream: window.stream
            controller: window.controller
        }
    }

    Toast {
        id: toast

        x: sections.x + (sections.width - width) / 2
        y: window.height - height - Theme.space6
        width: Math.min(400, sections.width - 2 * Theme.space8)
    }
}
