import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// Configurações › Controles: the controller in use and its battery, which one
// to prefer and the keyboard/mouse mapping.
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
    readonly property string cardDetail: {
        if (!section.input.controllerConnected) {
            return "Conecte um DualSense por USB ou Bluetooth. Sem controle, o teclado e o mouse fazem o papel dele.";
        }

        if (section.input.controllerWired) {
            return "Vibração fina e gatilhos adaptáveis completos.";
        }

        return "Por Bluetooth, a vibração pode ficar mais simples. Conecte por USB para vibração fina e gatilhos adaptáveis completos.";
    }
    readonly property int batteryPercent: section.input.controllerBatteryPercent
    readonly property int batteryCells: section.batteryPercent >= 0
                                       ? Math.min(5, Math.ceil(section.batteryPercent / 20)) : 0
    readonly property color batteryColor: section.input.controllerBatteryLow
                                          ? Theme.standby : Theme.ready
    readonly property string batteryDetail: {
        if (!section.input.controllerConnected) {
            return "";
        }

        if (section.input.controllerCharging) {
            return "Carregando";
        }

        if (section.input.controllerWired) {
            return "Alimentado por USB";
        }

        return section.batteryPercent >= 0 ? "Não está carregando" : "Sem leitura de bateria";
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
        radius: Theme.radiusMd
        color: Theme.surface

        RowLayout {
            id: card

            anchors.fill: parent
            anchors.margins: Theme.space4
            spacing: Theme.space4

            Rectangle {
                Layout.alignment: Qt.AlignTop
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
                Layout.alignment: Qt.AlignTop
                spacing: 2

                AppText {
                    Layout.fillWidth: true
                    text: section.cardTitle
                    textStyle: "labelLarge"
                    wrapMode: Text.WordWrap
                }

                AppText {
                    Layout.fillWidth: true
                    text: section.cardDetail
                    textStyle: "bodyMedium"
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: Theme.space2
                    implicitHeight: battery.implicitHeight + 2 * 14
                    visible: section.input.controllerConnected
                    radius: Theme.radiusMd
                    color: Theme.surfaceRaised

                    ColumnLayout {
                        id: battery

                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: Theme.space2

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Theme.space2

                            AppText {
                                text: "Bateria"
                                textStyle: "overline"
                                color: Theme.textSubtle
                            }

                            Item {
                                Layout.fillWidth: true
                            }

                            AppText {
                                text: section.batteryPercent >= 0
                                      ? `${section.batteryPercent}%` : "—"
                                font.family: Theme.fontDisplay
                                font.pixelSize: 20
                                font.weight: Font.Bold
                                lineHeight: 22
                            }
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 3

                            Repeater {
                                model: 5

                                Rectangle {
                                    required property int index

                                    Layout.fillWidth: true
                                    implicitHeight: 8
                                    radius: 2
                                    color: index < section.batteryCells ? section.batteryColor
                                                                       : Theme.surfaceHover
                                }
                            }
                        }

                        AppText {
                            Layout.fillWidth: true
                            text: section.batteryDetail
                            textStyle: "detail"
                            color: Theme.textMuted
                        }
                    }
                }
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

    SettingRow {
        Layout.fillWidth: true
        label: "Teclado como controle"
        description: "As teclas e o mouse jogam pelo controle virtual."

        Toggle {
            checked: section.settings.keyboardMouseEnabled
            Accessible.name: "Teclado como controle"
            onCheckedChanged: {
                if (checked !== section.settings.keyboardMouseEnabled) {
                    section.settings.keyboardMouseEnabled = checked;
                }
            }
        }
    }

    Rectangle {
        id: table

        Layout.fillWidth: true
        implicitHeight: tableContent.implicitHeight
        radius: Theme.radiusMd
        color: Theme.surface
        clip: true
        enabled: section.settings.keyboardMouseEnabled
        opacity: enabled ? 1 : 0.5

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
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.96
                        font.capitalization: Font.AllUppercase
                        lineHeight: 16
                        color: Theme.textSubtle
                    }

                    AppText {
                        Layout.fillWidth: true
                        text: "Tecla ou mouse"
                        font.family: Theme.fontBody
                        font.pixelSize: 12
                        font.weight: Font.DemiBold
                        font.letterSpacing: 0.96
                        font.capitalization: Font.AllUppercase
                        lineHeight: 16
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

                    RowLayout {
                        anchors.fill: parent
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
        enabled: section.settings.keyboardMouseEnabled
        opacity: enabled ? 1 : 0.5

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
        enabled: section.settings.keyboardMouseEnabled
        opacity: enabled ? 1 : 0.5

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
