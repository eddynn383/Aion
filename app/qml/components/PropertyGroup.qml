import QtQuick
import Aion

// O categorie de setări din Inspector: titlul (cu acțiuni opționale în dreapta) și câmpurile ei. Regula de spațiere
// a Inspector-ului: titlu → câmpuri = Theme.space1 + 2 (aproape), între categorii = Theme.space4 (spacing-ul
// părintelui), ca grupurile să se citească separat.
Column {
    id: root

    property string title: ""
    // Butoanele din dreapta titlului (IconButton-uri de 20 px).
    property alias actions: actionsRow.data

    width: parent ? parent.width : 0
    spacing: Theme.space1 + 2

    Item {
        width: root.width
        height: Math.max(titleText.implicitHeight, actionsRow.height)
        Text {
            id: titleText
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width - actionsRow.width - Theme.space2
            elide: Text.ElideRight
            text: root.title
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeSm
        }
        Row {
            id: actionsRow
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
