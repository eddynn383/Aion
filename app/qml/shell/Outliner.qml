import QtQuick
import Aion

// Outliner, ca în Blender: arborele grupurilor (foldere) și obiectelor, cu ochi și lacăt; drag & drop mută un obiect
// în alt grup (sau, pe zona goală de jos, înapoi în model). Dedesubt: tag-urile — etichete opționale, transversale,
// care ascund, afișează și selectează elemente din grupuri diferite.
Item {
    id: root

    property var document
    property var collapsed: ({}) // id-ul instanței → true dacă e strânsă
    property double dragging: 0 // instanța trasă

    // Rândurile vizibile: fără cele din grupurile strânse.
    readonly property var rows: {
        const out = []
        let hiddenBelow = -1
        for (const r of (root.document ? root.document.outline : [])) {
            if (hiddenBelow >= 0 && r.depth > hiddenBelow)
                continue
            hiddenBelow = root.collapsed[r.id] ? r.depth : -1
            out.push(r)
        }
        return out
    }

    function toggle(id) {
        const next = Object.assign({}, root.collapsed)
        if (next[id]) delete next[id]
        else next[id] = true
        root.collapsed = next
    }

    // --- Arborele ---
    ListView {
        id: tree
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height - tagsBox.height - Theme.space3
        clip: true
        model: root.rows
        spacing: 1
        boundsBehavior: Flickable.StopAtBounds

        // Zona goală de sub arbore: drop aici scoate obiectul din grup (îl pune în model).
        footer: Item {
            width: tree.width
            // Restul spațiului de sub rânduri (calculat din numărul lor, nu din contentHeight, care include footer-ul).
            height: Math.max(48, tree.height - tree.count * (Theme.controlHeightSm + 1))
            Rectangle {
                anchors.fill: parent
                anchors.margins: 2
                radius: Theme.radiusSm
                color: rootDrop.containsDrag ? Theme.accentSubtle : "transparent"
                border.color: rootDrop.containsDrag ? Theme.accent : "transparent"
                Text {
                    visible: rootDrop.containsDrag
                    anchors.centerIn: parent
                    text: "Scoate din grup"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeXs
                }
            }
            DropArea {
                id: rootDrop
                anchors.fill: parent
                keys: ["aion-instance"]
                onDropped: (drop) => { root.document.moveInstanceTo(root.dragging, 0); drop.accept() }
            }
        }

        delegate: Rectangle {
            id: row

            required property var modelData
            property bool editing: false
            readonly property bool isGroup: modelData.kind === "group"

            width: ListView.view.width
            height: Theme.controlHeightSm
            radius: Theme.radiusSm
            color: dropArea.containsDrag && row.isGroup ? Theme.accentSubtle
                 : modelData.selected ? Theme.accentSubtle
                 : rowArea.containsMouse ? Theme.bgHover : "transparent"
            border.color: dropArea.containsDrag && row.isGroup ? Theme.accent : "transparent"

            // Trasul rândului (drag & drop între grupuri).
            Drag.active: dragHandler.active
            Drag.keys: ["aion-instance"]
            Drag.hotSpot.x: 20
            Drag.hotSpot.y: height / 2
            DragHandler {
                id: dragHandler
                target: null
                onActiveChanged: {
                    if (active) {
                        root.dragging = row.modelData.id
                    } else {
                        row.Drag.drop()
                        root.dragging = 0
                    }
                }
            }
            DropArea {
                id: dropArea
                anchors.fill: parent
                keys: ["aion-instance"]
                enabled: row.isGroup && root.dragging !== row.modelData.id
                onDropped: (drop) => { root.document.moveInstanceTo(root.dragging, row.modelData.id); drop.accept() }
            }

            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.document.selectInstance(row.modelData.id)
                onDoubleClicked: row.editing = true
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space1 + row.modelData.depth * 14
                anchors.right: actions.left
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space1

                // Săgeata de strângere / desfacere (doar dacă are conținut).
                Item {
                    width: 16
                    height: 16
                    anchors.verticalCenter: parent.verticalCenter
                    Icon {
                        visible: row.modelData.hasChildren
                        anchors.centerIn: parent
                        name: "chevron-right"
                        size: 12
                        color: Theme.textTertiary
                        rotation: root.collapsed[row.modelData.id] ? 0 : 90
                        Behavior on rotation { NumberAnimation { duration: Theme.motionFast } }
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: row.modelData.hasChildren
                        onClicked: root.toggle(row.modelData.id)
                    }
                }
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: row.isGroup ? "folder" : row.modelData.copies > 1 ? "component" : "box"
                    size: Theme.iconSizeSm
                    color: row.modelData.current ? Theme.accent : row.isGroup ? Theme.textPrimary : Theme.textSecondary
                    opacity: row.modelData.hidden ? 0.45 : 1
                }
                Text {
                    visible: !row.editing
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.max(0, parent.width - 16 - Theme.iconSizeSm - Theme.space1 * 3 - copiesText.width - tagDot.width)
                    text: row.modelData.name
                    elide: Text.ElideRight
                    color: row.modelData.hidden ? Theme.textTertiary : Theme.textPrimary
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: row.isGroup || row.modelData.current ? Font.DemiBold : Font.Normal
                }
                Text {
                    id: copiesText
                    visible: !row.editing && row.modelData.copies > 1
                    width: visible ? implicitWidth : 0
                    anchors.verticalCenter: parent.verticalCenter
                    text: "×" + row.modelData.copies
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                }
                Rectangle {
                    id: tagDot
                    visible: row.modelData.tagColor !== undefined
                    width: visible ? 8 : 0
                    height: 8
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: row.modelData.tagColor ?? "transparent"
                }
                TextInput {
                    visible: row.editing
                    anchors.verticalCenter: parent.verticalCenter
                    width: 140
                    text: row.modelData.name
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeSm
                    onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                    onEditingFinished: {
                        if (row.editing)
                            root.document.renameInstance(row.modelData.id, text)
                        row.editing = false
                    }
                }
            }

            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                IconButton {
                    width: 22; height: 22; iconSize: 13
                    visible: rowArea.containsMouse || hovered || row.modelData.locked
                    iconName: row.modelData.locked ? "lock" : "lock-open"
                    tooltip: row.modelData.locked ? "Deblochează" : "Blochează"
                    onClicked: root.document.setInstanceLocked(row.modelData.id, !row.modelData.locked)
                }
                IconButton {
                    width: 22; height: 22; iconSize: 13
                    visible: rowArea.containsMouse || hovered || row.modelData.hidden
                    iconName: row.modelData.hidden ? "eye-off" : "eye"
                    tooltip: row.modelData.hidden ? "Arată" : "Ascunde"
                    onClicked: root.document.setInstanceHidden(row.modelData.id, !row.modelData.hidden)
                }
            }
        }
    }

    Text {
        visible: tree.count === 0
        anchors.top: parent.top
        width: parent.width
        wrapMode: Text.Wrap
        text: AppInfo.platformText("Niciun obiect. Selectează forme și apasă ⌘G ca să faci un obiect; obiectele se "
              + "grupează cu ⌥⌘G. Sau creează direct un cub, o sferă… din toolbar.")
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm
    }

    // --- Tag-urile ---
    Column {
        id: tagsBox
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 2

        Separator { width: parent.width }
        Item {
            width: parent.width
            height: Theme.controlHeightSm
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: Theme.space1
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "tag"
                    size: 13
                    color: Theme.textTertiary
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "TAG-URI"
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.6
                }
            }
            IconButton {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: 22; height: 22; iconSize: 13
                iconName: "plus"
                tooltip: "Tag nou"
                onClicked: root.document.addTag("Tag " + (root.document.tags.length + 1))
            }
        }

        Repeater {
            model: root.document ? root.document.tags : []
            delegate: Rectangle {
                id: tagRow
                required property var modelData
                property bool editing: false
                width: tagsBox.width
                height: Theme.controlHeightSm
                radius: Theme.radiusSm
                color: tagArea.containsMouse ? Theme.bgHover : "transparent"

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
                Row {
                    id: tagActions
                    anchors.right: parent.right
                    anchors.rightMargin: 2
                    anchors.verticalCenter: parent.verticalCenter
                    IconButton {
                        visible: tagArea.containsMouse || hovered
                        width: 22; height: 22; iconSize: 13
                        iconName: "tag"
                        tooltip: "Atribuie selecției"
                        onClicked: root.document.assignTag(tagRow.modelData.id)
                    }
                    IconButton {
                        visible: tagArea.containsMouse || hovered
                        width: 22; height: 22; iconSize: 13
                        iconName: "mouse-pointer-2"
                        tooltip: "Selectează ce are tag-ul"
                        onClicked: root.document.selectByTag(tagRow.modelData.id)
                    }
                    IconButton {
                        visible: tagArea.containsMouse || hovered
                        width: 22; height: 22; iconSize: 13
                        iconName: "trash-2"
                        tooltip: "Șterge tag-ul"
                        onClicked: root.document.removeTag(tagRow.modelData.id)
                    }
                    IconButton {
                        width: 22; height: 22; iconSize: 13
                        iconName: tagRow.modelData.visible ? "eye" : "eye-off"
                        tooltip: tagRow.modelData.visible ? "Ascunde tot ce are tag-ul" : "Arată"
                        onClicked: root.document.setTagVisible(tagRow.modelData.id, !tagRow.modelData.visible)
                    }
                }
            }
        }
        Text {
            visible: root.document && root.document.tags.length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: "Opționale: pune același tag pe elemente din grupuri diferite (de exemplu toți pereții) ca să le ascunzi sau selectezi împreună."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }
}
