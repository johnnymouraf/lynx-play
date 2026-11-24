pragma Singleton

import QtQuick

// The design's "Theme" variables, named after them: `color/x` is `x`, every
// other group keeps its prefix (`space/4` is `space4`, `size/control` is
// `sizeControl`). Colors with alpha are written #AARRGGBB, as QML reads them.
QtObject {
    id: theme

    readonly property color bg: "#1A1A1A"
    readonly property color menu: "#121212"
    readonly property color surface: "#232323"
    readonly property color surfaceRaised: "#2C2C2C"
    readonly property color surfaceHigh: "#2C2C2C"
    readonly property color surfaceHover: "#333333"
    readonly property color border: "#2E2E2E"
    readonly property color borderStrong: "#3A3A3A"
    readonly property color borderControl: "#5C5C5C"
    readonly property color text: "#F5F5F5"
    readonly property color textMuted: "#A6A6A6"
    readonly property color textSubtle: "#737373"
    readonly property color textDisabled: "#4D4D4D"
    readonly property color accent: "#0070D1"
    readonly property color accentHover: "#1A85E6"
    readonly property color accentPressed: "#005BB0"
    readonly property color accentText: "#FFFFFF"
    readonly property color accentSubtle: "#12263B"
    readonly property color focus: "#FFFFFF"
    readonly property color ready: "#3DD68C"
    readonly property color readySubtle: "#1B2E24"
    readonly property color standby: "#FFB547"
    readonly property color standbySubtle: "#2E2617"
    readonly property color offline: "#8C8C8C"
    readonly property color offlineSubtle: "#262626"
    readonly property color danger: "#F2625F"
    readonly property color dangerSubtle: "#331E1E"
    readonly property color dangerText: "#1C0707"
    readonly property color info: "#B0B0B0"
    readonly property color infoSubtle: "#2C2C2C"
    readonly property color scrim: "#CC000000"
    readonly property color overlayPanel: "#E6121212"
    readonly property color overlayBorder: "#1FFFFFFF"
    readonly property color overlayControl: "#0FFFFFFF"
    readonly property color psCross: "#7CB2E8"
    readonly property color psCircle: "#FF6B6B"
    readonly property color psTriangle: "#3EDBA0"
    readonly property color psSquare: "#F07FD8"

    // Not variables in the design: the backdrop behind the console render and
    // the icon tiles of buttons and rail entries, drawn as literals there.
    readonly property color mediaBackdrop: "#121212"
    readonly property color tileOnAccent: "#33000000"
    readonly property color tileOnSurface: "#12FFFFFF"

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

    readonly property int radiusSm: 4
    readonly property int radiusMd: 6
    readonly property int radiusLg: 6
    readonly property int radiusXl: 8
    readonly property int radiusFull: 999
    // Not a variable in the design: the 32 px icon tiles inside buttons.
    readonly property int radiusTile: 5

    readonly property int sizeControlSm: 36
    readonly property int sizeControl: 44
    readonly property int sizeControlLg: 52
    readonly property int sizeFocusRing: 2
    readonly property int sizeIcon: 20
    readonly property int sizeRail: 232

    readonly property string fontDisplay: "Sora"
    readonly property string fontBody: "Inter"
    readonly property string fontMono: "JetBrains Mono"

    readonly property int typeCaption: 12
    readonly property int typeBody: 14
    readonly property int typeBodyLg: 16
    readonly property int typeTitleSm: 18
    readonly property int typeTitle: 22
    readonly property int typeTitleLg: 28
    readonly property int typeDisplay: 36
    readonly property int typePin: 28

    // The design's text styles, read by AppText through its textStyle name.
    // The last ones are the literal fonts of the badge, the overlines, the
    // dialog title, the status counters and the controller card.
    readonly property var typography: ({
        display: {
            family: theme.fontDisplay, pixelSize: theme.typeDisplay, weight: Font.DemiBold,
            lineHeight: 44, letterSpacing: -0.36
        },
        titleLarge: {
            family: theme.fontDisplay, pixelSize: theme.typeTitleLg, weight: Font.DemiBold,
            lineHeight: 36, letterSpacing: -0.28
        },
        titleMedium: {
            family: theme.fontDisplay, pixelSize: theme.typeTitle, weight: Font.DemiBold,
            lineHeight: 28, letterSpacing: 0
        },
        titleSmall: {
            family: theme.fontBody, pixelSize: theme.typeTitleSm, weight: Font.DemiBold,
            lineHeight: 24, letterSpacing: 0
        },
        bodyLarge: {
            family: theme.fontBody, pixelSize: theme.typeBodyLg, weight: Font.Normal,
            lineHeight: 24, letterSpacing: 0
        },
        bodyMedium: {
            family: theme.fontBody, pixelSize: theme.typeBody, weight: Font.Normal,
            lineHeight: 20, letterSpacing: 0
        },
        labelLarge: {
            family: theme.fontBody, pixelSize: theme.typeBodyLg, weight: Font.Medium,
            lineHeight: 24, letterSpacing: 0
        },
        labelMedium: {
            family: theme.fontBody, pixelSize: theme.typeBody, weight: Font.Medium,
            lineHeight: 20, letterSpacing: 0
        },
        labelButton: {
            family: theme.fontBody, pixelSize: theme.typeBodyLg, weight: Font.DemiBold,
            lineHeight: 20, letterSpacing: 0
        },
        caption: {
            family: theme.fontBody, pixelSize: theme.typeCaption, weight: Font.Medium,
            lineHeight: 16, letterSpacing: 0
        },
        monoMedium: {
            family: theme.fontMono, pixelSize: theme.typeBody, weight: Font.Medium,
            lineHeight: 20, letterSpacing: 0
        },
        monoPin: {
            family: theme.fontMono, pixelSize: theme.typePin, weight: Font.DemiBold,
            lineHeight: 36, letterSpacing: 0
        },
        overline: {
            family: theme.fontMono, pixelSize: 10, weight: Font.DemiBold,
            lineHeight: 12, letterSpacing: 2, capitalization: Font.AllUppercase
        },
        tag: {
            family: theme.fontMono, pixelSize: 10, weight: Font.Bold,
            lineHeight: 12, letterSpacing: 1.2, capitalization: Font.AllUppercase
        },
        dialogTitle: {
            family: theme.fontDisplay, pixelSize: 20, weight: Font.Bold,
            lineHeight: 26, letterSpacing: -0.4
        },
        count: {
            family: theme.fontBody, pixelSize: 13, weight: Font.Bold,
            lineHeight: 16, letterSpacing: 0
        },
        bodySmall: {
            family: theme.fontBody, pixelSize: 13, weight: Font.Normal,
            lineHeight: 16, letterSpacing: 0
        },
        labelSmall: {
            family: theme.fontBody, pixelSize: 13, weight: Font.DemiBold,
            lineHeight: 16, letterSpacing: 0
        },
        detail: {
            family: theme.fontBody, pixelSize: theme.typeCaption, weight: Font.Normal,
            lineHeight: 15, letterSpacing: 0
        }
    })

    readonly property url brandMark: "qrc:/qt/qml/MFPlay/assets/images/app-icon.svg"
    readonly property url consoleRender: "qrc:/qt/qml/MFPlay/assets/images/console-render.png"
    readonly property url consoleGlyph: "qrc:/qt/qml/MFPlay/assets/images/console-glyph.svg"
}
