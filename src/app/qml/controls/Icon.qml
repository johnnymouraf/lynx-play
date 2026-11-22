import QtQuick
import LynxPlay

// A Lucide icon from assets/icons, drawn in any color by the "icon" image
// provider. `bold` matches the design's icon weight 600.
Image {
    id: icon

    property string name
    property color color: Theme.text
    property int size: Theme.iconSize
    property bool bold: false

    readonly property string colorHex: icon.color.toString().substring(1)
    readonly property string strokeWidth: icon.bold ? "2.5" : "2"

    width: icon.size
    height: icon.size
    sourceSize: Qt.size(icon.size, icon.size)
    fillMode: Image.PreserveAspectFit
    source: icon.name.length > 0
            ? `image://icon/${icon.name}/${icon.colorHex}/${icon.strokeWidth}`
            : ""
    Accessible.ignored: true
}
