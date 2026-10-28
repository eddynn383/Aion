import QtQuick
import Aion

// Un grup de setări (Preferințe): titlul mic deasupra și rândurile (SettingsRow) într-un card, despărțite de linii.
Column {
    id: root

    property string title: ""
    default property alias rows: body.data

    spacing: Theme.space2

    Text {
        visible: root.title.length > 0
        leftPadding: Theme.space1
        text: root.title.toUpperCase()
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
        font.weight: Font.DemiBold
        font.letterSpacing: 0.6
    }

    Rectangle {
        width: root.width
        height: body.height
        radius: Theme.radiusMd
        color: Theme.bgInput
        border.color: Theme.borderSubtle

        Column {
            id: body
            width: parent.width
        }
    }
}
