import QtQuick
import QtQuick.Effects

// An image cropped to fill its box, clipped to rounded corners.
Item {
    id: root

    property alias source: image.source
    // Decoded width in pixels; fixed so that resizing doesn't decode again.
    property int sourceWidth: 512
    property real topLeftRadius: 0
    property real topRightRadius: 0
    property real bottomLeftRadius: 0
    property real bottomRightRadius: 0

    Image {
        id: image

        anchors.fill: parent
        visible: false
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: root.sourceWidth
        mipmap: true
        asynchronous: true
    }

    Rectangle {
        id: mask

        anchors.fill: parent
        visible: false
        layer.enabled: true
        antialiasing: true
        topLeftRadius: root.topLeftRadius
        topRightRadius: root.topRightRadius
        bottomLeftRadius: root.bottomLeftRadius
        bottomRightRadius: root.bottomRightRadius
    }

    MultiEffect {
        anchors.fill: parent
        source: image
        maskEnabled: true
        maskSource: mask
        maskThresholdMin: 0.5
        maskSpreadAtMin: 1.0
    }
}
