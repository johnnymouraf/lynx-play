import QtQuick
import QtQuick.Layouts
import QtQuick.Shapes
import QtQuick.Templates as T
import MFPlay

// The design's "Add console" tile after the last card: a dashed outline with
// a plus, stacked in the grid and side by side in the list.
T.AbstractButton {
    id: tile

    property bool listMode: false
    // Set by the page, which moves the focus between the cards itself.
    property bool focused: false

    focusPolicy: Qt.NoFocus
    hoverEnabled: true
    text: "Adicionar pelo IP"
    Accessible.role: Accessible.Button

    contentItem: Item {
        RowLayout {
            anchors.fill: parent
            anchors.margins: Theme.space4
            visible: tile.listMode
            spacing: Theme.space3

            IconTile {
                Layout.alignment: Qt.AlignVCenter
                implicitWidth: 44
                implicitHeight: 44
                radius: Theme.radiusSm
                color: tile.hovered ? Theme.surfaceHigh : Theme.surface
                iconName: "plus"
                iconSize: Theme.sizeIcon
            }

            AppText {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                text: tile.text
                textStyle: "labelLarge"
                color: tile.hovered ? Theme.text : Theme.textMuted
            }
        }

        Grid {
            anchors.centerIn: parent
            visible: !tile.listMode
            columns: 1
            spacing: 10
            horizontalItemAlignment: Grid.AlignHCenter
            verticalItemAlignment: Grid.AlignVCenter

            IconTile {
                implicitWidth: 44
                implicitHeight: 44
                radius: Theme.radiusSm
                color: tile.hovered ? Theme.surfaceHigh : Theme.surface
                iconName: "plus"
                iconSize: Theme.sizeIcon
            }

            AppText {
                text: tile.text
                textStyle: "labelSmall"
                color: tile.hovered ? Theme.text : Theme.textMuted
            }
        }
    }

    background: Item {
        Shape {
            anchors.fill: parent

            ShapePath {
                strokeColor: Theme.borderStrong
                strokeWidth: 1
                strokeStyle: ShapePath.DashLine
                dashPattern: [4, 4]
                fillColor: "transparent"

                PathRectangle {
                    x: 0.5
                    y: 0.5
                    width: tile.width - 1
                    height: tile.height - 1
                    radius: Theme.radiusMd
                }
            }
        }

        FocusRing {
            targetRadius: Theme.radiusMd
            visible: tile.focused
        }
    }
}
