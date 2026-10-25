import QtQuick
import Forma

// Panou lateral cu titlu. Conținutul se pune ca elemente copil.
Rectangle {
    id: root

    property string title: ""
    default property alias content: body.data

    color: Theme.bgPanel

    Text {
        id: header
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: Theme.space4
        text: root.title
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSm
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }

    Item {
        id: body
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space4
    }
}
