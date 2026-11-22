import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// Configurações › Áudio e microfone: where the game's sound plays, what the
// console hears and how loud the game is.
ColumnLayout {
    id: section

    required property SettingsController settings

    function withDefault(names) {
        const options = [{ label: "Padrão do Windows", value: "" }];

        for (let i = 0; i < names.length; ++i) {
            options.push({ label: names[i], value: names[i] });
        }

        return options;
    }

    spacing: Theme.space2

    Connections {
        target: section.settings

        function onAudioChanged() {
            muteToggle.checked = section.settings.microphoneMuted;
        }
    }

    SectionHeader {
        Layout.fillWidth: true
        title: "Áudio e microfone"
        subtitle: "O som do jogo sai no PC; o microfone vai para o PS5 (chat de voz e comandos)."
    }

    Banner {
        Layout.fillWidth: true
        visible: section.settings.outputDevices.length === 0
        tone: "standby"
        title: "Nenhuma saída de áudio encontrada"
        message: "O som tenta a saída padrão do Windows. Confira os dispositivos no Windows."
    }

    Banner {
        Layout.fillWidth: true
        visible: section.settings.microphoneDevices.length === 0
        tone: "standby"
        title: "Nenhum microfone encontrado"
        message: "Verifique Configurações do Windows › Privacidade e segurança › Microfone, e se o microfone está conectado."
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Saída de áudio"
        description: "Onde o som do jogo toca."

        Select {
            name: "Saída de áudio"
            options: section.withDefault(section.settings.outputDevices)
            currentValue: section.settings.outputDevice
            onActivated: value => section.settings.outputDevice = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Microfone"
        description: "Usado no chat de voz do PS5."

        Select {
            name: "Microfone"
            options: section.withDefault(section.settings.microphoneDevices)
            currentValue: section.settings.microphoneDevice
            onActivated: value => section.settings.microphoneDevice = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Silenciar microfone"
        description: "Também pelo botão de mudo do DualSense, a qualquer momento."

        Toggle {
            id: muteToggle

            checked: section.settings.microphoneMuted
            Accessible.name: "Silenciar microfone"
            onToggled: section.settings.microphoneMuted = checked
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Volume do jogo"

        Row {
            spacing: Theme.space3

            ValueSlider {
                anchors.verticalCenter: parent.verticalCenter
                from: 0
                to: 100
                stepSize: 1
                snapMode: T.Slider.SnapAlways
                value: section.settings.volumePercent
                Accessible.name: "Volume do jogo"
                onMoved: section.settings.volumePercent = Math.round(value)
            }

            AppText {
                anchors.verticalCenter: parent.verticalCenter
                width: 76
                text: `${section.settings.volumePercent} %`
                textStyle: "monoMedium"
                horizontalAlignment: Text.AlignRight
            }
        }
    }
}
