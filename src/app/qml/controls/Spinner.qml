import QtQuick
import QtQuick.Shapes
import MFPlay

// The design's Spinner: an accent arc turning over a ring.
Item {
    id: spinner

    property int size: 64
    readonly property real thickness: size * (1 - 0.8125) / 2

    implicitWidth: spinner.size
    implicitHeight: spinner.size
    Accessible.role: Accessible.Animation
    Accessible.name: "Carregando"

    Rectangle {
        anchors.fill: parent
        radius: width / 2
        color: "transparent"
        border.width: spinner.thickness
        border.color: Theme.border
    }

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.accent
            strokeWidth: spinner.thickness
            fillColor: "transparent"
            capStyle: ShapePath.FlatCap

            PathAngleArc {
                centerX: spinner.size / 2
                centerY: spinner.size / 2
                radiusX: (spinner.size - spinner.thickness) / 2
                radiusY: (spinner.size - spinner.thickness) / 2
                startAngle: -90
                sweepAngle: 110
            }
        }

        RotationAnimator on rotation {
            from: 0
            to: 360
            duration: 1000
            loops: Animation.Infinite
            running: spinner.visible
        }
    }
}
