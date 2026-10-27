import QtQuick
import Aion

// O secțiune a panoului acordeon: antetul (click = strânge / desface) și conținutul. Toate secțiunile au același
// antet: linie de separare, săgeată, iconiță, titlu cu majuscule, numărul de elemente și acțiunile din dreapta
// (de exemplu „+”). Înălțimea conținutului o stabilește părintele (`bodyHeight`), după secțiunile deschise.
Item {
    id: root

    property string title: ""
    property string iconName: ""
    property bool expanded: true
    property real weight: 1
    property real bodyHeight: 0
    property int count: -1 // -1 = fără număr
    property bool separator: true
    default property alias content: body.data
    // Butoanele din dreapta antetului (IconButton-uri de 22 px).
    property alias actions: actionsRow.data

    readonly property real headerHeight: Theme.controlHeight

    signal toggled()

    height: headerHeight + (expanded ? bodyHeight : 0)
    clip: true

    Behavior on height { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }

    Separator {
        visible: root.separator
        width: parent.width
    }

    Item {
        id: header
        width: parent.width
        height: root.headerHeight

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 2
            radius: Theme.radiusSm
            color: headerArea.containsMouse ? Theme.bgHover : "transparent"
        }
        MouseArea {
            id: headerArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: root.toggled()
        }
        Row {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space1 + 2
            Icon {
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron-right"
                size: 11
                color: Theme.textTertiary
                rotation: root.expanded ? 90 : 0
                Behavior on rotation { NumberAnimation { duration: Theme.motionFast } }
            }
            Icon {
                visible: root.iconName.length > 0
                anchors.verticalCenter: parent.verticalCenter
                name: root.iconName
                size: 13
                color: root.expanded ? Theme.textSecondary : Theme.textTertiary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.title.toUpperCase()
                color: root.expanded ? Theme.textSecondary : Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.DemiBold
                font.letterSpacing: 0.6
            }
            Text {
                visible: root.count > 0
                anchors.verticalCenter: parent.verticalCenter
                text: root.count
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
                font.family: Theme.fontMono
            }
        }
        Row {
            id: actionsRow
            anchors.right: parent.right
            anchors.rightMargin: 2
            anchors.verticalCenter: parent.verticalCenter
            spacing: 0
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
