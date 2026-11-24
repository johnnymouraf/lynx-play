import QtQuick
import MFPlay

// A Banner on a solid block that floats over the screen for a few seconds.
Rectangle {
    id: toast

    property int duration: 5000

    function show(tone, title, message) {
        banner.tone = tone;
        banner.title = title;
        banner.message = message;
        toast.opacity = 1;
        hideTimer.restart();
    }

    implicitWidth: 400
    implicitHeight: banner.implicitHeight + 2 * Theme.space4
    radius: Theme.radiusMd
    color: Theme.surfaceRaised
    border.width: 1
    border.color: Theme.borderStrong
    opacity: 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: 150
        }
    }

    Banner {
        id: banner

        anchors.fill: parent
        anchors.margins: Theme.space4
    }

    Timer {
        id: hideTimer

        interval: toast.duration
        onTriggered: toast.opacity = 0
    }
}
