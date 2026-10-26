import QtQuick
import QtQuick.Window
import Forma

// Iconiță Lucide (SVG), recolorată după temă și randată la rezoluția ecranului.
Image {
    id: root

    property string name: ""
    property color color: Theme.textPrimary
    property int size: Theme.iconSizeMd

    width: size
    height: size
    sourceSize: Qt.size(size * Screen.devicePixelRatio, size * Screen.devicePixelRatio)
    source: name.length > 0 ? "image://icon/" + root.color.toString().slice(1) + "/" + name : ""
    fillMode: Image.PreserveAspectFit
    smooth: true
}
