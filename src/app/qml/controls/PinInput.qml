import QtQuick
import LynxPlay

// The eight-digit pairing code: digit boxes over a hidden input that takes
// what is typed or pasted.
Item {
    id: pin

    property bool error: false
    property bool readOnly: false
    property alias text: input.text
    readonly property int digitCount: 8

    signal accepted

    function focusInput() {
        input.forceActiveFocus(Qt.TabFocusReason);
    }

    implicitWidth: row.implicitWidth
    implicitHeight: 60

    TextInput {
        id: input

        anchors.fill: parent
        opacity: 0
        maximumLength: pin.digitCount
        validator: RegularExpressionValidator {
            regularExpression: /[0-9]{0,8}/
        }
        inputMethodHints: Qt.ImhDigitsOnly
        readOnly: pin.readOnly
        Accessible.role: Accessible.EditableText
        Accessible.name: "Código mostrado na TV"
        onAccepted: pin.accepted()
    }

    Row {
        id: row

        spacing: Theme.space2

        Repeater {
            model: pin.digitCount

            delegate: Row {
                id: slot

                required property int index
                readonly property string digit: input.text.charAt(index)
                readonly property int nextIndex: Math.min(input.text.length, pin.digitCount - 1)
                readonly property bool current: input.activeFocus && !pin.readOnly && index === nextIndex

                spacing: Theme.space2

                Rectangle {
                    id: box

                    width: 48
                    height: 60
                    radius: Theme.radiusMd
                    color: Theme.bg
                    border.width: 1
                    border.color: Theme.borderControl

                    AppText {
                        anchors.centerIn: parent
                        text: slot.digit
                        textStyle: "monoPin"
                        color: pin.error ? Theme.danger : Theme.text
                    }

                    Rectangle {
                        anchors.centerIn: parent
                        width: 2
                        height: 28
                        radius: 1
                        color: Theme.accent
                        visible: slot.current && slot.digit.length === 0

                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            running: slot.current

                            PauseAnimation {
                                duration: 500
                            }

                            NumberAnimation {
                                to: 0
                                duration: 0
                            }

                            PauseAnimation {
                                duration: 500
                            }

                            NumberAnimation {
                                to: 1
                                duration: 0
                            }
                        }
                    }

                    FocusRing {
                        targetRadius: box.radius
                        visible: slot.current && !pin.error
                    }

                    states: [
                        State {
                            name: "error"
                            when: pin.error

                            PropertyChanges {
                                target: box
                                border.color: Theme.danger
                            }
                        },
                        State {
                            name: "current"
                            when: slot.current

                            PropertyChanges {
                                target: box
                                border.color: Theme.accent
                            }
                        }
                    ]
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: slot.index === 3
                    width: 10
                    height: 2
                    radius: 1
                    color: Theme.borderStrong
                }
            }
        }
    }
}
