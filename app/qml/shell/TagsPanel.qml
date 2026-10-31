import QtQuick
import Aion

// Tag-urile: etichete opționale, transversale, care ascund, afișează și selectează elemente (fețe, muchii, obiecte)
// din grupuri diferite — de exemplu toți pereții. Dublu-click redenumește.
Item {
    id: root

    property var document

    Flickable {
        anchors.fill: parent
        contentHeight: tagsBox.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds
    Column {
        id: tagsBox
        width: parent.width
        spacing: 2

        Repeater {
            model: root.document ? root.document.tags : []
            delegate: Rectangle {
                id: tagRow
                required property var modelData
                property bool editing: false
                width: tagsBox.width
                height: Theme.controlHeightSm
                radius: Theme.radiusSm
                color: tagHover.hovered ? Theme.bgHover : "transparent"

                MouseArea {
                    id: tagArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onDoubleClicked: tagRow.editing = true
                }
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.right: tagActions.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space2
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 10
                        height: 10
                        radius: 5
                        color: tagRow.modelData.color
                        opacity: tagRow.modelData.visible ? 1 : 0.35
                    }
                    Text {
                        visible: !tagRow.editing
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.max(0, parent.width - 40)
                        text: tagRow.modelData.name + "  ·  " + tagRow.modelData.count
                        elide: Text.ElideRight
                        color: tagRow.modelData.visible ? Theme.textPrimary : Theme.textTertiary
                        font.pixelSize: Theme.fontSizeSm
                    }
                    TextInput {
                        visible: tagRow.editing
                        anchors.verticalCenter: parent.verticalCenter
                        width: 120
                        text: tagRow.modelData.name
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSm
                        onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                        onEditingFinished: {
                            if (tagRow.editing)
                                root.document.renameTag(tagRow.modelData.id, text)
                            tagRow.editing = false
                        }
                    }
                }
                // Acțiunile (RowAction: locuri fixe): șterge, atribuie, selectează, apoi ochiul.
                HoverHandler { id: tagHover }
                Row {
                    id: tagActions
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    RowAction {
                        shown: tagHover.hovered
                        iconName: "trash-2"
                        tooltip: qsTrId("TagsPanel.sterge_tag_ul")
                        onClicked: root.document.removeTag(tagRow.modelData.id)
                    }
                    RowAction {
                        shown: tagHover.hovered
                        iconName: "tag"
                        tooltip: qsTrId("TagsPanel.atribuie_selectiei")
                        onClicked: root.document.assignTag(tagRow.modelData.id)
                    }
                    RowAction {
                        shown: tagHover.hovered
                        iconName: "mouse-pointer-2"
                        tooltip: qsTrId("TagsPanel.selecteaza_ce_are_tag_ul")
                        onClicked: root.document.selectByTag(tagRow.modelData.id)
                    }
                    RowAction {
                        shown: tagHover.hovered
                        active: !tagRow.modelData.visible
                        iconName: tagRow.modelData.visible ? "eye" : "eye-off"
                        tooltip: tagRow.modelData.visible ? qsTrId("TagsPanel.ascunde_tot_ce_are_tag_ul") : qsTrId("TagsPanel.arata")
                        onClicked: root.document.setTagVisible(tagRow.modelData.id, !tagRow.modelData.visible)
                    }
                }
            }
        }
        Text {
            visible: root.document && root.document.tags.length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("TagsPanel.optionale_pune_acelasi_tag_pe_elemente_din_grupuri")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }
    }
}
