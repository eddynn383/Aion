import QtQuick
import Aion

Rectangle {
    id: root

    property string message: qsTrId("StatusBar.gata")
    property int framesPerSecond: 0
    property bool orthographic: false
    property string selectionText: ""
    property string unitText: "mm"
    property bool snapToGrid: false
    property string gridText: "auto"

    signal snapToggled()

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

        // Comutatorul pentru snap la grid (click = pornit/oprit).
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: snapRow.implicitWidth + Theme.space2 * 2
            height: 20
            radius: 10
            color: root.snapToGrid ? Theme.accentSubtle : snapArea.containsMouse ? Theme.bgHover : "transparent"
            border.color: root.snapToGrid ? Theme.accent : Theme.borderSubtle
            Row {
                id: snapRow
                anchors.centerIn: parent
                spacing: Theme.space1
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "grid-3x3"
                    size: 12
                    color: root.snapToGrid ? Theme.accent : Theme.textTertiary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.snapToGrid ? qsTrId("StatusBar.snap") + root.gridText : qsTrId("StatusBar.snap_oprit")
                    color: root.snapToGrid ? Theme.textPrimary : Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                }
            }
            MouseArea {
                id: snapArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: root.snapToggled()
            }
        }

        Repeater {
            model: [
                root.selectionText,
                qsTrId("StatusBar.unitati") + root.unitText,
                root.orthographic ? qsTrId("StatusBar.paralela") : qsTrId("StatusBar.perspectiva"),
                AppInfo.graphicsApi,
                root.framesPerSecond > 0 ? root.framesPerSecond + qsTrId("StatusBar.fps") : "",
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
