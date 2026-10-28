import QtQuick
import Aion
import "../components/EasingMath.js" as EasingMath

// Timeline-ul (ca în After Effects / Blender, ADR 0034): animația activă, cu un rând pe obiect (un romb pentru fiecare
// moment cu chei, bare pe intervalele animate) care se deschide (▸ / ▾) în canalele lui — doar proprietățile animate —,
// cu valoarea fiecărei chei alături și valoarea curentă, editabilă, în stânga. Cheile se selectează (⇧ / ⌘ adaugă,
// tragerea pe gol = dreptunghi), se trag în grup (cu snap la cadre și la alte chei; ⇧ fără snap), se șterg, se copiază
// și se lipesc (⌘C / ⌘V), au meniu (click dreapta) și valoarea editabilă (dublu click). Rotița derulează rândurile,
// ⇧ + rotița timpul, ⌘ + rotița face zoom; bara de jos derulează și (de capete) face zoom; rigla mută playhead-ul.
Rectangle {
    id: root

    property var document
    property var viewport

    // Curbele de easing salvate de utilizator (din preferințe).
    property var userEasings: []

    signal exportRequested()
    signal userEasingsEdited(var list)
    // Înălțimea nouă cerută prin tragerea marginii de sus.
    signal resizeRequested(real height)

    readonly property var tl: document ? document.timeline : ({})
    readonly property bool hasAnimation: document && document.activeAnimation > 0
    readonly property real duration: tl.duration ?? 5
    readonly property int fps: tl.fps ?? 30
    readonly property real namesWidth: 248
    readonly property real rowHeight: 24
    readonly property real lanesWidth: Math.max(0, body.width - namesWidth)
    // Pe un timeline îngust, butoanele rămân doar cu iconiță (și tooltip), iar ceasul fără durată.
    readonly property bool wide: width > 1080
    readonly property bool narrow: width < 1000

    // --- Vederea în timp: secunda de la marginea din stânga și zoom-ul. Până la primul zoom, toată animația încape. ---
    property real viewStart: 0
    property real pxPerSecond: 100
    property bool userZoomed: false
    readonly property real fitScale: Math.max(2, (lanesWidth - 28) / Math.max(duration, 0.1))
    onFitScaleChanged: if (!userZoomed) { pxPerSecond = fitScale; viewStart = 0 }

    function xAt(t) { return 12 + (t - viewStart) * pxPerSecond }
    function timeAt(x) { return viewStart + (x - 12) / pxPerSecond }
    function clampTime(t) { return Math.max(0, Math.min(duration, t)) }
    function fitAll() {
        userZoomed = false
        viewStart = 0
        pxPerSecond = fitScale
    }
    // Zoom în jurul punctului `x` din pistă (factor > 1 = apropiere).
    function zoomAt(x, factor) {
        const t = timeAt(x)
        userZoomed = true
        pxPerSecond = Math.max(fitScale / 4, Math.min(4000, pxPerSecond * factor))
        viewStart = Math.max(-1, t - (x - 12) / pxPerSecond)
    }
    function scrollTime(seconds) {
        userZoomed = true
        const visible = Math.max(0.1, (lanesWidth - 24) / pxPerSecond)
        viewStart = Math.max(-0.5, Math.min(Math.max(0, duration - visible) + 0.5, viewStart + seconds))
    }
    function scrubTo(x) {
        if (hasAnimation)
            document.timelineTime = clampTime(Math.round(timeAt(x) * fps) / fps)
    }

    // Un timp tastat: „1.5” / „1,5 s” (secunde), „36f” (cadre), „01:02.50” (minute și secunde). NaN dacă nu e valid.
    function parseTime(text) {
        const t = text.trim().toLowerCase().replace(",", ".")
        if (t.endsWith("f"))
            return parseFloat(t) / fps
        if (t.indexOf(":") >= 0) {
            const parts = t.split(":")
            return parseFloat(parts[0]) * 60 + parseFloat(parts[1])
        }
        return parseFloat(t.endsWith("s") ? t.slice(0, -1) : t)
    }
    function copySelectedKeys() {
        if (selectedKeys.length === 0)
            return false
        document.copyKeys(selectedKeys)
        return true
    }
    function pasteKeys() {
        const pasted = document.pasteKeys()
        if (pasted.length > 0)
            selectKeys(pasted)
    }
    // Cheile din dreptunghiul tras (coordonatele pistelor): cele ale căror romburi cad înăuntru.
    function keysInRect(x0, y0, x1, y1) {
        const out = []
        for (let i = 0; i < rows.length; ++i) {
            const cy = i * rowHeight + rowHeight / 2
            if (cy < Math.min(y0, y1) || cy > Math.max(y0, y1))
                continue
            for (const k of rows[i].keys) {
                const x = xAt(k.time)
                if (x >= Math.min(x0, x1) && x <= Math.max(x0, x1))
                    out.push({ instance: rows[i].instance, property: rows[i].property, time: k.time })
            }
        }
        return out
    }

    // Timpul ca `mm:ss.cc`.
    function clock(t) {
        const m = Math.floor(t / 60)
        const s = t - m * 60
        return String(m).padStart(2, "0") + ":" + (s < 10 ? "0" : "") + s.toFixed(2)
    }

    // --- Rândurile: camera, apoi fiecare obiect și (deschis) canalele lui ---
    // Obiectele deschise (id instanță → true), ținute cât e deschisă aplicația.
    property var expanded: ({})
    function toggleExpanded(id) {
        const e = Object.assign({}, expanded)
        e[id] = !e[id]
        expanded = e
    }
    function expandAll(open) {
        const e = {}
        for (const t of (tl.tracks ?? []))
            e[t.instance] = open
        expanded = e
    }
    readonly property var rows: {
        const out = []
        if (!hasAnimation)
            return out
        if ((tl.camera ?? []).length > 0)
            out.push({ kind: "camera", instance: 0, property: "", name: "Cameră", icon: "video", keys: tl.camera, selected: false })
        for (const t of (tl.tracks ?? [])) {
            const open = expanded[t.instance] === true
            // Pe rândul sumar, fiecare moment arată easing-ul primei chei de la el.
            const easingAt = (time) => {
                for (const c of t.channels) {
                    const k = c.keys.find(k => sameTime(k.time, time))
                    if (k)
                        return k.easing
                }
                return null
            }
            out.push({ kind: "object", instance: t.instance, property: "", name: t.name, icon: "box",
                       keys: t.times.map(time => ({ time: time, easing: easingAt(time) })), selected: t.selected, open: open })
            if (!open)
                continue
            for (const c of t.channels)
                out.push({ kind: "channel", instance: t.instance, property: c.property, name: c.label, unit: c.unit,
                           keys: c.keys, current: c.current, currentText: c.currentText, selected: t.selected })
        }
        return out
    }

    // --- Cheile selectate: { instance, property, time } (instance 0 = camera, property "" = rândul sumar) ---
    property var selectedKeys: []
    function sameTime(a, b) { return Math.abs(a - b) < 0.5 / Math.max(1, fps) }
    function sameKey(a, b) { return a.instance === b.instance && a.property === b.property && sameTime(a.time, b.time) }
    // O cheie e selectată dacă e chiar ea în listă, sau rândul sumar al obiectului ei la același moment.
    function isSelected(ref) {
        for (const s of selectedKeys) {
            if (s.instance === ref.instance && sameTime(s.time, ref.time) && (s.property === ref.property || s.property === ""))
                return true
        }
        return false
    }
    function selectKeys(list) { selectedKeys = list }
    // Valoarea canalului la timpul curent (urmează playhead-ul, fără să refacă rândurile).
    function currentValue(row) {
        const v = document ? document.channelValues[row.instance + ":" + row.property] : undefined
        return v !== undefined ? v : row.current
    }
    function deleteSelectedKeys() {
        if (selectedKeys.length === 0)
            return false
        document.removeKeys(selectedKeys)
        selectKeys([])
        return true
    }

    // Tragerea cheilor: toate cele selectate se mută cu `dragDelta` (secunde), cu snap la cadre, la playhead și la
    // celelalte chei (⇧ = fără snap); timpul se scrie la eliberare (un pas de undo).
    property real dragDelta: 0
    property bool dragging: false
    property var dragStart: null // { time, x, additive, ref }
    function cursorX(item, mouse) { return item.mapToItem(lanesArea, mouse.x, 0).x }
    function snapTime(t) {
        let best = null
        let bestDx = 8
        const consider = (time) => {
            const dx = Math.abs((time - t) * pxPerSecond)
            if (dx < bestDx) {
                bestDx = dx
                best = time
            }
        }
        consider(document.timelineTime)
        for (const r of rows) {
            for (const k of r.keys) {
                if (!isSelected({ instance: r.instance, property: r.property, time: k.time }))
                    consider(k.time)
            }
        }
        return best !== null ? best : Math.round(t * fps) / fps
    }
    function keyPressed(item, mouse) {
        root.forceActiveFocus()
        const ref = item.ref
        if (mouse.button === Qt.RightButton) {
            if (!isSelected(ref))
                selectKeys([ref])
            const p = item.mapToItem(root, mouse.x, mouse.y)
            keyMenu.anchorItem = item
            keyMenu.openAt(p.x, p.y)
            return
        }
        const additive = (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) !== 0
        if (additive) {
            const exact = selectedKeys.findIndex(s => sameKey(s, ref))
            selectKeys(exact >= 0 ? selectedKeys.filter((s, i) => i !== exact) : selectedKeys.concat([ref]))
        } else if (!isSelected(ref)) {
            selectKeys([ref])
        }
        dragStart = { time: ref.time, x: cursorX(item, mouse), additive: additive, ref: ref }
        dragging = false
    }
    function keyDragged(item, mouse) {
        if (!dragStart || !isSelected(dragStart.ref))
            return
        const dx = cursorX(item, mouse) - dragStart.x
        if (!dragging && Math.abs(dx) < 3)
            return
        dragging = true
        const raw = dragStart.time + dx / pxPerSecond
        const target = (mouse.modifiers & Qt.ShiftModifier) ? raw : snapTime(raw)
        // Grupul rămâne în durata animației.
        let lo = Infinity
        let hi = -Infinity
        for (const s of selectedKeys) {
            lo = Math.min(lo, s.time)
            hi = Math.max(hi, s.time)
        }
        dragDelta = Math.max(-lo, Math.min(duration - hi, target - dragStart.time))
    }
    function keyReleased(item, mouse) {
        if (dragging && Math.abs(dragDelta) > 1e-9) {
            const delta = dragDelta
            document.moveKeys(selectedKeys, delta)
            selectKeys(selectedKeys.map(s => Object.assign({}, s, { time: s.time + delta })))
        } else if (!dragging && mouse.button === Qt.LeftButton && dragStart && !dragStart.additive) {
            selectKeys([dragStart.ref])
            document.timelineTime = dragStart.time
        }
        dragDelta = 0
        dragging = false
        dragStart = null
    }
    // --- Easing: editorul de curbă pentru cheile date sau pentru cheile noi (easing-ul implicit) ---
    property var easingTarget: null // { refs } sau { defaultEasing: true }
    function editEasing(target, easing, subtitle, item) {
        easingTarget = target
        const p = item.mapToItem(root, item.width / 2, 0)
        easingEditor.x = Math.max(Theme.space2, Math.min(root.width - easingEditor.width - Theme.space2, p.x - easingEditor.width / 2))
        easingEditor.y = -easingEditor.implicitHeight - Theme.space2
        easingEditor.edit(easing ?? { name: "easeInOut", x1: 0.42, y1: 0, x2: 0.58, y2: 1, step: false }, subtitle)
    }
    // Segmentul dintre cheia `index` a rândului și următoarea: easing-ul cheii de la început.
    function segmentClicked(row, index, item) {
        const key = row.keys[index]
        editEasing({ refs: [{ instance: row.instance, property: row.property, time: key.time }] }, key.easing,
                   row.kind === "object" ? row.name + " · toate proprietățile" : row.name, item)
    }
    // Tot canalul (sau toate canalele obiectului): click dreapta pe numele lui.
    function editRowEasing(row, item) {
        if (row.keys.length === 0)
            return
        editEasing({ refs: row.keys.map(k => ({ instance: row.instance, property: row.property, time: k.time })) },
                   row.keys[0].easing, row.name + " · " + row.keys.length + (row.keys.length === 1 ? " cheie" : " chei"), item)
    }
    function editSelectedEasing(item) {
        if (selectedKeys.length === 0)
            return
        const first = selectedKeys[0]
        const row = rows.find(r => r.instance === first.instance && r.property === first.property)
        const key = row ? row.keys.find(k => sameTime(k.time, first.time)) : null
        editEasing({ refs: selectedKeys }, key ? key.easing : null,
                   selectedKeys.length === 1 ? "1 cheie" : selectedKeys.length + " chei", item)
    }

    function keyDoubleClicked(item) {
        if (item.row.kind !== "channel" || item.row.property === "color")
            return
        valueEditor.open(item)
    }

    color: Theme.bgPanel
    focus: true

    Separator { width: parent.width; anchors.top: parent.top }
    // Marginea de sus se trage (înălțimea timeline-ului).
    MouseArea {
        anchors.left: parent.left
        anchors.right: parent.right
        y: -3
        height: 7
        z: 5
        cursorShape: Qt.SizeVerCursor
        property real startY: 0
        property real startHeight: 0
        onPressed: (mouse) => { startY = mapToItem(null, mouse.x, mouse.y).y; startHeight = root.height }
        onPositionChanged: (mouse) => {
            if (pressed)
                root.resizeRequested(startHeight - (mapToItem(null, mouse.x, mouse.y).y - startY))
        }
    }

    // --- Antetul: animația, redarea, timpul, keyframe-urile, setările ---
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
                width: root.narrow ? 140 : 180
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

            // Transportul: început, cheia anterioară, redare, cheia următoare, sfârșit, buclă.
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                IconButton {
                    width: 28; height: 28; iconSize: Theme.iconSizeSm
                    iconName: "skip-back"
                    tooltip: "La început"
                    shortcut: "Home"
                    enabled: root.hasAnimation
                    onClicked: root.document.jumpToEdge(false)
                }
                IconButton {
                    width: 28; height: 28; iconSize: Theme.iconSizeSm
                    iconName: "arrow-left"
                    tooltip: "Cheia anterioară"
                    shortcut: "Alt+Left"
                    enabled: root.hasAnimation
                    onClicked: root.document.jumpToKey(-1)
                }
                IconButton {
                    width: 28; height: 28; iconSize: Theme.iconSizeSm
                    iconName: root.document && root.document.playing ? "pause" : "play"
                    tooltip: root.document && root.document.playing ? "Pauză" : "Redă"
                    shortcut: "Space"
                    enabled: root.hasAnimation
                    checked: root.document && root.document.playing
                    onClicked: root.document.togglePlay()
                }
                IconButton {
                    width: 28; height: 28; iconSize: Theme.iconSizeSm
                    iconName: "arrow-right"
                    tooltip: "Cheia următoare"
                    shortcut: "Alt+Right"
                    enabled: root.hasAnimation
                    onClicked: root.document.jumpToKey(1)
                }
                IconButton {
                    width: 28; height: 28; iconSize: Theme.iconSizeSm
                    iconName: "skip-back"
                    rotation: 180
                    tooltip: "La sfârșit"
                    shortcut: "End"
                    enabled: root.hasAnimation
                    onClicked: root.document.jumpToEdge(true)
                }
                IconButton {
                    width: 28; height: 28; iconSize: Theme.iconSizeSm
                    iconName: "repeat"
                    tooltip: "În buclă"
                    enabled: root.hasAnimation
                    checked: root.tl.loop === true
                    solidChecked: false
                    onClicked: root.document.setAnimationSettings(root.duration, root.fps, !root.tl.loop)
                }
            }

            // Timpul: mm:ss.cc / durata · cadrul; click = tastezi un timp („1.5”, „36f”, „00:02.50”).
            Rectangle {
                id: clockBox
                anchors.verticalCenter: parent.verticalCenter
                visible: root.hasAnimation
                width: clockText.implicitWidth + Theme.space2 * 2
                height: Theme.controlHeightSm
                radius: Theme.radiusMd
                color: clockInput.visible ? Theme.bgInput : clockArea.containsMouse ? Theme.bgHover : "transparent"
                border.color: clockInput.visible ? Theme.focusRing : "transparent"
                Text {
                    id: clockText
                    visible: !clockInput.visible
                    anchors.centerIn: parent
                    text: root.clock(root.document ? root.document.timelineTime : 0)
                          + (root.narrow ? "" : " / " + root.clock(root.duration))
                          + "  ·  " + Math.round((root.document ? root.document.timelineTime : 0) * root.fps) + "f"
                    color: Theme.textSecondary
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeSm
                }
                MouseArea {
                    id: clockArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.IBeamCursor
                    onClicked: {
                        clockInput.text = root.clock(root.document.timelineTime)
                        clockInput.visible = true
                        clockInput.forceActiveFocus()
                        clockInput.selectAll()
                    }
                }
                Tooltip { visible: clockArea.containsMouse && !clockInput.visible; text: "Tastează un timp: 1.5 · 36f · 00:02.50" }
                TextInput {
                    id: clockInput
                    visible: false
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space2
                    verticalAlignment: TextInput.AlignVCenter
                    color: Theme.textPrimary
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeSm
                    selectByMouse: true
                    onAccepted: {
                        const t = root.parseTime(text)
                        if (!isNaN(t))
                            root.document.timelineTime = root.clampTime(t)
                        visible = false
                    }
                    onActiveFocusChanged: if (!activeFocus) visible = false
                    Keys.onEscapePressed: visible = false
                }
            }

            // Auto-key: bulina roșie când e pornit.
            Rectangle {
                id: autoKeyButton
                anchors.verticalCenter: parent.verticalCenter
                visible: root.hasAnimation
                readonly property bool on: root.document && root.document.autoKey
                width: autoKeyRow.implicitWidth + Theme.space2 * 2
                height: Theme.controlHeightSm
                radius: Theme.radiusMd
                color: autoKeyArea.containsMouse ? Theme.bgHover : on ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.12) : "transparent"
                border.color: on ? Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.45) : Theme.borderSubtle
                Row {
                    id: autoKeyRow
                    anchors.centerIn: parent
                    spacing: Theme.space1 + 2
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: 8
                        height: 8
                        radius: 4
                        color: autoKeyButton.on ? Theme.danger : "transparent"
                        border.color: autoKeyButton.on ? Theme.danger : Theme.textTertiary
                        border.width: 1.5
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Auto-key"
                        color: autoKeyButton.on ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSm
                    }
                }
                MouseArea {
                    id: autoKeyArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.document.autoKey = !root.document.autoKey
                }
                Tooltip {
                    visible: autoKeyArea.containsMouse
                    text: autoKeyButton.on ? "Auto-key pornit: Move, Rotate, Scale și Inspector-ul pun keyframe-uri"
                                           : "Auto-key oprit: schimbările ajustează poza de bază (și toată animația proprietăților animate)"
                }
            }

            Separator { width: 1; height: 20; anchors.verticalCenter: parent.verticalCenter }

            // ◆ Keyframe (toate canalele de transformare ale selecției) și ▾: culoare, cameră.
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1
                Button {
                    id: keyButton
                    small: true
                    text: root.narrow ? "" : "Keyframe"
                    iconName: "diamond"
                    Tooltip { visible: parent.hovered; text: "Keyframe pe selecție: poziție, rotație, scară (K)" }
                    enabled: root.hasAnimation
                    onClicked: root.document.keySelection()
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20; height: Theme.controlHeightSm; iconSize: 12
                    iconName: "chevron-down"
                    tooltip: "Alte keyframe-uri: culoare, cameră"
                    enabled: root.hasAnimation
                    onClicked: keyKindMenu.openAt(parent.x + parent.parent.x + x, header.y + header.height)
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space2
            IconButton {
                width: 28; height: 28; iconSize: Theme.iconSizeSm
                iconName: "scan"
                tooltip: "Încadrează toată animația"
                enabled: root.hasAnimation
                onClicked: root.fitAll()
            }
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
                        // Easing-ul cheilor noi.
                        Rectangle {
                            id: defaultEasingButton
                            width: 174
                            height: Theme.controlHeightSm
                            radius: Theme.radiusMd
                            color: defaultEasingArea.containsMouse ? Theme.bgHover : Theme.bgInput
                            EasingCurve {
                                id: defaultCurve
                                anchors.left: parent.left
                                anchors.leftMargin: Theme.space2
                                anchors.verticalCenter: parent.verticalCenter
                                width: 18
                                height: 18
                                easing: root.tl.defaultEasing ?? null
                            }
                            Text {
                                anchors.left: defaultCurve.right
                                anchors.leftMargin: Theme.space2
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                elide: Text.ElideRight
                                text: "Implicit: " + EasingMath.title(root.tl.defaultEasing)
                                color: Theme.textPrimary
                                font.pixelSize: Theme.fontSizeSm
                            }
                            MouseArea {
                                id: defaultEasingArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: {
                                    settingsPopup.close()
                                    root.editEasing({ defaultEasing: true }, root.tl.defaultEasing, "cheile noi", defaultEasingButton)
                                }
                            }
                        }
                        NumberField {
                            width: 174
                            label: "Durată"
                            unit: "s"
                            from: 0.1
                            to: 3600
                            step: 0.5
                            decimals: 1
                            value: root.duration
                            onValueModified: (v) => root.document.setAnimationSettings(v, root.fps, root.tl.loop)
                        }
                        NumberField {
                            width: 174
                            label: "fps"
                            from: 1
                            to: 120
                            value: root.fps
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

    // --- Rigla, rândurile, playhead-ul ---
    Item {
        id: body
        anchors.top: header.bottom
        anchors.topMargin: Theme.space1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        anchors.bottomMargin: Theme.space1

        TimelineRuler {
            id: ruler
            visible: root.hasAnimation
            x: root.namesWidth
            width: root.lanesWidth
            height: 20
            timeline: root
        }

        Flickable {
            id: rowsFlick
            anchors.top: ruler.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: timeBar.top
            anchors.bottomMargin: 2
            contentWidth: width
            contentHeight: rowsColumn.height
            interactive: false
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            // Fundalul pistelor: un click mută playhead-ul (și golește selecția cheilor), tragerea face un dreptunghi
            // de selecție (⇧ / ⌘ = adaugă la selecție).
            MouseArea {
                id: lanesArea
                x: root.namesWidth
                width: root.lanesWidth
                height: Math.max(rowsColumn.height, rowsFlick.height)
                property point start: Qt.point(0, 0)
                property bool banding: false
                property var before: []
                onPressed: (mouse) => {
                    root.forceActiveFocus()
                    start = Qt.point(mouse.x, mouse.y)
                    banding = false
                    before = (mouse.modifiers & (Qt.ShiftModifier | Qt.ControlModifier)) ? root.selectedKeys : []
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    if (!banding && Math.hypot(mouse.x - start.x, mouse.y - start.y) > 4)
                        banding = true
                    if (banding) {
                        band.x = Math.min(start.x, mouse.x)
                        band.y = Math.min(start.y, mouse.y)
                        band.width = Math.abs(mouse.x - start.x)
                        band.height = Math.abs(mouse.y - start.y)
                        const inside = root.keysInRect(start.x, start.y, mouse.x, mouse.y)
                        root.selectKeys(before.concat(inside.filter(k => !before.some(b => root.sameKey(b, k)))))
                    }
                }
                onReleased: (mouse) => {
                    if (!banding) {
                        if (before.length === 0)
                            root.selectKeys([])
                        root.scrubTo(mouse.x)
                    }
                    banding = false
                }
                Rectangle {
                    id: band
                    visible: lanesArea.banding
                    color: Theme.accentSubtle
                    border.color: Theme.accent
                    radius: 2
                }
            }

            Column {
                id: rowsColumn
                Repeater {
                    model: root.rows
                    delegate: TimelineRow {
                        required property var modelData
                        timeline: root
                        row: modelData
                    }
                }
            }
        }

        // Bara de jos: partea vizibilă din animație; mijlocul se trage (derulare), capetele schimbă zoom-ul.
        Item {
            id: timeBar
            visible: root.hasAnimation
            anchors.bottom: parent.bottom
            x: root.namesWidth
            width: root.lanesWidth
            height: 10
            readonly property real visibleSpan: Math.max(0.01, (root.lanesWidth - 24) / root.pxPerSecond)
            readonly property real total: Math.max(root.duration, root.viewStart + visibleSpan) - Math.min(0, root.viewStart)
            readonly property real origin: Math.min(0, root.viewStart)
            Rectangle {
                anchors.fill: parent
                anchors.topMargin: 3
                anchors.bottomMargin: 3
                radius: 2
                color: Theme.bgHover
            }
            Rectangle {
                id: thumbBar
                x: (root.viewStart - timeBar.origin) / timeBar.total * timeBar.width
                width: Math.max(16, timeBar.visibleSpan / timeBar.total * timeBar.width)
                height: parent.height
                radius: 4
                color: thumbArea.pressed || thumbArea.containsMouse ? Theme.borderStrong : Theme.borderSubtle
                MouseArea {
                    id: thumbArea
                    anchors.fill: parent
                    anchors.leftMargin: 5
                    anchors.rightMargin: 5
                    hoverEnabled: true
                    property real startX: 0
                    property real startView: 0
                    onPressed: (mouse) => { startX = mapToItem(timeBar, mouse.x, 0).x; startView = root.viewStart }
                    onPositionChanged: (mouse) => {
                        if (!pressed)
                            return
                        const dx = mapToItem(timeBar, mouse.x, 0).x - startX
                        root.userZoomed = true
                        root.viewStart = Math.max(-0.5, startView + dx / timeBar.width * timeBar.total)
                    }
                }
                // Capetele: zoom (marginea opusă rămâne pe loc).
                Repeater {
                    model: 2
                    delegate: MouseArea {
                        required property int index
                        x: index === 0 ? 0 : thumbBar.width - 6
                        width: 6
                        height: thumbBar.height
                        cursorShape: Qt.SizeHorCursor
                        property real startX: 0
                        property real startA: 0
                        property real startB: 0
                        onPressed: (mouse) => {
                            startX = mapToItem(timeBar, mouse.x, 0).x
                            startA = root.viewStart
                            startB = root.viewStart + timeBar.visibleSpan
                        }
                        onPositionChanged: (mouse) => {
                            if (!pressed)
                                return
                            const dt = (mapToItem(timeBar, mouse.x, 0).x - startX) / timeBar.width * timeBar.total
                            const a = index === 0 ? Math.min(startB - 0.05, startA + dt) : startA
                            const b = index === 1 ? Math.max(startA + 0.05, startB + dt) : startB
                            root.userZoomed = true
                            root.pxPerSecond = (root.lanesWidth - 24) / (b - a)
                            root.viewStart = a
                        }
                    }
                }
            }
        }

        // Rotița: rândurile; ⇧ (sau gestul orizontal) = timpul; ⌘ / Ctrl = zoom în jurul cursorului.
        WheelHandler {
            target: null
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
            onWheel: (event) => {
                const lanesX = event.x - root.namesWidth
                const horizontal = Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)
                if (event.modifiers & Qt.ControlModifier) {
                    root.zoomAt(Math.max(0, lanesX), Math.pow(1.0015, event.angleDelta.y))
                } else if ((event.modifiers & Qt.ShiftModifier) || horizontal) {
                    const d = horizontal ? event.angleDelta.x : event.angleDelta.y
                    root.scrollTime(-d / 120 * 60 / root.pxPerSecond)
                } else {
                    rowsFlick.contentY = Math.max(0, Math.min(rowsFlick.contentHeight - rowsFlick.height,
                                                              rowsFlick.contentY - event.angleDelta.y / 2))
                }
            }
        }

        Text {
            // Fără animație, textul stă în mijlocul timeline-ului; cu una (dar fără rânduri), în mijlocul pistelor, ca
            // rigla și playhead-ul de la 0 s să nu treacă prin el.
            visible: !root.hasAnimation || root.rows.length === 0
            readonly property real areaX: root.hasAnimation ? root.namesWidth : 0
            readonly property real areaWidth: root.hasAnimation ? root.lanesWidth : parent.width
            width: Math.min(areaWidth - Theme.space6 * 2, 560)
            x: areaX + (areaWidth - width) / 2
            anchors.verticalCenter: parent.verticalCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: !root.hasAnimation
                  ? "Creează o animație (meniul din stânga sus), apoi: așază obiectul, apasă „Keyframe”, mută playhead-ul, "
                    + "schimbă poziția obiectului și apasă din nou „Keyframe”."
                  : "Selectează un obiect și mută-l, rotește-l sau apasă „Keyframe” (K): fiecare proprietate schimbată "
                    + "devine un canal, vizibil când deschizi obiectul (▸)."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }

        // Playhead-ul, peste riglă și piste.
        Item {
            visible: root.hasAnimation
            x: root.namesWidth
            width: root.lanesWidth
            height: parent.height
            clip: true
            Rectangle {
                x: root.xAt(root.document ? root.document.timelineTime : 0) - 1
                width: 2
                height: parent.height
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

    // Editarea valorii unei chei (dublu click pe ea).
    NumberField {
        id: valueEditor
        property var ref: null
        function open(item) {
            ref = item.ref
            const unit = item.row.unit
            valueEditor.unit = unit === "×" ? "" : unit
            valueEditor.decimals = unit === "mm" ? 0 : unit === "°" ? 1 : 3
            valueEditor.value = item.key.value
            valueEditor.text = valueEditor.format(item.key.value)
            const p = item.mapToItem(root, item.width / 2, item.height / 2)
            x = Math.max(0, Math.min(root.width - width, p.x - width / 2))
            y = p.y - height / 2
            visible = true
            forceActiveFocus()
            selectAll()
        }
        visible: false
        z: 10
        width: 104
        height: 22
        compact: true
        labelVisible: false
        onValueModified: (v) => { if (ref) root.document.setKeyValue(ref, v) }
        onActiveFocusChanged: if (!activeFocus) visible = false
        Keys.onEscapePressed: visible = false
    }

    EasingEditor {
        id: easingEditor
        builtins: root.document ? root.document.easingPresets() : []
        userPresets: root.userEasings
        onApplied: (e) => {
            if (!root.easingTarget)
                return
            if (root.easingTarget.defaultEasing)
                root.document.setDefaultEasing(e)
            else
                root.document.setKeysEasing(root.easingTarget.refs, e)
        }
        onUserPresetsEdited: (list) => root.userEasingsEdited(list)
    }

    // Keyframe-urile din meniul ▾ de lângă „Keyframe”.
    ContextMenu {
        id: keyKindMenu
        items: [
            { id: "transform", name: "Poziție, rotație, scară", shortcut: "K" },
            { id: "color", name: "Culoare" },
            { id: "camera", name: "Cameră (vederea curentă)" }
        ]
        onTriggered: (id) => {
            if (id === "transform")
                root.document.keySelection()
            else if (id === "color")
                root.document.keySelectionColor()
            else
                root.document.keyCamera(root.viewport.cameraState())
        }
    }

    // Meniul unei chei (click dreapta): curba, easing-uri rapide, mergi la cheie, ștergere.
    ContextMenu {
        id: keyMenu
        property var anchorItem: null
        items: [
            { id: "curve", name: "Editează curba…" },
            { separator: true },
            { id: "easing:linear", name: "Easing: liniar" },
            { id: "easing:easeInOut", name: "Easing: lin (in-out)" },
            { id: "easing:easeIn", name: "Easing: accelerează (in)" },
            { id: "easing:easeOut", name: "Easing: încetinește (out)" },
            { id: "easing:step", name: "Easing: salt (step)" },
            { separator: true },
            { id: "goto", name: "Mergi la cheie" },
            { id: "copy", name: "Copiază", shortcut: "⌘C" },
            { id: "paste", name: "Lipește la playhead", shortcut: "⌘V", enabled: root.document ? root.document.hasCopiedKeys() : false },
            { separator: true },
            { id: "delete", name: root.selectedKeys.length > 1 ? "Șterge keyframe-urile" : "Șterge keyframe-ul",
              shortcut: "⌫", destructive: true }
        ]
        onTriggered: (id) => {
            if (id === "curve" && anchorItem)
                root.editSelectedEasing(anchorItem)
            else if (id.startsWith("easing:"))
                root.document.setKeysEasing(root.selectedKeys, id.slice(7))
            else if (id === "copy")
                root.copySelectedKeys()
            else if (id === "paste")
                root.pasteKeys()
            else if (id === "goto" && root.selectedKeys.length > 0)
                root.document.timelineTime = root.selectedKeys[0].time
            else if (id === "delete")
                root.deleteSelectedKeys()
        }
    }
}
