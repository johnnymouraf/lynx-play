import QtQuick
import QtQuick.Controls.Basic
import MFPlay

// A window control of the title bar. The glyphs come from the system's icon
// font, which draws them crisp at every scale like the native controls.
// `nativeHovered` and `nativePressed` stand in for the mouse when Windows owns
// it over the button (the maximize button).
Rectangle {
    id: button

    // A code point of Segoe Fluent Icons (Windows 11) or Segoe MDL2 Assets.
    property string glyph
    property string tooltip
    property bool danger: false
    property bool nativeHovered: false
    property bool nativePressed: false

    readonly property bool hot: hover.hovered || button.nativeHovered
    readonly property bool down: tap.pressed || button.nativePressed
    readonly property string iconFont: Qt.fontFamilies().indexOf("Segoe Fluent Icons") >= 0
                                       ? "Segoe Fluent Icons" : "Segoe MDL2 Assets"

    signal clicked

    implicitWidth: Theme.space12
    implicitHeight: Theme.sizeControlSm
    color: {
        if (button.danger) {
            if (button.down) {
                return Qt.darker(Theme.danger, 1.2);
            }

            return button.hot ? Theme.danger : "transparent";
        }

        if (button.down) {
            return Theme.surfaceHover;
        }

        return button.hot ? Theme.surfaceHigh : "transparent";
    }
    Accessible.role: Accessible.Button
    Accessible.name: button.tooltip
    Accessible.onPressAction: button.clicked()

    Text {
        anchors.centerIn: parent
        text: button.glyph
        color: button.danger && button.hot ? Theme.dangerContent : Theme.text
        font.family: button.iconFont
        font.pixelSize: 10
        renderType: Text.NativeRendering
    }

    HoverHandler {
        id: hover
    }

    TapHandler {
        id: tap

        onTapped: button.clicked()
    }

    ToolTip.visible: button.hot
    ToolTip.delay: 800
    ToolTip.text: button.tooltip
}
