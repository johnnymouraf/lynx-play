import QtQuick
import QtQuick.Layouts
import LynxPlay

// Configurações › RTX: the NVIDIA features applied to the stream.
ColumnLayout {
    id: section

    required property SettingsController settings
    property string gpuName

    function upscalingDescription() {
        if (!section.settings.dlssAvailable) {
            return "DLSS Super Resolution indisponível: "
                    + section.settings.dlssUnavailableReason
                    + " O RTX VSR amplia a imagem.";
        }

        if (!section.settings.videoSuperResolutionAvailable) {
            return "RTX VSR indisponível: "
                    + section.settings.videoSuperResolutionUnavailableReason
                    + " O shader amplia a imagem.";
        }

        return "Como a imagem é ampliada para a resolução da tela. RTX Video Super "
                + "Resolution remove artefatos de compressão; DLSS Super Resolution é "
                + "experimental e usa os vetores do fluxo óptico.";
    }

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
        label: "Upscaling"
        description: section.upscalingDescription()

        SegmentedControl {
            name: "Upscaling"
            options: [
                { label: "Shader", value: "shader" },
                { label: "RTX VSR", value: "vsr",
                  enabled: section.settings.videoSuperResolutionAvailable },
                { label: "DLSS SR", value: "dlss", enabled: section.settings.dlssAvailable }
            ]
            currentValue: section.settings.upscaling
            onActivated: value => section.settings.upscaling = value
        }

        StateBadge {
            status: "waking"
            label: "Experimental"
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
