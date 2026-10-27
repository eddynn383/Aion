import QtQuick
import Aion

// Obiectele modelului, ca Outliner-ul din Blender: arborele grupurilor (foldere) și obiectelor, fiecare cu iconița
// tipului (cub, sferă, con, cilindru, formă liberă, componentă), cu ochi și lacăt; drag & drop mută un obiect în alt
// grup (sau, pe zona goală de jos, înapoi în model). La final: camerele și luminile.
Item {
    id: root

    property var document
    property var viewport // pentru camerele noi (vederea curentă)
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
        if (root.document) {
            const selected = root.document.selectedObject
            for (const o of root.document.cameras.concat(root.document.lights))
                out.push(Object.assign({}, o, { lightKind: o.kind, kind: o.kind === "camera" ? "camera" : "light", depth: 0,
                    hasChildren: false, copies: 1, hidden: !o.visible, locked: false,
                    selected: selected.id === o.id && selected.kind === (o.kind === "camera" ? "camera" : "light") }))
        }
        return out
    }

    readonly property var shapeIcons: ({ box: "prim-cube", cylinder: "prim-cylinder", cone: "prim-cone", sphere: "prim-sphere" })
    readonly property var lightIcons: ({ point: "lightbulb", spot: "light-spot", directional: "sun", area: "light-area" })
    function iconFor(r) {
        if (r.kind === "camera") return "camera"
        if (r.kind === "light") return root.lightIcons[r.lightKind] ?? "lightbulb"
        if (r.kind === "group") return "folder"
        if (r.copies > 1) return "component"
        return root.shapeIcons[r.shape] ?? "shapes"
    }

    // Camerele noi privesc ca vederea curentă; luminile vin deasupra punctului privit.
    function addObject(kind) {
        if (kind === "camera") {
            root.document.addCamera(root.viewport.cameraState(), 0)
        } else {
            const c = root.viewport.cameraState()
            root.document.addLight(kind, Qt.vector3d(c.tx, c.ty, c.tz))
        }
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
        height: parent.height
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
            readonly property bool isSceneObject: modelData.kind === "camera" || modelData.kind === "light"

            width: ListView.view.width
            height: Theme.controlHeightSm
            radius: Theme.radiusSm
            color: dropArea.containsDrag && row.isGroup ? Theme.accentSubtle
                 : modelData.selected ? Theme.accentSubtle
                 : rowHover.hovered ? Theme.bgHover : "transparent"
            border.color: dropArea.containsDrag && row.isGroup ? Theme.accent : "transparent"

            // Trasul rândului (drag & drop între grupuri).
            Drag.active: dragHandler.active
            Drag.keys: ["aion-instance"]
            Drag.hotSpot.x: 20
            Drag.hotSpot.y: height / 2
            DragHandler {
                id: dragHandler
                target: null
                enabled: !row.isSceneObject
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
                onClicked: row.isSceneObject ? root.document.selectObject(row.modelData.kind, row.modelData.id)
                                             : root.document.selectInstance(row.modelData.id)
                onDoubleClicked: {
                    if (row.modelData.kind === "camera" && root.viewport)
                        root.viewport.animateToCamera(root.document.cameraView(row.modelData.id), 0.8)
                    else if (!row.isSceneObject)
                        row.editing = true
                }
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
                    name: root.iconFor(row.modelData)
                    size: Theme.iconSizeSm
                    color: row.modelData.current ? Theme.accent
                         : row.modelData.kind === "light" ? row.modelData.color
                         : row.isGroup ? Theme.textPrimary : Theme.textSecondary
                    opacity: row.modelData.hidden || row.modelData.enabled === false ? 0.45 : 1
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

            // Acțiunile rândului (RowAction: locuri fixe, se estompează): o comandă sau o stare, apoi ochiul.
            HoverHandler { id: rowHover }
            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                // Camera: privește prin ea.
                RowAction {
                    visible: row.modelData.kind === "camera"
                    shown: rowHover.hovered
                    iconName: "video"
                    tooltip: "Privește prin cameră"
                    onClicked: root.viewport.animateToCamera(root.document.cameraView(row.modelData.id), 0.8)
                }
                // Lumina: aprinsă / stinsă (stinsă se vede mereu).
                RowAction {
                    visible: row.modelData.kind === "light"
                    shown: rowHover.hovered
                    active: row.modelData.enabled === false
                    iconName: row.modelData.enabled ? "lightbulb" : "lightbulb-off"
                    tooltip: row.modelData.enabled ? "Stinge" : "Aprinde"
                    onClicked: {
                        root.document.selectObject("light", row.modelData.id)
                        root.document.setObjectProperties({ enabled: !row.modelData.enabled })
                    }
                }
                // Obiectele și grupurile: blocat (se vede mereu când e blocat).
                RowAction {
                    visible: !row.isSceneObject
                    shown: rowHover.hovered
                    active: row.modelData.locked
                    iconName: row.modelData.locked ? "lock" : "lock-open"
                    tooltip: row.modelData.locked ? "Deblochează" : "Blochează"
                    onClicked: root.document.setInstanceLocked(row.modelData.id, !row.modelData.locked)
                }
                RowAction {
                    shown: rowHover.hovered
                    active: row.modelData.hidden
                    iconName: row.modelData.hidden ? "eye-off" : "eye"
                    tooltip: row.isSceneObject ? (row.modelData.hidden ? "Arată marcajul" : "Ascunde marcajul din viewport")
                                               : (row.modelData.hidden ? "Arată" : "Ascunde")
                    onClicked: {
                        if (row.isSceneObject) {
                            root.document.selectObject(row.modelData.kind, row.modelData.id)
                            root.document.setObjectProperties({ visible: row.modelData.hidden })
                        } else {
                            root.document.setInstanceHidden(row.modelData.id, !row.modelData.hidden)
                        }
                    }
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
}
