import QtQuick
import Aion

// Tag-urile (layerele): vizibilitate, culoare, redenumire (dublu-click), ștergere și adăugare.
Item {
    id: root

    property var document

    Column {
        id: header
        width: parent.width
        spacing: Theme.space2

        Row {
            width: parent.width
            spacing: Theme.space2

            Input {
                id: newTag
                width: parent.width - addButton.width - Theme.space2
                placeholderText: "Tag nou"
                onAccepted: root.add()
            }
            IconButton {
                id: addButton
                anchors.verticalCenter: parent.verticalCenter
                iconName: "plus"
                iconSize: Theme.iconSizeSm
                tooltip: "Adaugă tag"
                enabled: newTag.text.trim().length > 0
                onClicked: root.add()
            }
        }
    }

    function add() {
        const name = newTag.text.trim()
        if (name.length === 0)
            return
        root.document.addTag(name)
        newTag.text = ""
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: Theme.space3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        spacing: 1
        model: root.document ? root.document.tags : []
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            id: row

            required property var modelData
            property bool editing: false

            width: ListView.view.width
            height: Theme.controlHeightSm
            radius: Theme.radiusSm
            color: rowArea.containsMouse ? Theme.bgHover : "transparent"

            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                onDoubleClicked: row.editing = true
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.right: actions.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space2

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 10
                    height: 10
                    radius: 5
                    color: row.modelData.color
                    opacity: row.modelData.visible ? 1 : 0.35
                }
                Text {
                    visible: !row.editing
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 10 - Theme.space2
                    text: row.modelData.name + (row.modelData.count > 0 ? "  ·  " + row.modelData.count : "")
                    elide: Text.ElideRight
                    color: row.modelData.visible ? Theme.textPrimary : Theme.textTertiary
                    font.pixelSize: Theme.fontSizeSm
                }
                TextInput {
                    visible: row.editing
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 10 - Theme.space2
                    text: row.modelData.name
                    color: Theme.textPrimary
                    selectionColor: Theme.accentSubtle
                    font.pixelSize: Theme.fontSizeSm
                    onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                    onEditingFinished: {
                        if (row.editing)
                            root.document.renameTag(row.modelData.id, text)
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

                IconButton {
                    visible: rowArea.containsMouse || hovered
                    width: 24
                    height: 24
                    iconSize: Theme.iconSizeSm
                    iconName: "trash-2"
                    tooltip: "Șterge tag-ul"
                    onClicked: root.document.removeTag(row.modelData.id)
                }
                IconButton {
                    width: 24
                    height: 24
                    iconSize: Theme.iconSizeSm
                    iconName: row.modelData.visible ? "eye" : "eye-off"
                    tooltip: row.modelData.visible ? "Ascunde tag-ul" : "Arată tag-ul"
                    onClicked: root.document.setTagVisible(row.modelData.id, !row.modelData.visible)
                }
            }
        }
    }

    Text {
        visible: list.count === 0
        anchors.top: header.bottom
        anchors.topMargin: Theme.space3
        width: parent.width
        wrapMode: Text.Wrap
        text: "Niciun tag. Tag-urile grupează obiectele ca să le ascunzi sau să le arăți împreună."
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm
    }
}
