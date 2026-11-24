import QtQuick
import QtQuick.Templates as T
import MFPlay

// A menu of MenuRow entries, styled as the design's open Select.
T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            contentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             contentHeight + topPadding + bottomPadding)
    padding: Theme.space1
    margins: 0

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: false
        spacing: 2
        currentIndex: control.currentIndex
    }

    background: Rectangle {
        implicitWidth: 200
        radius: Theme.radiusMd
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.borderStrong
    }
}
