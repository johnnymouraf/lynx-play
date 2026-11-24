import QtQuick
import QtQuick.Layouts
import MFPlay

// Configurações › Vídeo: what the PS5 is asked to stream and how it is shown.
ColumnLayout {
    id: section

    required property SettingsController settings

    spacing: Theme.space2

    SectionHeader {
        Layout.fillWidth: true
        title: "Vídeo"
        subtitle: "Qualidade da imagem pedida ao PS5. Valores mais altos exigem uma rede mais estável."
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Modo da janela"
        description: "Tela cheia sem bordas no monitor do MFPlay, ou uma janela ajustável. F11 alterna durante a transmissão."

        SegmentedControl {
            name: "Modo da janela"
            options: [
                { label: "Tela cheia", value: "fullscreen" },
                { label: "Janela", value: "windowed" }
            ]
            currentValue: section.settings.windowMode
            onActivated: value => section.settings.windowMode = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Tamanho da janela"
        description: section.settings.windowMode === "fullscreen"
            ? `Tela cheia. O último tamanho de janela foi ${section.settings.windowSizeText}.`
            : `Tamanho atual: ${section.settings.windowSizeText}. Arraste a borda da janela para ajustar.`

        Select {
            width: 220
            enabled: section.settings.windowMode === "windowed"
            opacity: enabled ? 1 : 0.5
            name: "Tamanho da janela"
            options: {
                const sizes = [
                    { label: "1920 × 1080", value: "1920x1080" },
                    { label: "1600 × 900", value: "1600x900" },
                    { label: "1280 × 720", value: "1280x720" },
                    { label: "960 × 540", value: "960x540" }
                ];
                if (section.settings.windowSizePreset === "custom") {
                    sizes.push({ label: "Personalizado", value: "custom" });
                }
                return sizes;
            }
            currentValue: section.settings.windowSizePreset
            onActivated: value => section.settings.windowSizePreset = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Resolução"
        description: "Resolução pedida ao PS5. A imagem é ampliada para a sua tela."

        SegmentedControl {
            name: "Resolução"
            options: [
                { label: "1080p", value: 1080 },
                { label: "720p", value: 720 },
                { label: "540p", value: 540 },
                { label: "360p", value: 360 }
            ]
            currentValue: section.settings.resolution
            onActivated: value => section.settings.resolution = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Taxa de quadros"

        SegmentedControl {
            name: "Taxa de quadros"
            options: [
                { label: "60 fps", value: 60 },
                { label: "30 fps", value: 30 }
            ]
            currentValue: section.settings.framesPerSecond
            onActivated: value => section.settings.framesPerSecond = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Codec"
        description: "H.265 entrega mais qualidade no mesmo bitrate."

        SegmentedControl {
            name: "Codec"
            options: [
                { label: "H.265", value: "h265" },
                { label: "H.264", value: "h264" }
            ]
            currentValue: section.settings.codec
            onActivated: value => section.settings.codec = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "Sincronização vertical"
        description: "Menor latência pode mostrar cortes na imagem (tearing)."

        SegmentedControl {
            name: "Sincronização vertical"
            options: [
                { label: "Menor latência", value: true },
                { label: "VSync", value: false }
            ]
            currentValue: section.settings.lowestLatency
            onActivated: value => section.settings.lowestLatency = value
        }
    }

    SettingRow {
        Layout.fillWidth: true
        label: "HDR"
        description: section.settings.hdrDescription

        SegmentedControl {
            name: "HDR"
            options: [
                { label: "Desligado", value: "off" },
                { label: "Automático", value: "auto" },
                {
                    label: "RTX Video HDR",
                    value: "rtx",
                    enabled: section.settings.hdrRtxAvailable
                }
            ]
            currentValue: section.settings.hdr
            onActivated: value => section.settings.hdr = value
        }
    }
}
