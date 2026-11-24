import QtQuick
import QtQuick.Templates as T
import MFPlay

// The design's ViewToggle: shows the consoles as a grid or as a list.
Rectangle {
    id: viewToggle

    // "grid" or "list".
    property string mode: "grid"

    signal activated(string mode)

    implicitWidth: buttons.implicitWidth + 2 * Theme.space1
    implicitHeight: Theme.space10
    radius: Theme.radiusMd
    color: Theme.surface
    Accessible.role: Accessible.Grouping
    Accessible.name: "Exibição"

    Row {
        id: buttons

        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: [
                { mode: "grid", icon: "layout-grid", name: "Grade" },
                { mode: "list", icon: "list", name: "Lista" }
            ]

            T.Button {
                id: option

                required property var modelData
                readonly property bool selected: option.modelData.mode === viewToggle.mode

                implicitWidth: 34
                implicitHeight: 32
                focusPolicy: Qt.StrongFocus
                hoverEnabled: true
                text: option.modelData.name
                Accessible.role: Accessible.RadioButton
                Accessible.checked: option.selected
                onClicked: viewToggle.activated(option.modelData.mode)

                contentItem: Item {
                    Icon {
                        anchors.centerIn: parent
                        name: option.modelData.icon
                        size: 16
                        color: option.selected || option.hovered ? Theme.text : Theme.textSubtle
                    }
                }

                background: Rectangle {
                    id: fill

                    radius: Theme.radiusSm
                    color: option.selected ? Theme.surfaceHover : "transparent"

                    FocusRing {
                        targetRadius: fill.radius
                        visible: option.visualFocus
                    }
                }
            }
        }
    }
}
