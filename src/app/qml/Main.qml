import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import MFPlay

ApplicationWindow {
    id: window

    required property AppController controller
    required property ConsolesController consoles
    required property SettingsController settings
    required property StreamController stream
    required property InputController input
    // Set by the `--gallery` developer option.
    property bool gallery: false

    property string section: "consoles"
    // The sections the StackLayout below holds, in order.
    readonly property var sectionIds: ["consoles", "settings", "about"]
    // The side margins of the pages and the footer: the design's 48 px, or
    // 32 px in a window narrower than 1280.
    readonly property int pageMargin: window.width >= 1280 ? Theme.space12 : Theme.space8
    readonly property var hintsBySection: ({
        consoles: [
            { button: "cross", label: "Jogar" },
            { button: "triangle", label: "Opções do console" },
            { button: "square", label: "Adicionar pelo IP" },
            { button: "options", label: "Menu" }
        ],
        settings: window.navigationHints,
        about: window.navigationHints
    })
    readonly property var navigationHints: [
        { button: "cross", label: "Selecionar" },
        { button: "circle", label: "Voltar" },
        { button: "options", label: "Menu" }
    ]

    readonly property bool connecting: window.stream.phase !== StreamController.Idle
                                        && window.stream.phase !== StreamController.Streaming

    width: 1280
    height: 800
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "MFPlay"
    color: Theme.bg
    font.family: Theme.fontBody
    font.pixelSize: 14

    function controllerStatusText() {
        if (!window.input.controllerConnected) {
            return "Desconectado";
        }

        return window.input.controllerBatteryLow ? "Bateria baixa" : "Conectado";
    }

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

            ColumnLayout {
                id: contentArea

                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Theme.space5

                StackLayout {
                    id: sections

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: window.sectionIds.indexOf(window.section)

                    ConsolesPage {
                        id: consolesPage

                        consoles: window.consoles
                        settings: window.settings
                        sideMargin: window.pageMargin
                        onToastRequested: (tone, title, message) => toast.show(tone, title, message)
                    }

                    SettingsPage {
                        id: settingsPage

                        controller: window.controller
                        consoles: window.consoles
                        settings: window.settings
                        input: window.input
                        sideMargin: window.pageMargin
                    }

                    AboutPage {
                        controller: window.controller
                        sideMargin: window.pageMargin
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: window.section === "settings"
                                       ? 240 + Theme.space4 : window.pageMargin
                    Layout.rightMargin: window.pageMargin
                    Layout.bottomMargin: Theme.space4
                    spacing: Theme.space4

                    ControllerHintBar {
                        Layout.fillWidth: true
                        opacity: window.input.controllerConnected ? 1 : 0
                        hints: window.hintsBySection[window.section]
                    }

                    ControllerStatus {
                        Layout.preferredWidth: 240
                        visible: window.section !== "settings"
                                 || (settingsPage.section !== "controls"
                                     && window.width >= 1180)
                        connected: window.input.controllerConnected
                        name: window.input.controllerTitle
                        status: window.controllerStatusText()
                        connection: window.input.controllerWired ? "via USB" : "via Bluetooth"
                        batteryLow: window.input.controllerBatteryLow
                    }
                }
            }
        }

        ConnectingPage {
            anchors.fill: parent
            visible: window.connecting
            stream: window.stream
            controller: window.controller
        }

        Loader {
            anchors.fill: parent
            active: window.gallery

            sourceComponent: Gallery {}
        }
    }

    Toast {
        id: toast

        x: contentArea.x + (contentArea.width - width) / 2
        y: window.height - height - Theme.sizeControlLg - Theme.space4 - Theme.space5
        width: Math.min(400, contentArea.width - 2 * Theme.space8)
    }
}
