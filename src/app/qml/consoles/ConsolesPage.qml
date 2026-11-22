import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import LynxPlay

// The home screen: the consoles on the network and the saved ones.
FocusScope {
    id: page

    signal addByAddressRequested

    function focusFirstControl() {
        emptyState.focusAction();
    }

    Flickable {
        id: scroller

        anchors.fill: parent
        anchors.topMargin: Theme.space6
        anchors.leftMargin: Theme.space8
        anchors.rightMargin: Theme.space8
        anchors.bottomMargin: Theme.space6
        contentWidth: width
        contentHeight: content.implicitHeight + 2 * Theme.space2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        T.ScrollBar.vertical: ThinScrollBar {}

        ColumnLayout {
            id: content

            x: Theme.space2
            y: Theme.space2
            width: scroller.width - 2 * Theme.space2
            spacing: Theme.space8

            RowLayout {
                id: header

                Layout.fillWidth: true
                spacing: Theme.space6

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Theme.space1

                    AppText {
                        text: "Consoles"
                        textStyle: "titleLarge"
                        Accessible.role: Accessible.Heading
                    }

                    ScanIndicator {}
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(emptyState.implicitHeight,
                                                 scroller.height - 2 * Theme.space2
                                                 - header.height - content.spacing)

                EmptyState {
                    id: emptyState

                    anchors.centerIn: parent
                    width: parent.width
                    onAddByAddressRequested: page.addByAddressRequested()
                }
            }
        }
    }
}
