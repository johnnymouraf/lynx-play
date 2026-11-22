pragma Singleton

import QtQuick

// The design's "Theme" variables (design/lynx-play.drafia). Colors with alpha
// are written #AARRGGBB, as QML reads them.
QtObject {
    id: theme

    readonly property color bg: "#0A0D12"
    readonly property color surface: "#11161D"
    readonly property color surfaceRaised: "#171D26"
    readonly property color surfaceHigh: "#1F2631"
    readonly property color surfaceHover: "#283142"
    readonly property color border: "#273040"
    readonly property color borderStrong: "#3A4555"
    readonly property color borderControl: "#626D7F"
    readonly property color text: "#EDF0F4"
    readonly property color textMuted: "#A1AAB8"
    readonly property color textSubtle: "#828C9B"
    readonly property color textDisabled: "#545E6D"
    readonly property color accent: "#F5A23A"
    readonly property color accentHover: "#FFB65C"
    readonly property color accentPressed: "#DB8A24"
    readonly property color textOnAccent: "#1A1105"
    readonly property color accentSubtle: "#312A21"
    readonly property color focus: "#FFB65C"
    readonly property color ready: "#3DD68C"
    readonly property color readySubtle: "#17312D"
    readonly property color standby: "#E9C46A"
    readonly property color standbySubtle: "#2F2E28"
    readonly property color offline: "#8A94A3"
    readonly property color offlineSubtle: "#222830"
    readonly property color danger: "#F2625F"
    readonly property color dangerSubtle: "#312126"
    readonly property color textOnDanger: "#1C0707"
    readonly property color info: "#5EB0F2"
    readonly property color infoSubtle: "#1C2C3B"
    readonly property color scrim: "#CC05070A"
    readonly property color overlayPanel: "#D90A0D12"
    readonly property color overlayBorder: "#1FFFFFFF"
    readonly property color overlayControl: "#0FFFFFFF"

    // Not a variable in the design: the backdrop behind the console render.
    readonly property color mediaBackdrop: "#0B0F15"
    readonly property color dialogShadow: "#99000000"
    readonly property color toastShadow: "#80000000"

    readonly property int space1: 4
    readonly property int space2: 8
    readonly property int space3: 12
    readonly property int space4: 16
    readonly property int space5: 20
    readonly property int space6: 24
    readonly property int space8: 32
    readonly property int space10: 40
    readonly property int space12: 48
    readonly property int space16: 64

    readonly property int radiusSm: 6
    readonly property int radiusMd: 10
    readonly property int radiusLg: 14
    readonly property int radiusXl: 20
    readonly property int radiusFull: 999

    readonly property int controlSmall: 36
    readonly property int control: 44
    readonly property int controlLarge: 52
    readonly property int focusRingWidth: 2
    readonly property int iconSize: 20
    readonly property int railWidth: 248

    readonly property string fontDisplay: "Sora"
    readonly property string fontBody: "Inter"
    readonly property string fontMono: "JetBrains Mono"

    // The design's text styles, read by AppText through its textStyle name.
    readonly property var typography: ({
        display: {
            family: theme.fontDisplay, pixelSize: 36, weight: Font.DemiBold,
            lineHeight: 44, letterSpacing: -0.36
        },
        titleLarge: {
            family: theme.fontDisplay, pixelSize: 28, weight: Font.DemiBold,
            lineHeight: 36, letterSpacing: -0.28
        },
        titleMedium: {
            family: theme.fontDisplay, pixelSize: 22, weight: Font.DemiBold,
            lineHeight: 28, letterSpacing: 0
        },
        titleSmall: {
            family: theme.fontBody, pixelSize: 18, weight: Font.DemiBold,
            lineHeight: 24, letterSpacing: 0
        },
        bodyLarge: {
            family: theme.fontBody, pixelSize: 16, weight: Font.Normal,
            lineHeight: 24, letterSpacing: 0
        },
        bodyMedium: {
            family: theme.fontBody, pixelSize: 14, weight: Font.Normal,
            lineHeight: 20, letterSpacing: 0
        },
        labelLarge: {
            family: theme.fontBody, pixelSize: 16, weight: Font.Medium,
            lineHeight: 24, letterSpacing: 0
        },
        labelMedium: {
            family: theme.fontBody, pixelSize: 14, weight: Font.Medium,
            lineHeight: 20, letterSpacing: 0
        },
        labelButton: {
            family: theme.fontBody, pixelSize: 16, weight: Font.DemiBold,
            lineHeight: 20, letterSpacing: 0
        },
        caption: {
            family: theme.fontBody, pixelSize: 12, weight: Font.Medium,
            lineHeight: 16, letterSpacing: 0
        },
        overline: {
            family: theme.fontBody, pixelSize: 12, weight: Font.DemiBold,
            lineHeight: 16, letterSpacing: 0.96
        },
        monoMedium: {
            family: theme.fontMono, pixelSize: 14, weight: Font.Medium,
            lineHeight: 20, letterSpacing: 0
        },
        monoPin: {
            family: theme.fontMono, pixelSize: 28, weight: Font.DemiBold,
            lineHeight: 36, letterSpacing: 0
        }
    })

    readonly property url brandMark: "qrc:/qt/qml/LynxPlay/assets/images/brand-mark.png"
    readonly property url consoleRender: "qrc:/qt/qml/LynxPlay/assets/images/console-render.png"
    readonly property url consoleGlyph: "qrc:/qt/qml/LynxPlay/assets/images/console-glyph.svg"
}
