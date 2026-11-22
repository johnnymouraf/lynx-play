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
        label: "RTX Video Super Resolution"
        description: section.settings.videoSuperResolutionAvailable
                     ? "Remove artefatos de compressão e amplia a imagem para a resolução da "
                       + "tela. Também requer Super Resolution ligado no NVIDIA App "
                       + "(Sistema › Vídeo › RTX Video Enhancements)."
                     : "Indisponível: " + section.settings.videoSuperResolutionUnavailableReason

        Toggle {
            checked: section.settings.videoSuperResolution
            enabled: section.settings.videoSuperResolutionAvailable
            opacity: enabled ? 1 : 0.5
            Accessible.name: "RTX Video Super Resolution"
            onCheckedChanged: {
                if (checked !== section.settings.videoSuperResolution) {
                    section.settings.videoSuperResolution = checked;
                }
            }
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Optical Flow"
        description: section.settings.opticalFlowAvailable
                     ? "Calcula os vetores de movimento entre os quadros. Identifica os quadros "
                       + "repetidos dos jogos de 30 fps e prepara os vetores para o frame "
                       + "generation."
                     : "Indisponível: " + section.settings.opticalFlowUnavailableReason

        Toggle {
            checked: section.settings.opticalFlow
            enabled: section.settings.opticalFlowAvailable
            opacity: enabled ? 1 : 0.5
            Accessible.name: "Optical Flow"
            onCheckedChanged: {
                if (checked !== section.settings.opticalFlow) {
                    section.settings.opticalFlow = checked;
                }
            }
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "DLSS Frame Generation 2x"
        description: section.settings.frameGenerationAvailable
                     ? "Gera um quadro entre cada dois: 30 → 60 ou 60 → 120 fps. Adiciona cerca "
                       + "de 1 quadro de latência; usa o NVIDIA Reflex."
                     : "Indisponível: " + section.settings.frameGenerationUnavailableReason

        SegmentedControl {
            name: "DLSS Frame Generation 2x"
            enabled: section.settings.frameGenerationAvailable
            opacity: enabled ? 1 : 0.5
            options: [
                { label: "Desligado", value: "off" },
                { label: "Ligado 2x", value: "2x" }
            ]
            currentValue: section.settings.frameGeneration
            onActivated: value => section.settings.frameGeneration = value
        }
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
