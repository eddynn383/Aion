import QtQuick
import Forma

Rectangle {
    id: root

    property string message: "Gata"

    height: Theme.statusBarHeight
    color: Theme.bgPanel

    Rectangle {
        anchors.top: parent.top
        width: parent.width
        height: 1
        color: Theme.borderSubtle
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        text: root.message
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSm
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        text: "Unități: mm  ·  " + AppInfo.graphicsApi + "  ·  Temă: " + Theme.modeLabel
              + "  ·  v" + AppInfo.version
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm
    }
}
