import QtQuick
import QtQuick.Layouts
import LynxPlay

// Configurações › Controles: the controller in use and which one to prefer
// when more than one is connected.
ColumnLayout {
    id: section

    required property SettingsController settings
    required property InputController input

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
                    text: "Vibração fina e gatilhos adaptáveis completos. Por Bluetooth, a vibração pode ficar mais simples."
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
}
