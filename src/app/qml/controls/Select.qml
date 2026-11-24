import QtQuick
import QtQuick.Controls.Basic as Basic
import QtQuick.Layouts
import MFPlay

// The design's Select: an optional label over a value with a chevron that
// opens the list of choices below it. `options` lists {label, value}.
FocusScope {
    id: control

    // Shown above the box; `name` alone names it for screen readers.
    property string label
    property string name
    property var options: []
    property var currentValue

    signal activated(var value)

    readonly property int currentIndex: {
        for (let i = 0; i < control.options.length; ++i) {
            if (control.options[i].value === control.currentValue) {
                return i;
            }
        }

        return -1;
    }
    readonly property string currentLabel: {
        if (control.currentIndex >= 0) {
            return control.options[control.currentIndex].label;
        }

        return control.currentValue !== undefined && control.currentValue !== null
                ? String(control.currentValue) : "";
    }

    function openMenu() {
        menu.highlightedIndex = Math.max(0, control.currentIndex);
        menu.open();
    }

    function choose(index) {
        if (index < 0 || index >= control.options.length) {
            return;
        }

        const value = control.options[index].value;
        menu.close();
        control.forceActiveFocus(Qt.TabFocusReason);
        control.activated(value);
    }

    implicitWidth: 300
    implicitHeight: column.implicitHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.ComboBox
    Accessible.name: control.label.length > 0 ? control.label : control.name
    Accessible.description: control.currentLabel
    Accessible.onPressAction: control.openMenu()

    Column {
        id: column

        width: control.width
        spacing: Theme.space2

        AppText {
            visible: control.label.length > 0
            text: control.label
            textStyle: "labelMedium"
        }

        Rectangle {
            id: box

            width: column.width
            height: Theme.sizeControl
            radius: Theme.radiusMd
            color: Theme.surfaceHigh
            border.width: control.activeFocus && !menu.visible ? 0 : 1
            border.color: menu.visible ? Theme.accent : Theme.borderControl

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                spacing: Theme.space2

                AppText {
                    Layout.fillWidth: true
                    text: control.currentLabel
                    textStyle: "bodyLarge"
                }

                Icon {
                    name: menu.visible ? "chevron-up" : "chevron-down"
                    color: Theme.textMuted
                }
            }

            FocusRing {
                targetRadius: box.radius
                visible: control.activeFocus && !menu.visible
            }

            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    control.forceActiveFocus(Qt.MouseFocusReason);
                    control.openMenu();
                }
            }
        }
    }

    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Return
                || event.key === Qt.Key_Enter || event.key === Qt.Key_Down) {
            control.openMenu();
            event.accepted = true;
        }
    }

    Basic.Popup {
        id: menu

        property int highlightedIndex: 0

        parent: control
        x: 0
        y: box.y + box.height + Theme.space2
        width: Math.max(control.width, contentItem.implicitWidth + 2 * padding)
        padding: Theme.space1
        focus: true
        closePolicy: Basic.Popup.CloseOnEscape | Basic.Popup.CloseOnPressOutside
        onOpened: menuContent.forceActiveFocus(Qt.PopupFocusReason)

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.surfaceRaised
            border.width: 1
            border.color: Theme.borderStrong
        }

        contentItem: ColumnLayout {
            id: menuContent

            focus: true
            spacing: 2

            Keys.onPressed: event => {
                if (event.key === Qt.Key_Down) {
                    menu.highlightedIndex = Math.min(control.options.length - 1,
                                                      menu.highlightedIndex + 1);
                    event.accepted = true;
                    return;
                }

                if (event.key === Qt.Key_Up) {
                    menu.highlightedIndex = Math.max(0, menu.highlightedIndex - 1);
                    event.accepted = true;
                    return;
                }

                if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                    control.choose(menu.highlightedIndex);
                    event.accepted = true;
                }
            }

            Repeater {
                model: control.options

                Rectangle {
                    id: row

                    required property var modelData
                    required property int index

                    readonly property bool highlighted: row.index === menu.highlightedIndex
                    readonly property bool chosen: row.modelData.value === control.currentValue

                    Layout.fillWidth: true
                    implicitHeight: 40
                    radius: Theme.radiusSm
                    color: row.highlighted ? Theme.surfaceHigh : "transparent"
                    Accessible.role: Accessible.MenuItem
                    Accessible.name: row.modelData.label
                    Accessible.selected: row.chosen
                    Accessible.onPressAction: control.choose(row.index)

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: Theme.space3
                        anchors.rightMargin: Theme.space3
                        spacing: Theme.space2

                        AppText {
                            Layout.fillWidth: true
                            text: row.modelData.label
                            textStyle: "bodyMedium"
                            color: row.chosen ? Theme.accent : Theme.text
                        }

                        Icon {
                            visible: row.chosen
                            name: "check"
                            size: 18
                            color: Theme.accent
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onEntered: menu.highlightedIndex = row.index
                        onClicked: control.choose(row.index)
                    }
                }
            }
        }
    }
}
