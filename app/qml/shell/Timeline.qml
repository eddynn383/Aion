import QtQuick
import Aion

// Timeline-ul (ca în Figma / Blender): animațiile modelului, cu keyframe-uri pe obiecte (poziție, rotație, mărime,
// culoare) și pe cameră. Romburile se trag pentru a muta cheia în timp, click dreapta alege easing-ul, Delete o
// șterge. Click pe riglă sau pe piste mută playhead-ul și arată animația; un click de editare în viewport revine
// la model.
Rectangle {
    id: root

    property var document
    property var viewport

    signal closeRequested()
    signal exportRequested()

    readonly property var tl: document ? document.timeline : ({})
    readonly property bool hasAnimation: document && document.activeAnimation > 0
    readonly property real duration: tl.duration ?? 5
    readonly property real namesWidth: 170
    // Pe un timeline îngust, butoanele de keyframe rămân doar cu iconiță (și tooltip).
    readonly property bool wide: width > 1080
    readonly property real pxPerSecond: Math.max(10, (lanes.width - 16) / Math.max(duration, 0.1))
    property var selectedKey: null // { kind, instance, time }

    function timeAt(x) { return Math.max(0, Math.min(root.duration, (x - 8) / root.pxPerSecond)) }
    function xAt(t) { return 8 + t * root.pxPerSecond }
    function fmt(t) { return Number(t).toLocaleString(Qt.locale("ro_RO"), "f", 2) }

    color: Theme.bgPanel
    focus: true

    Keys.onDeletePressed: deleteSelected()
    Keys.onPressed: (event) => {
        if (event.key === Qt.Key_Backspace) {
            deleteSelected()
            event.accepted = true
        }
    }
    Keys.onSpacePressed: root.document.playing ? root.document.pause() : root.document.play()
    function deleteSelected() {
        if (root.selectedKey) {
            root.document.removeKey(root.selectedKey.kind, root.selectedKey.instance, root.selectedKey.time)
            root.selectedKey = null
        }
    }

    Separator { width: parent.width; anchors.top: parent.top }

    // --- Antetul: animația, redarea, keyframe-urile, setările ---
    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: Theme.space2
        height: Theme.controlHeight

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space2

            // Alegerea animației.
            Rectangle {
                width: 180
                height: Theme.controlHeightSm
                radius: Theme.radiusMd
                color: animArea.containsMouse || animPicker.opened ? Theme.bgHover : Theme.bgInput
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.right: animChevron.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.hasAnimation ? (root.tl.name ?? "") : "Nicio animație"
                    elide: Text.ElideRight
                    color: root.hasAnimation ? Theme.textPrimary : Theme.textTertiary
                    font.pixelSize: Theme.fontSizeSm
                }
                Icon {
                    id: animChevron
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    name: "chevron-down"
                    size: 12
                    color: Theme.textTertiary
                }
                MouseArea {
                    id: animArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: animPicker.opened ? animPicker.close() : animPicker.open()
                }
                Popover {
                    id: animPicker
                    y: -height - Theme.space1
                    width: 240
                    padding: Theme.space2
                    contentItem: Column {
                        spacing: 1
                        Repeater {
                            model: root.document ? root.document.animations : []
                            delegate: Rectangle {
                                id: animRow
                                required property var modelData
                                property bool editing: false
                                width: 224
                                height: Theme.controlHeightSm
                                radius: Theme.radiusSm
                                color: animRowArea.containsMouse ? Theme.bgHover
                                     : root.document.activeAnimation === modelData.id ? Theme.accentSubtle : "transparent"
                                MouseArea {
                                    id: animRowArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onClicked: { root.document.activeAnimation = animRow.modelData.id; animPicker.close() }
                                    onDoubleClicked: animRow.editing = true
                                }
                                Text {
                                    visible: !animRow.editing
                                    anchors.left: parent.left
                                    anchors.leftMargin: Theme.space2
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: animRow.modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeSm
                                }
                                TextInput {
                                    visible: animRow.editing
                                    anchors.left: parent.left
                                    anchors.leftMargin: Theme.space2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 160
                                    text: animRow.modelData.name
                                    color: Theme.textPrimary
                                    font.pixelSize: Theme.fontSizeSm
                                    onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                                    onEditingFinished: {
                                        if (animRow.editing)
                                            root.document.renameAnimation(animRow.modelData.id, text)
                                        animRow.editing = false
                                    }
                                }
                                IconButton {
                                    anchors.right: parent.right
                                    anchors.verticalCenter: parent.verticalCenter
                                    visible: animRowArea.containsMouse || hovered
                                    width: 22; height: 22; iconSize: 13
                                    iconName: "trash-2"
                                    tooltip: "Șterge animația"
                                    onClicked: root.document.removeAnimation(animRow.modelData.id)
                                }
                            }
                        }
                        Button {
                            width: 224
                            text: "Animație nouă"
                            iconName: "plus"
                            onClicked: { root.document.createAnimation(); animPicker.close() }
                        }
                    }
                }
            }

            Separator { width: 1; height: 20; anchors.verticalCenter: parent.verticalCenter }

            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 28; height: 28; iconSize: Theme.iconSizeSm
                iconName: "skip-back"
                tooltip: "La început"
                enabled: root.hasAnimation
                onClicked: root.document.timelineTime = 0
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 28; height: 28; iconSize: Theme.iconSizeSm
                iconName: root.document && root.document.playing ? "pause" : "play"
                tooltip: root.document && root.document.playing ? "Pauză (Space)" : "Redă (Space)"
                enabled: root.hasAnimation
                checked: root.document && root.document.playing
                onClicked: root.document.playing ? root.document.pause() : root.document.play()
            }
            IconButton {
                anchors.verticalCenter: parent.verticalCenter
                width: 28; height: 28; iconSize: Theme.iconSizeSm
                iconName: "repeat"
                tooltip: "În buclă"
                enabled: root.hasAnimation
                checked: root.tl.loop === true
                solidChecked: false
                onClicked: root.document.setAnimationSettings(root.duration, root.tl.fps, !root.tl.loop)
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: root.hasAnimation ? root.fmt(root.document.timelineTime) + " / " + root.fmt(root.duration) + " s" : ""
                color: Theme.textSecondary
                font.family: Theme.fontMono
                font.pixelSize: Theme.fontSizeSm
            }

            Separator { width: 1; height: 20; anchors.verticalCenter: parent.verticalCenter }

            Button {
                anchors.verticalCenter: parent.verticalCenter
                small: true
                text: root.wide ? "Keyframe" : ""
                iconName: "diamond"
                Tooltip { visible: parent.hovered && !root.wide; text: "Keyframe: poziție, rotație, mărime" }
                enabled: root.hasAnimation
                onClicked: root.document.keySelection()
            }
            Button {
                anchors.verticalCenter: parent.verticalCenter
                small: true
                text: root.wide ? "Culoare" : ""
                iconName: "palette"
                Tooltip { visible: parent.hovered && !root.wide; text: "Keyframe de culoare" }
                enabled: root.hasAnimation
                onClicked: root.document.keySelectionColor()
            }
            Button {
                anchors.verticalCenter: parent.verticalCenter
                small: true
                text: root.wide ? "Cameră" : ""
                iconName: "video"
                Tooltip { visible: parent.hovered && !root.wide; text: "Keyframe de cameră" }
                enabled: root.hasAnimation
                onClicked: root.document.keyCamera(root.viewport.cameraState())
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space2
            IconButton {
                width: 28; height: 28; iconSize: Theme.iconSizeSm
                iconName: "settings"
                tooltip: "Durată și fps"
                enabled: root.hasAnimation
                checked: settingsPopup.opened
                solidChecked: false
                onClicked: settingsPopup.opened ? settingsPopup.close() : settingsPopup.open()
                Popover {
                    id: settingsPopup
                    y: -height - Theme.space1
                    x: parent.width - width
                    width: 200
                    padding: Theme.space3
                    contentItem: Column {
                        spacing: Theme.space2
                        NumberField {
                            width: 174
                            label: "Durată"
                            unit: "s"
                            from: 0.1
                            to: 3600
                            step: 0.5
                            decimals: 1
                            value: root.duration
                            onValueModified: (v) => root.document.setAnimationSettings(v, root.tl.fps, root.tl.loop)
                        }
                        NumberField {
                            width: 174
                            label: "fps"
                            from: 1
                            to: 120
                            value: root.tl.fps ?? 30
                            onValueModified: (v) => root.document.setAnimationSettings(root.duration, Math.round(v), root.tl.loop)
                        }
                    }
                }
            }
            Button {
                small: true
                text: root.wide ? "Exportă video" : ""
                iconName: "film"
                Tooltip { visible: parent.hovered && !root.wide; text: "Exportă video (MP4)" }
                enabled: root.hasAnimation && !(root.viewport && root.viewport.exporting)
                onClicked: root.exportRequested()
            }
        }
    }

    // Progresul exportului.
    Rectangle {
        visible: root.viewport && root.viewport.exporting
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 2
        color: Theme.bgHover
        Rectangle {
            width: parent.width * (root.viewport ? root.viewport.exportProgress : 0)
            height: parent.height
            color: Theme.accent
        }
    }

    // --- Pistele ---
    Item {
        id: body
        anchors.top: header.bottom
        anchors.topMargin: Theme.space2
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: Theme.space2

        readonly property var rows: {
            const out = []
            if (!root.hasAnimation)
                return out
            if ((root.tl.camera ?? []).length > 0)
                out.push({ kind: "camera", instance: 0, name: "Cameră", keys: root.tl.camera })
            for (const t of (root.tl.tracks ?? [])) {
                out.push({ kind: "transform", instance: t.instance, name: t.name, keys: t.transform, selected: t.selected })
                if (t.color.length > 0)
                    out.push({ kind: "color", instance: t.instance, name: t.name + " · culoare", keys: t.color, selected: t.selected })
            }
            return out
        }

        // Numele pistelor.
        Column {
            id: names
            anchors.top: parent.top
            anchors.topMargin: 24
            width: root.namesWidth
            spacing: 2
            Repeater {
                model: body.rows
                delegate: Item {
                    required property var modelData
                    width: names.width
                    height: 22
                    Row {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.space1 + 2
                        Icon {
                            anchors.verticalCenter: parent.verticalCenter
                            name: modelData.kind === "camera" ? "video" : modelData.kind === "color" ? "palette" : "box"
                            size: 12
                            color: Theme.textTertiary
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            width: names.width - 24
                            elide: Text.ElideRight
                            text: modelData.name
                            color: modelData.selected ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }
                }
            }
        }

        Text {
            visible: !root.hasAnimation || body.rows.length === 0
            anchors.left: lanes.left
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: Theme.space3
            width: lanes.width - Theme.space6
            wrapMode: Text.Wrap
            text: !root.hasAnimation
                  ? "Creează o animație (meniul din stânga sus), apoi: așază obiectul, apasă „Keyframe”, mută playhead-ul, "
                    + "schimbă poziția obiectului și apasă din nou „Keyframe”."
                  : "Selectează un obiect și apasă „Keyframe” (poziție, rotație, mărime), „Culoare” sau „Cameră”."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }

        // Rigla, pistele și playhead-ul.
        Item {
            id: lanes
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: names.right
            anchors.right: parent.right
            clip: true
            visible: root.hasAnimation

            // Click / drag pe zona goală: mută playhead-ul.
            MouseArea {
                anchors.fill: parent
                onPressed: (mouse) => { root.selectedKey = null; root.forceActiveFocus(); root.document.timelineTime = root.timeAt(mouse.x) }
                onPositionChanged: (mouse) => { if (pressed) root.document.timelineTime = root.timeAt(mouse.x) }
            }

            // Rigla: o gradație pe secundă (sau mai rară, dacă nu încape).
            Repeater {
                model: Math.floor(root.duration / Math.max(1, Math.ceil(40 / root.pxPerSecond))) + 1
                delegate: Item {
                    required property int index
                    readonly property real step: Math.max(1, Math.ceil(40 / root.pxPerSecond))
                    x: root.xAt(index * step)
                    height: lanes.height
                    Rectangle { width: 1; height: lanes.height; color: Theme.borderSubtle }
                    Text {
                        x: 3
                        text: index * parent.step + " s"
                        color: Theme.textTertiary
                        font.pixelSize: 10
                    }
                }
            }

            Column {
                anchors.top: parent.top
                anchors.topMargin: 24
                width: parent.width
                spacing: 2
                Repeater {
                    model: body.rows
                    delegate: Rectangle {
                        id: lane
                        required property var modelData
                        width: lanes.width
                        height: 22
                        radius: Theme.radiusSm
                        color: modelData.selected ? Theme.accentSubtle : Theme.bgHover
                        Repeater {
                            model: lane.modelData.keys
                            delegate: Item {
                                id: keyItem
                                required property var modelData
                                readonly property bool selected: root.selectedKey !== null
                                    && root.selectedKey.kind === lane.modelData.kind
                                    && root.selectedKey.instance === lane.modelData.instance
                                    && Math.abs(root.selectedKey.time - modelData.time) < 1e-6
                                property real dragX: 0
                                x: root.xAt(modelData.time) - 7 + dragX
                                y: 4
                                width: 14
                                height: 14
                                Rectangle {
                                    anchors.centerIn: parent
                                    width: 10
                                    height: 10
                                    rotation: 45
                                    radius: 2
                                    color: lane.modelData.kind === "color" ? Theme.danger
                                         : lane.modelData.kind === "camera" ? Theme.axisY : Theme.accent
                                    border.color: keyItem.selected ? Theme.textPrimary : "transparent"
                                    border.width: 2
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -3
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    cursorShape: Qt.SizeHorCursor
                                    property real startX: 0
                                    onPressed: (mouse) => {
                                        root.forceActiveFocus()
                                        root.selectedKey = { kind: lane.modelData.kind, instance: lane.modelData.instance,
                                                             time: keyItem.modelData.time }
                                        startX = mouse.x
                                        if (mouse.button === Qt.RightButton) {
                                            easingMenu.key = root.selectedKey
                                            easingMenu.x = keyItem.x
                                            easingMenu.open()
                                        } else {
                                            root.document.timelineTime = keyItem.modelData.time
                                        }
                                    }
                                    onPositionChanged: (mouse) => { if (pressed && mouse.buttons & Qt.LeftButton) keyItem.dragX += mouse.x - startX }
                                    onReleased: (mouse) => {
                                        if (mouse.button !== Qt.LeftButton || Math.abs(keyItem.dragX) < 2) {
                                            keyItem.dragX = 0
                                            return
                                        }
                                        const t = root.timeAt(root.xAt(keyItem.modelData.time) + keyItem.dragX)
                                        const key = { kind: lane.modelData.kind, instance: lane.modelData.instance }
                                        root.document.moveKey(key.kind, key.instance, keyItem.modelData.time, t)
                                        keyItem.dragX = 0
                                        root.selectedKey = { kind: key.kind, instance: key.instance, time: t }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Playhead.
            Rectangle {
                visible: root.hasAnimation
                x: root.xAt(root.document ? root.document.timelineTime : 0)
                width: 2
                height: lanes.height
                color: Theme.accent
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 10
                    height: 10
                    radius: 2
                    rotation: 45
                    y: 4
                    color: Theme.accent
                }
            }
        }
    }

    Popover {
        id: easingMenu
        property var key: null
        y: header.height + Theme.space2
        width: 180
        padding: Theme.space1
        contentItem: Column {
            spacing: 1
            Repeater {
                model: ["Liniar", "Lin (in-out)", "Accelerează (in)", "Încetinește (out)", "Salt (step)"]
                delegate: Rectangle {
                    required property string modelData
                    required property int index
                    width: 172
                    height: Theme.controlHeightSm
                    radius: Theme.radiusSm
                    color: easingArea.containsMouse ? Theme.bgHover : "transparent"
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: Theme.space2
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSm
                    }
                    MouseArea {
                        id: easingArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                            const k = easingMenu.key
                            root.document.setKeyEasing(k.kind, k.instance, k.time, index)
                            easingMenu.close()
                        }
                    }
                }
            }
            Rectangle {
                width: 172
                height: Theme.controlHeightSm
                radius: Theme.radiusSm
                color: deleteKeyArea.containsMouse ? Theme.bgHover : "transparent"
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Șterge keyframe-ul"
                    color: Theme.danger
                    font.pixelSize: Theme.fontSizeSm
                }
                MouseArea {
                    id: deleteKeyArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { root.deleteSelected(); easingMenu.close() }
                }
            }
        }
    }
}
