import QtQuick
import Aion

// Panou lateral: antet (titlu sau tab-uri) și conținut așezat sub el.
Rectangle {
    id: root

    property string title: ""
    // Element opțional afișat în antet în locul titlului (de exemplu Tabs).
    property alias header: headerSlot.data
    default property alias content: body.data

    color: Theme.bgPanel

    Item {
        id: headerArea
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.space3
        height: Theme.controlHeightSm

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            visible: headerSlot.children.length === 0
            text: root.title
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }

        Item {
            id: headerSlot
            anchors.fill: parent
        }
    }

    Item {
        id: body
        anchors.top: headerArea.bottom
        anchors.topMargin: Theme.space3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space4
        anchors.rightMargin: Theme.space4
        anchors.bottomMargin: Theme.space4
    }
}
