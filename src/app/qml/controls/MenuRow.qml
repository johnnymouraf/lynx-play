import QtQuick
import QtQuick.Templates as T
import LynxPlay

// An entry of a PopupMenu, with an icon; `danger` paints it red.
T.MenuItem {
    id: control

    property string iconName
    property bool danger: false
    readonly property color contentColor: {
        if (!control.enabled) {
            return Theme.textDisabled;
        }

        return control.danger ? Theme.danger : Theme.text;
    }

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: 40
    leftPadding: Theme.space3
    rightPadding: Theme.space3
    spacing: Theme.space2
    hoverEnabled: true

    contentItem: Row {
        spacing: control.spacing

        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: control.iconName
            size: 18
            color: control.contentColor
        }

        AppText {
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            textStyle: "bodyMedium"
            color: control.contentColor
        }
    }

    background: Rectangle {
        radius: Theme.radiusSm
        color: control.highlighted ? Theme.surfaceHigh : "transparent"
    }
}
