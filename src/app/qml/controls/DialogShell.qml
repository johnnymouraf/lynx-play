import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import MFPlay

// The design's DialogShell: a modal over a scrim, with an optional icon tile
// and eyebrow, a title, a close button, the body and the actions at the end.
T.Popup {
    id: dialog

    property string title
    property string eyebrow
    property string iconName
    default property alias body: bodyColumn.data
    property alias actions: actionRow.data
    property bool showFooter: true

    parent: T.Overlay.overlay
    anchors.centerIn: parent
    width: 560
    implicitHeight: implicitContentHeight + topPadding + bottomPadding
    modal: true
    focus: true
    padding: 0
    closePolicy: T.Popup.CloseOnEscape

    T.Overlay.modal: Rectangle {
        color: Theme.scrim
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0
            to: 1
            duration: 120
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1
            to: 0
            duration: 100
        }
    }

    background: Rectangle {
        radius: Theme.radiusXl
        color: Theme.surface
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space5
            Layout.leftMargin: Theme.space5
            Layout.rightMargin: Theme.space5
            Layout.bottomMargin: Theme.space4
            spacing: 14

            IconTile {
                visible: dialog.iconName.length > 0
                implicitWidth: Theme.space10
                implicitHeight: Theme.space10
                radius: Theme.radiusMd
                color: Theme.accent
                iconName: dialog.iconName
                iconSize: Theme.sizeIcon
                iconColor: Theme.onAccent
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                AppText {
                    visible: dialog.eyebrow.length > 0
                    text: dialog.eyebrow
                    textStyle: "overline"
                    color: Theme.textSubtle
                }

                AppText {
                    Layout.fillWidth: true
                    text: dialog.title
                    textStyle: "dialogTitle"
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.Heading
                }
            }

            IconButton {
                implicitWidth: Theme.sizeControlSm
                implicitHeight: Theme.sizeControlSm
                restingFill: Theme.surfaceRaised
                iconName: "x"
                iconSize: Theme.space4
                text: "Fechar"
                onClicked: dialog.close()
            }
        }

        ColumnLayout {
            id: bodyColumn

            Layout.fillWidth: true
            Layout.topMargin: Theme.space1
            Layout.leftMargin: Theme.space5
            Layout.rightMargin: Theme.space5
            Layout.bottomMargin: Theme.space5
            spacing: Theme.space4
        }

        Row {
            id: actionRow

            Layout.alignment: Qt.AlignRight
            Layout.rightMargin: Theme.space5
            Layout.bottomMargin: Theme.space5
            visible: dialog.showFooter
            spacing: 10
        }
    }
}
