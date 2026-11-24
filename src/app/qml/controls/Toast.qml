import QtQuick
import QtQuick.Effects
import MFPlay

// A Banner that floats over the screen for a few seconds.
Item {
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
    implicitHeight: banner.implicitHeight
    opacity: 0
    visible: opacity > 0

    Behavior on opacity {
        NumberAnimation {
            duration: 150
        }
    }

    Banner {
        id: banner

        width: toast.width
        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowColor: Theme.toastShadow
            shadowVerticalOffset: 8
            shadowBlur: 1.0
            blurMax: 24
        }
    }

    Timer {
        id: hideTimer

        interval: toast.duration
        onTriggered: toast.opacity = 0
    }
}
