import QtQuick
import QtQuick.Layouts
import LynxPlay

// Configurações › RTX: the NVIDIA features applied to the stream.
ColumnLayout {
    id: section

    required property SettingsController settings
    property string gpuName

    spacing: Theme.space2

    SectionHeader {
        Layout.fillWidth: true
        title: "RTX"
        subtitle: section.gpuName.length > 0
                  ? "Recursos da sua GPU NVIDIA aplicados ao vídeo do PS5. GPU: "
                    + section.gpuName + "."
                  : "Recursos da sua GPU NVIDIA aplicados ao vídeo do PS5."
    }

    SettingRow {
        Layout.fillWidth: true
        label: "NVIDIA Reflex"
        description: section.settings.reflexAvailable
                     ? "Reduz a fila da GPU para baixar a latência. Boost mantém o clock alto."
                     : "Indisponível: " + section.settings.reflexUnavailableReason

        SegmentedControl {
            name: "NVIDIA Reflex"
            enabled: section.settings.reflexAvailable
            opacity: section.settings.reflexAvailable ? 1 : 0.5
            options: [
                { label: "Desligado", value: "off" },
                { label: "Ligado", value: "on" },
                { label: "Ligado + Boost", value: "boost" }
            ]
            currentValue: section.settings.reflex
            onActivated: value => section.settings.reflex = value
        }
    }
}
