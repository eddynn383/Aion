import QtQuick
import Forma

// Secțiune dintr-un panou: titlu mic și conținut așezat pe verticală.
Column {
    id: root

    property string title: ""
    default property alias content: body.data

    spacing: Theme.space2

    Text {
        visible: root.title.length > 0
        text: root.title
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeXs
        font.weight: Font.DemiBold
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.6
    }

    Column {
        id: body
        width: root.width
        spacing: Theme.space2
    }
}
