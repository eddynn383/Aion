import QtQuick
import Aion

Rectangle {
    id: root

    property string message: "Gata"
    property int framesPerSecond: 0
    property bool orthographic: false
    property string selectionText: ""
    property string unitText: "mm"

    height: Theme.statusBarHeight
    color: Theme.bgPanel

    Separator {
        anchors.top: parent.top
        width: parent.width
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        text: root.message
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSm
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space3

        Repeater {
            model: [
                root.selectionText,
                "Unități: " + root.unitText,
                root.orthographic ? "Paralelă" : "Perspectivă",
                AppInfo.graphicsApi,
                root.framesPerSecond > 0 ? root.framesPerSecond + " fps" : "",
                "v" + AppInfo.version
            ]
            delegate: Text {
                required property string modelData
                visible: modelData.length > 0
                text: modelData
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeSm
                font.family: modelData.indexOf("fps") >= 0 ? Theme.fontMono : Theme.fontUi
            }
        }
    }
}
