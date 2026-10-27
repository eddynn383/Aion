import QtQuick
import Aion

// Editorul de interacțiuni cu blocuri (ca Scratch / Sketchware): în stânga paleta pe categorii, în dreapta
// regulile obiectului — un bloc-eveniment („Când dau click”) urmat de blocurile de acțiune care se execută pe rând.
// Blocurile se trag din paletă în regulă, se mută între ele și se șterg trăgându-le înapoi pe paletă.
Popover {
    id: root

    property var document
    property double instanceId: 0
    property string instanceName: ""
    property var rules: []
    property var dragPayload: null
    property point ghostPos: Qt.point(0, 0)
    property string ghostLabel: ""
    property color ghostColor: "gray"

    readonly property color canvasColor: Theme.bgElevated

    modal: true
    anchors.centerIn: parent
    width: Math.min(1100, (parent ? parent.width : 1100) - 80)
    height: Math.min(720, (parent ? parent.height : 720) - 80)
    padding: Theme.space6

    function openFor(id, name) {
        root.instanceId = id
        root.instanceName = name
        root.rules = root.document.rulesOf(id)
        open()
    }

    // --- Blocurile ---
    readonly property var categories: [
        { name: "Evenimente", color: "#D97706", blocks: [
            { trigger: "click", label: "Când dau click" }, { trigger: "doubleClick", label: "Când dau dublu-click" },
            { trigger: "rightClick", label: "Când dau click dreapta" }, { trigger: "longPress", label: "Când țin apăsat" },
            { trigger: "hoverEnter", label: "Când cursorul intră" }, { trigger: "hoverLeave", label: "Când cursorul iese" },
            { trigger: "drag", label: "Când trag obiectul" }, { trigger: "key", label: "Când apăs tasta" } ] },
        { name: "Mișcare", color: "#2563EB", blocks: [ { kind: "moveBy", label: "Mută cu" }, { kind: "moveTo", label: "Mută la" }, { kind: "rotateBy" }, { kind: "scaleTo" } ] },
        { name: "Aspect", color: "#7C3AED", blocks: [ { kind: "show" }, { kind: "hide" }, { kind: "toggleVisible" }, { kind: "setMaterial" } ] },
        { name: "Animație și scene", color: "#DB2777", blocks: [ { kind: "playAnimation" }, { kind: "goToScene" } ] },
        { name: "Control", color: "#EA580C", blocks: [ { kind: "wait" }, { kind: "alternate" } ] }
    ]
    readonly property var triggerOptions: categories[0].blocks.map(b => ({ value: b.trigger, label: b.label }))

    function blockInfo(kind) {
        const mm = { field: "duration", label: "în", unit: "s" }
        switch (kind) {
        case "moveBy": return { label: "Mută", color: "#2563EB", target: true,
            fields: [{ field: "x", label: "cu X" }, { field: "y", label: "Y" }, { field: "z", label: "Z", unit: "mm" }, mm] }
        case "moveTo": return { label: "Mută", color: "#2563EB", target: true,
            fields: [{ field: "x", label: "la X" }, { field: "y", label: "Y" }, { field: "z", label: "Z", unit: "mm" }, mm] }
        case "rotateBy": return { label: "Rotește", color: "#2563EB", target: true, fields: [{ field: "z", label: "cu", unit: "°" }, mm] }
        case "scaleTo": return { label: "Scalează", color: "#2563EB", target: true, fields: [{ field: "x", label: "la", unit: "×" }, mm] }
        case "show": return { label: "Arată", color: "#7C3AED", target: true }
        case "hide": return { label: "Ascunde", color: "#7C3AED", target: true }
        case "toggleVisible": return { label: "Comută vizibilitatea", color: "#7C3AED", target: true }
        case "setMaterial": return { label: "Schimbă materialul", color: "#7C3AED", target: true, pick: "material" }
        case "playAnimation": return { label: "Pornește animația", color: "#DB2777", pick: "animation" }
        case "goToScene": return { label: "Mergi la scena", color: "#DB2777", pick: "scene" }
        case "wait": return { label: "Așteaptă", color: "#EA580C", fields: [{ field: "duration", label: "", unit: "s" }] }
        case "alternate": return { label: "Alternează", color: "#EA580C" }
        }
        return { label: kind, color: "gray" }
    }

    function newAction(kind) {
        const first = (list) => list.length > 0 ? list[0].id : 0
        const a = { kind: kind, target: 0, x: 0, y: 0, z: 0, duration: 0.5, material: 0, animation: 0, scene: 0, first: [], second: [] }
        if (kind === "moveBy") a.z = 500
        if (kind === "rotateBy") a.z = 90
        if (kind === "scaleTo") a.x = 1.5
        if (kind === "wait") a.duration = 1
        if (kind === "setMaterial") a.material = first(root.document.modelMaterials)
        if (kind === "playAnimation") a.animation = first(root.document.animations)
        if (kind === "goToScene") a.scene = first(root.document.scenes)
        return a
    }

    readonly property var targetOptions: [{ value: 0, label: "acest obiect" }].concat(
        (root.document ? root.document.outline : []).map(o => ({ value: o.id, label: o.name })))
    function optionsFor(field) {
        const list = field === "material" ? root.document.modelMaterials
                   : field === "animation" ? root.document.animations : root.document.scenes
        const options = list.map(o => ({ value: o.id, label: o.name }))
        return options.length > 0 ? options : [{ value: 0, label: field === "material" ? "(niciun material în model)"
                                                       : field === "animation" ? "(nicio animație)" : "(nicio scenă)" }]
    }

    // --- Editarea (căi: [regulă, index, "first"|"second", index, …]) ---
    function copy() { return JSON.parse(JSON.stringify(root.rules)) }
    function listAt(rules, listPath) {
        let list = rules[listPath[0]].actions
        for (let i = 1; i + 1 < listPath.length; i += 2)
            list = list[listPath[i]][listPath[i + 1]]
        return list
    }
    function commit(rules) {
        root.document.setRules(root.instanceId, rules)
        root.rules = root.document.rulesOf(root.instanceId)
    }
    function setField(path, field, value) {
        const rules = copy()
        listAt(rules, path.slice(0, -1))[path[path.length - 1]][field] = value
        commit(rules)
    }
    function setRuleField(ruleIndex, field, value) {
        const rules = copy()
        rules[ruleIndex][field] = value
        commit(rules)
    }
    function removeRule(ruleIndex) {
        const rules = copy()
        rules.splice(ruleIndex, 1)
        commit(rules)
    }
    function addRule(trigger) {
        const rules = copy()
        rules.push({ trigger: trigger, key: trigger === "key" ? "Space" : "", actions: [] })
        commit(rules)
    }
    function dropAt(listPath, index) {
        const payload = root.dragPayload
        if (!payload)
            return
        if (payload.type === "event") {
            addRule(payload.trigger)
            return
        }
        const rules = copy()
        let item
        if (payload.type === "move") {
            const src = payload.path
            // Nu se poate pune un bloc în el însuși.
            if (listPath.length >= src.length && src.every((v, i) => listPath[i] === v))
                return
            const srcListPath = src.slice(0, -1)
            item = listAt(rules, srcListPath).splice(src[src.length - 1], 1)[0]
            if (JSON.stringify(srcListPath) === JSON.stringify(listPath) && src[src.length - 1] < index)
                index -= 1
        } else {
            item = newAction(payload.kind)
        }
        listAt(rules, listPath).splice(index, 0, item)
        commit(rules)
    }
    function deleteDragged() {
        const payload = root.dragPayload
        if (!payload || payload.type !== "move")
            return
        const rules = copy()
        const src = payload.path
        listAt(rules, src.slice(0, -1)).splice(src[src.length - 1], 1)
        commit(rules)
    }

    // --- Tragerea (o „fantomă” urmărește cursorul și poartă drag-ul) ---
    function beginDrag(payload, label, color) {
        root.dragPayload = payload
        root.ghostLabel = label
        root.ghostColor = color
    }
    function moveDrag(item, position) {
        root.ghostPos = item.mapToItem(root.contentItem, position.x, position.y)
    }
    function endDrag() {
        ghost.Drag.drop()
        root.dragPayload = null
    }

    // O listă de opțiuni într-o „pastilă” albă (ținta, materialul, animația, scena, evenimentul).
    property Component pickComponent: Component {
        Rectangle {
            id: pick
            property var options: []
            property var value: 0
            signal picked(var value)
            readonly property var current: options.find(o => o.value === value) ?? options[0]
            width: Math.min(180, pickText.implicitWidth + 30)
            height: 24
            radius: 12
            color: Qt.rgba(1, 1, 1, 0.92)
            Text {
                id: pickText
                anchors.left: parent.left
                anchors.leftMargin: 10
                anchors.right: parent.right
                anchors.rightMargin: 18
                anchors.verticalCenter: parent.verticalCenter
                text: pick.current ? pick.current.label : ""
                elide: Text.ElideRight
                color: "#111111"
                font.pixelSize: Theme.fontSizeXs
            }
            Icon {
                anchors.right: parent.right
                anchors.rightMargin: 6
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron-down"
                size: 10
                color: "#444444"
            }
            MouseArea {
                anchors.fill: parent
                onClicked: list.opened ? list.close() : list.open()
            }
            Popover {
                id: list
                y: parent.height + 2
                width: 220
                padding: Theme.space1
                contentItem: ListView {
                    implicitHeight: Math.min(260, contentHeight)
                    clip: true
                    model: pick.options
                    delegate: Rectangle {
                        required property var modelData
                        width: 212
                        height: Theme.controlHeightSm
                        radius: Theme.radiusSm
                        color: optionArea.containsMouse ? Theme.bgHover : "transparent"
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: Theme.space2
                            anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label
                            color: Theme.textPrimary
                            font.pixelSize: Theme.fontSizeSm
                        }
                        MouseArea {
                            id: optionArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: { pick.picked(modelData.value); list.close() }
                        }
                    }
                }
            }
        }
    }

    contentItem: Item {
        // Antetul.
        Row {
            id: header
            width: parent.width
            Text {
                width: parent.width - closeButton.width
                anchors.verticalCenter: parent.verticalCenter
                text: "Interacțiuni · " + root.instanceName
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
            }
            IconButton {
                id: closeButton
                iconName: "x"
                tooltip: "Închide"
                onClicked: root.close()
            }
        }
        Text {
            id: hint
            anchors.top: header.bottom
            anchors.topMargin: Theme.space1
            width: parent.width
            wrapMode: Text.Wrap
            text: "Trage un eveniment în dreapta ca să începi o regulă, apoi trage acțiunile sub el. Rulează în Modul "
                  + "interacțiune (F5). Pentru a șterge un bloc, trage-l înapoi pe paletă."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }

        // Paleta (și coșul de gunoi).
        Rectangle {
            id: palette
            anchors.top: hint.bottom
            anchors.topMargin: Theme.space3
            anchors.bottom: parent.bottom
            width: 250
            radius: Theme.radiusMd
            color: trash.containsDrag ? Qt.rgba(0.9, 0.2, 0.2, 0.15) : Theme.bgHover
            border.color: trash.containsDrag ? Theme.danger : "transparent"

            DropArea {
                id: trash
                anchors.fill: parent
                keys: ["aion-block"]
                onDropped: (drop) => { root.deleteDragged(); drop.accept() }
            }
            Flickable {
                anchors.fill: parent
                anchors.margins: Theme.space2
                contentHeight: paletteColumn.height
                clip: true
                Column {
                    id: paletteColumn
                    width: parent.width
                    spacing: Theme.space2
                    Repeater {
                        model: root.categories
                        delegate: Column {
                            id: category
                            required property var modelData
                            width: paletteColumn.width
                            spacing: 4
                            Text {
                                text: category.modelData.name
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSizeXs
                                font.weight: Font.DemiBold
                            }
                            Repeater {
                                model: category.modelData.blocks
                                delegate: Rectangle {
                                    id: paletteBlock
                                    required property var modelData
                                    readonly property string label: modelData.label ?? root.blockInfo(modelData.kind).label
                                    width: category.width
                                    height: 30
                                    radius: modelData.trigger ? 14 : Theme.radiusMd
                                    color: category.modelData.color
                                    Text {
                                        anchors.left: parent.left
                                        anchors.leftMargin: Theme.space3
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: paletteBlock.label
                                        color: "white"
                                        font.pixelSize: Theme.fontSizeSm
                                        font.weight: Font.Medium
                                    }
                                    DragHandler {
                                        target: null
                                        onActiveChanged: active
                                            ? root.beginDrag(paletteBlock.modelData.trigger
                                                    ? { type: "event", trigger: paletteBlock.modelData.trigger }
                                                    : { type: "new", kind: paletteBlock.modelData.kind },
                                                paletteBlock.label, category.modelData.color)
                                            : root.endDrag()
                                        onCentroidChanged: if (active) root.moveDrag(paletteBlock, centroid.position)
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Regulile.
        Rectangle {
            id: canvas
            anchors.top: hint.bottom
            anchors.topMargin: Theme.space3
            anchors.left: palette.right
            anchors.leftMargin: Theme.space4
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            radius: Theme.radiusMd
            color: root.canvasColor
            border.color: Theme.borderSubtle

            // Un eveniment lăsat oriunde pe zonă începe o regulă nouă.
            DropArea {
                anchors.fill: parent
                keys: ["aion-block"]
                onDropped: (drop) => {
                    if (root.dragPayload && root.dragPayload.type === "event") {
                        root.addRule(root.dragPayload.trigger)
                        drop.accept()
                    }
                }
            }

            Flickable {
                anchors.fill: parent
                anchors.margins: Theme.space4
                contentHeight: rulesColumn.height
                clip: true
                Column {
                    id: rulesColumn
                    width: parent.width
                    spacing: Theme.space6
                    Repeater {
                        model: root.rules
                        delegate: Column {
                            id: ruleColumn
                            required property var modelData
                            required property int index
                            width: Math.min(rulesColumn.width, 560)
                            spacing: 0
                            // Blocul-eveniment („pălăria”).
                            Rectangle {
                                width: parent.width
                                height: 40
                                radius: 18
                                color: "#D97706"
                                Rectangle { x: 18; y: parent.height - 1; width: 22; height: 6; radius: 2; color: "#D97706" }
                                Row {
                                    anchors.left: parent.left
                                    anchors.leftMargin: Theme.space3
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: Theme.space2
                                    Loader {
                                        anchors.verticalCenter: parent.verticalCenter
                                        sourceComponent: root.pickComponent
                                        onLoaded: {
                                            item.options = root.triggerOptions
                                            item.value = Qt.binding(() => ruleColumn.modelData.trigger)
                                            item.picked.connect((v) => root.setRuleField(ruleColumn.index, "trigger", v))
                                        }
                                    }
                                    Rectangle {
                                        visible: ruleColumn.modelData.trigger === "key"
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: 70
                                        height: 24
                                        radius: 12
                                        color: Qt.rgba(1, 1, 1, 0.92)
                                        TextInput {
                                            anchors.centerIn: parent
                                            text: ruleColumn.modelData.key
                                            color: "#111111"
                                            font.pixelSize: Theme.fontSizeXs
                                            onEditingFinished: root.setRuleField(ruleColumn.index, "key", text)
                                        }
                                    }
                                }
                                IconButton {
                                    anchors.right: parent.right
                                    anchors.rightMargin: Theme.space2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 26; height: 26; iconSize: 13
                                    iconName: "trash-2"
                                    tooltip: "Șterge regula"
                                    onClicked: root.removeRule(ruleColumn.index)
                                }
                            }
                            RulesActionList {
                                width: parent.width
                                editor: root
                                listPath: [ruleColumn.index]
                                actions: ruleColumn.modelData.actions
                            }
                        }
                    }
                    Text {
                        visible: root.rules.length === 0
                        width: rulesColumn.width
                        topPadding: Theme.space8
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.Wrap
                        text: "Nicio regulă. Trage un eveniment galben (de exemplu „Când dau click”) aici."
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeSm
                    }
                }
            }
        }

        // Fantoma blocului tras.
        Rectangle {
            id: ghost
            visible: root.dragPayload !== null
            x: root.ghostPos.x - 20
            y: root.ghostPos.y - 12
            z: 100
            width: ghostText.implicitWidth + Theme.space3 * 2
            height: 28
            radius: Theme.radiusMd
            color: root.ghostColor
            opacity: 0.9
            Drag.active: root.dragPayload !== null
            Drag.keys: ["aion-block"]
            Drag.hotSpot.x: 20
            Drag.hotSpot.y: 12
            Text {
                id: ghostText
                anchors.centerIn: parent
                text: root.ghostLabel
                color: "white"
                font.pixelSize: Theme.fontSizeSm
            }
        }
    }
}
