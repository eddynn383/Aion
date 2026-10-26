import QtQuick
import Aion

// Outliner: arborele grupurilor și obiectelor. Click selectează (intrând, la nevoie, în grupul care conține
// obiectul), dublu-click pe nume redenumește, ochiul ascunde sau arată.
Item {
    id: root

    property var document

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        model: root.document ? root.document.outline : []
        spacing: 1
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            id: row

            required property var modelData
            property bool editing: false

            width: ListView.view.width
            height: Theme.controlHeightSm
            radius: Theme.radiusSm
            color: modelData.selected ? Theme.accentSubtle : rowArea.containsMouse ? Theme.bgHover : "transparent"

            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.document.selectInstance(row.modelData.id)
                onDoubleClicked: row.editing = true
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2 + row.modelData.depth * 14
                anchors.right: actions.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space2

                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: row.modelData.kind === "component" ? "component" : "box"
                    size: Theme.iconSizeSm
                    color: row.modelData.current ? Theme.accent : Theme.textSecondary
                    opacity: row.modelData.hidden ? 0.45 : 1
                }
                Text {
                    visible: !row.editing
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - Theme.iconSizeSm - Theme.space2
                    text: row.modelData.name
                    elide: Text.ElideRight
                    color: row.modelData.hidden ? Theme.textTertiary : Theme.textPrimary
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: row.modelData.current ? Font.DemiBold : Font.Normal
                }
                TextInput {
                    visible: row.editing
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - Theme.iconSizeSm - Theme.space2
                    text: row.modelData.name
                    color: Theme.textPrimary
                    selectionColor: Theme.accentSubtle
                    font.pixelSize: Theme.fontSizeSm
                    onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                    onEditingFinished: {
                        if (row.editing)
                            root.document.renameInstance(row.modelData.id, text)
                        row.editing = false
                    }
                    Keys.onEscapePressed: row.editing = false
                }
            }

            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: Theme.space1
                anchors.verticalCenter: parent.verticalCenter

                Icon {
                    visible: row.modelData.locked
                    anchors.verticalCenter: parent.verticalCenter
                    name: "lock"
                    size: 12
                    color: Theme.textTertiary
                }
                IconButton {
                    visible: rowArea.containsMouse || row.modelData.hidden || hovered
                    width: 24
                    height: 24
                    iconSize: Theme.iconSizeSm
                    iconName: row.modelData.hidden ? "eye-off" : "eye"
                    tooltip: row.modelData.hidden ? "Arată" : "Ascunde"
                    onClicked: root.document.setInstanceHidden(row.modelData.id, !row.modelData.hidden)
                }
            }
        }
    }

    Text {
        visible: list.count === 0
        width: parent.width
        wrapMode: Text.Wrap
        text: "Niciun grup sau obiect. Selectează geometrie și alege „Creează grup” (⌘G) sau „Creează obiect” (⇧⌘G); le găsești și în meniul de click dreapta."
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm
    }
}
