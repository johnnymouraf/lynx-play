import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// The design's TextField: label, input, and a helper line that turns into the
// error message when `errorText` is set.
ColumnLayout {
    id: field

    property string label
    property alias text: input.text
    property alias placeholderText: input.placeholderText
    property alias maximumLength: input.maximumLength
    property string helperText
    property string errorText
    readonly property bool hasError: errorText.length > 0

    signal accepted

    function focusInput() {
        input.forceActiveFocus(Qt.TabFocusReason);
        input.selectAll();
    }

    spacing: Theme.space2

    AppText {
        text: field.label
        textStyle: "labelMedium"
    }

    T.TextField {
        id: input

        Layout.fillWidth: true
        implicitHeight: Theme.sizeControl
        leftPadding: Theme.space3
        rightPadding: Theme.space3
        color: Theme.text
        selectionColor: Theme.accent
        selectedTextColor: Theme.accentText
        placeholderTextColor: Theme.textSubtle
        font.family: Theme.fontBody
        font.pixelSize: Theme.typeBodyLg
        verticalAlignment: TextInput.AlignVCenter
        selectByMouse: true
        Accessible.name: field.label
        Accessible.description: field.hasError ? field.errorText : field.helperText
        onAccepted: field.accepted()

        background: Rectangle {
            id: box

            radius: Theme.radiusMd
            color: Theme.bg
            border.width: 1
            border.color: Theme.borderControl

            FocusRing {
                targetRadius: box.radius
                visible: input.activeFocus
            }

            states: [
                State {
                    name: "error"
                    when: field.hasError

                    PropertyChanges {
                        target: box
                        border.color: Theme.danger
                    }
                },
                State {
                    name: "focused"
                    when: input.activeFocus

                    PropertyChanges {
                        target: box
                        border.width: 0
                    }
                }
            ]
        }

        AppText {
            x: input.leftPadding
            anchors.verticalCenter: parent.verticalCenter
            width: input.width - input.leftPadding - input.rightPadding
            visible: input.length === 0 && input.preeditText.length === 0
            text: input.placeholderText
            textStyle: "bodyLarge"
            color: input.placeholderTextColor
        }
    }

    AppText {
        Layout.fillWidth: true
        visible: field.hasError || field.helperText.length > 0
        text: field.hasError ? field.errorText : field.helperText
        textStyle: "bodyMedium"
        color: field.hasError ? Theme.danger : Theme.textMuted
        wrapMode: Text.WordWrap
    }
}
