import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// Configurações › Controles: the controller in use, which one to prefer and
// the keyboard/mouse mapping.
ColumnLayout {
    id: section

    required property SettingsController settings
    required property InputController input

    property string capturing
    property bool mouseAssigned

    readonly property string connectionWord: section.input.controllerWired ? "USB" : "Bluetooth"
    readonly property string cardTitle: {
        if (!section.input.controllerConnected) {
            return "Nenhum controle conectado";
        }

        if (section.input.controllers.length > 1) {
            return `${section.input.controllers.length} controles conectados`;
        }

        return `${section.input.controllerTitle} conectado por ${section.connectionWord}`;
    }
    readonly property var controllerOptions: {
        const options = [{ label: "Primeiro conectado", value: "" }];

        for (let i = 0; i < section.input.controllers.length; ++i) {
            options.push({
                label: section.input.controllers[i].label,
                value: section.input.controllers[i].name
            });
        }

        return options;
    }
    readonly property var keyboardRows: [
        {
            label: "Analógico esquerdo",
            controls: ["leftStickUp", "leftStickDown", "leftStickLeft", "leftStickRight"]
        },
        { label: "Analógico direito", controls: ["rightStick"] },
        { label: "Cruz · Círculo", controls: ["cross", "circle"] },
        { label: "Quadrado · Triângulo", controls: ["square", "triangle"] },
        { label: "L1 · R1", controls: ["l1", "r1"] },
        { label: "L2 · R2", controls: ["l2", "r2"] },
        { label: "Options · Create", controls: ["options", "create"] }
    ]

    function bindingFor(control) {
        const binding = section.settings.keyboardBindings[control];

        return binding === undefined ? "" : binding;
    }

    function keyLabel(control) {
        const binding = section.bindingFor(control);

        return binding.length > 0 ? section.settings.bindingLabel(binding) : "—";
    }

    function beginCapture(control) {
        if (section.mouseAssigned) {
            section.mouseAssigned = false;
            return;
        }

        if (section.capturing === control && control !== "rightStick") {
            section.settings.setKeyboardBinding(control, "MouseLeft");
            section.capturing = "";
            return;
        }

        section.capturing = control;
        captureItem.forceActiveFocus();
    }

    function assignMouse(button) {
        if (section.capturing.length === 0) {
            return;
        }

        let binding = "";

        if (button === Qt.LeftButton) {
            binding = "MouseLeft";
        } else if (button === Qt.RightButton) {
            binding = "MouseRight";
        } else if (button === Qt.MiddleButton) {
            binding = "MouseMiddle";
        }

        if (binding.length > 0) {
            section.settings.setKeyboardBinding(section.capturing, binding);
            section.capturing = "";
            section.mouseAssigned = true;
        }
    }

    spacing: Theme.space2

    SectionHeader {
        Layout.fillWidth: true
        title: "Controles"
        subtitle: "O DualSense funciona direto. Sem controle, o teclado e o mouse fazem o papel dele."
    }

    Rectangle {
        Layout.fillWidth: true
        implicitHeight: card.implicitHeight + 2 * Theme.space4
        radius: Theme.radiusLg
        color: Theme.surface
        border.width: 1
        border.color: Theme.border

        RowLayout {
            id: card

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.space4
            anchors.rightMargin: Theme.space4
            spacing: Theme.space4

            Rectangle {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 48
                implicitHeight: 48
                radius: Theme.radiusLg
                color: Theme.surfaceHigh

                Icon {
                    anchors.centerIn: parent
                    name: "gamepad-2"
                    size: 26
                    color: Theme.text
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                AppText {
                    Layout.fillWidth: true
                    text: section.cardTitle
                    textStyle: "labelLarge"
                }

                AppText {
                    Layout.fillWidth: true
                    text: section.input.controllerConnected
                          ? "Vibração fina e gatilhos adaptáveis completos. Por Bluetooth, a vibração pode ficar mais simples."
                          : "Conecte um DualSense por USB ou Bluetooth. Sem controle, o teclado e o mouse fazem o papel dele."
                    textStyle: "bodyMedium"
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }

            StateBadge {
                Layout.alignment: Qt.AlignVCenter
                status: section.input.controllerConnected ? "ready" : "offline"
                label: section.input.controllerConnected ? section.input.controllerStatus
                                                         : "Desconectado"
            }
        }
    }

    SettingRow {
        Layout.fillWidth: true
        visible: section.input.controllers.length > 1
        label: "Controle"
        description: "O primeiro conectado é usado quando não há uma escolha."

        Select {
            name: "Controle"
            options: section.controllerOptions
            currentValue: section.settings.preferredController
            onActivated: value => section.settings.preferredController = value
        }
    }

    AppText {
        Layout.fillWidth: true
        Layout.topMargin: Theme.space3
        text: "Teclado e mouse"
        textStyle: "titleSmall"
    }

    Rectangle {
        id: table

        Layout.fillWidth: true
        implicitHeight: tableContent.implicitHeight
        radius: Theme.radiusMd
        color: "transparent"
        border.width: 1
        border.color: Theme.border
        clip: true

        readonly property int labelColumnWidth: Math.min(260, table.width * 0.4)

        Column {
            id: tableContent

            width: parent.width

            Rectangle {
                width: parent.width
                height: 36
                color: Theme.surface

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space4
                    anchors.rightMargin: Theme.space4
                    spacing: Theme.space4

                    AppText {
                        Layout.preferredWidth: table.labelColumnWidth
                        text: "Botão do controle"
                        textStyle: "overline"
                        color: Theme.textSubtle
                    }

                    AppText {
                        Layout.fillWidth: true
                        text: "Tecla ou mouse"
                        textStyle: "overline"
                        color: Theme.textSubtle
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: Theme.border
                }
            }

            Repeater {
                model: section.keyboardRows

                Rectangle {
                    required property var modelData

                    width: tableContent.width
                    height: Math.max(44, keyFlow.implicitHeight + 2 * Theme.space1)
                    color: "transparent"

                    RowLayout {                        anchors.fill: parent
                        anchors.leftMargin: Theme.space4
                        anchors.rightMargin: Theme.space4
                        spacing: Theme.space4

                        AppText {
                            Layout.preferredWidth: table.labelColumnWidth
                            text: modelData.label
                            textStyle: "bodyMedium"
                        }

                        Flow {
                            id: keyFlow

                            Layout.fillWidth: true
                            spacing: Theme.space1

                            Repeater {
                                model: modelData.controls

                                Keycap {
                                    required property string modelData

                                    editable: modelData !== "rightStick"
                                    capturing: section.capturing === modelData
                                    text: section.keyLabel(modelData)
                                    onActivated: section.beginCapture(modelData)
                                    onMouseButton: button => section.assignMouse(button)
                                }
                            }
                        }
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Theme.border
                    }
                }
            }
        }
    }

    AppText {
        Layout.fillWidth: true
        visible: section.capturing.length > 0
        text: "Pressione uma tecla ou um botão do mouse. Esc cancela."
        textStyle: "bodyMedium"
        color: Theme.accent
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Sensibilidade do mouse"
        description: "Quanto o mouse move o analógico direito."

        Row {
            spacing: Theme.space3

            ValueSlider {
                anchors.verticalCenter: parent.verticalCenter
                from: 10
                to: 500
                stepSize: 10
                snapMode: T.Slider.SnapAlways
                value: section.settings.mouseSensitivityPercent
                Accessible.name: "Sensibilidade do mouse"
                onMoved: section.settings.mouseSensitivityPercent = Math.round(value)
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                width: 64
                text: `${section.settings.mouseSensitivityPercent} %`
                textStyle: "monoMedium"
                horizontalAlignment: Text.AlignRight
            }
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Inverter eixo Y"
        description: "Para quem prefere o mouse com a vertical invertida."

        Toggle {
            checked: section.settings.invertMouseY
            Accessible.name: "Inverter eixo Y"
            onCheckedChanged: {
                if (checked !== section.settings.invertMouseY) {
                    section.settings.invertMouseY = checked;
                }
            }
        }
    }

    Item {
        id: captureItem

        width: 0
        height: 0
        focus: section.capturing.length > 0
        Keys.enabled: focus
        Keys.onPressed: event => {
            if (event.key === Qt.Key_Escape) {
                section.capturing = "";
                event.accepted = true;
                return;
            }

            const binding = section.settings.bindingNameForKey(event.key);

            if (binding.length > 0) {
                section.settings.setKeyboardBinding(section.capturing, binding);
                section.capturing = "";
            }

            event.accepted = true;
        }
    }
}
