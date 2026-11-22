import QtQuick
import QtQuick.Effects
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// The design's DialogShell: a modal over a scrim, with an optional eyebrow,
// a title, a close button, the body and a footer for the actions.
T.Popup {
    id: dialog

    property string title
    property string eyebrow
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
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.border
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.dialogShadow
            shadowVerticalOffset: 24
            shadowBlur: 1.0
            blurMax: 64
        }
    }

    contentItem: ColumnLayout {
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Theme.space6
            Layout.leftMargin: Theme.space6
            Layout.rightMargin: Theme.space4
            Layout.bottomMargin: Theme.space2
            spacing: Theme.space4

            ColumnLayout {
                Layout.fillWidth: true
                Layout.topMargin: Theme.space1
                Layout.alignment: Qt.AlignTop
                spacing: Theme.space1

                AppText {
                    visible: dialog.eyebrow.length > 0
                    text: dialog.eyebrow
                    textStyle: "overline"
                    font.capitalization: Font.AllUppercase
                    color: Theme.accent
                }

                AppText {
                    Layout.fillWidth: true
                    text: dialog.title
                    textStyle: "titleMedium"
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.Heading
                }
            }

            IconButton {
                Layout.alignment: Qt.AlignTop
                iconName: "x"
                text: "Fechar"
                onClicked: dialog.close()
            }
        }

        ColumnLayout {
            id: bodyColumn

            Layout.fillWidth: true
            Layout.topMargin: Theme.space4
            Layout.leftMargin: Theme.space6
            Layout.rightMargin: Theme.space6
            Layout.bottomMargin: Theme.space6
            spacing: Theme.space5
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 1
            Layout.rightMargin: 1
            Layout.bottomMargin: 1
            visible: dialog.showFooter
            implicitHeight: actionRow.implicitHeight + 2 * Theme.space4
            color: Theme.surface
            bottomLeftRadius: Theme.radiusXl - 1
            bottomRightRadius: Theme.radiusXl - 1

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                height: 1
                color: Theme.border
            }

            Row {
                id: actionRow

                anchors.right: parent.right
                anchors.rightMargin: Theme.space6
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space3
            }
        }
    }
}
