import QtQuick
import Aion

// O secțiune a panoului acordeon: antetul (click = strânge / desface) și conținutul. Înălțimea conținutului o
// stabilește părintele (`bodyHeight`), după secțiunile deschise.
Item {
    id: root

    property string title: ""
    property string iconName: ""
    property bool expanded: true
    property real weight: 1
    property real bodyHeight: 0
    default property alias content: body.data

    readonly property real headerHeight: Theme.controlHeight

    signal toggled()

    height: headerHeight + (expanded ? bodyHeight : 0)
    clip: true

    Behavior on height { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }

    Rectangle {
        id: header
        width: parent.width
        height: root.headerHeight
        radius: Theme.radiusSm
        color: headerArea.containsMouse ? Theme.bgHover : "transparent"
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space2
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron-right"
                size: 12
                color: Theme.textTertiary
                rotation: root.expanded ? 90 : 0
                Behavior on rotation { NumberAnimation { duration: Theme.motionFast } }
            }
            Icon {
                visible: root.iconName.length > 0
                anchors.verticalCenter: parent.verticalCenter
                name: root.iconName
                size: Theme.iconSizeSm
                color: Theme.textSecondary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.title
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.DemiBold
            }
        }
        MouseArea {
            id: headerArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.toggled()
        }
    }

    Item {
        id: body
        anchors.top: header.bottom
        anchors.topMargin: Theme.space1
        width: parent.width
        height: Math.max(0, root.bodyHeight - Theme.space2)
        visible: root.expanded
    }
}
