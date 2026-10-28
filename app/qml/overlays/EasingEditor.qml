import QtQuick
import Aion
import "../components/EasingMath.js" as EasingMath

// Editorul curbei de easing (ADR 0034): graficul cu cele două mânere trase cu mouse-ul, cele patru valori ale curbei
// (ca `cubic-bezier` din CSS), o previzualizare în buclă, presetările aplicației și cele salvate de utilizator (în
// preferințe, deci disponibile în orice document; se redenumesc și se șterg de aici). Fiecare schimbare terminată
// (mâner eliberat, valoare tastată, presetare aleasă) se aplică imediat cheilor editate (`applied`).
Popover {
    id: editor

    property var easing: ({ name: "easeInOut", x1: 0.42, y1: 0, x2: 0.58, y2: 1, step: false })
    property string subtitle: ""
    property var builtins: []
    property var userPresets: []
    // Indexul presetării proprii redenumite (−1 = numele salvează o presetare nouă).
    property int renaming: -1
    property real progress: 0
    readonly property real fieldWidth: (340 - Theme.space3 * 2 - Theme.space1 * 3) / 4

    signal applied(var easing)
    signal userPresetsEdited(var list)

    function copy(e) {
        return { name: e.name ?? "", x1: e.x1 ?? 0, y1: e.y1 ?? 0, x2: e.x2 ?? 1, y2: e.y2 ?? 1, step: e.step === true }
    }
    // Deschide editorul pentru curba dată; `text` spune ce se editează („2 chei”).
    function edit(e, text) {
        easing = copy(e)
        subtitle = text
        renaming = -1
        nameField.text = ""
        open()
    }
    function setCurve(e, commit) {
        easing = copy(e)
        if (commit)
            applied(easing)
    }
    // O valoare schimbată de mână: curba devine una proprie (fără numele presetării).
    function setValue(key, value, commit) {
        const e = copy(easing)
        e[key] = key.startsWith("x") ? EasingMath.clamp01(value) : Math.max(-1, Math.min(2, value))
        e.name = ""
        for (const p of editor.builtins.concat(editor.userPresets)) {
            if (EasingMath.same(p, e))
                e.name = p.name
        }
        setCurve(e, commit)
    }
    function savePreset() {
        const name = nameField.text.trim()
        if (name.length === 0)
            return
        const list = editor.userPresets.slice()
        if (editor.renaming >= 0) {
            list[editor.renaming] = Object.assign({}, list[editor.renaming], { name: name })
        } else {
            const e = copy(editor.easing)
            e.name = name
            const existing = list.findIndex(p => p.name === name)
            if (existing >= 0)
                list[existing] = e
            else
                list.push(e)
            setCurve(e, true) // cheile primesc și numele
        }
        editor.renaming = -1
        nameField.text = ""
        userPresetsEdited(list)
    }

    width: 340
    padding: Theme.space3

    // Previzualizarea: timpul curge de la 0 la 1, apoi o pauză scurtă, în buclă.
    SequentialAnimation on progress {
        running: editor.opened
        loops: Animation.Infinite
        NumberAnimation { from: 0; to: 1; duration: 1100 }
        PauseAnimation { duration: 500 }
    }

    component Thumb: Rectangle {
        id: thumb
        required property var preset
        property bool custom: false
        property int slot: -1 // în lista presetărilor proprii
        readonly property bool current: EasingMath.same(preset, editor.easing)
        width: 50
        height: 50
        radius: Theme.radiusMd
        color: thumbArea.containsMouse ? Theme.bgHover : Theme.bgInput
        border.color: current ? Theme.accent : "transparent"
        border.width: 1.5
        EasingCurve {
            anchors.centerIn: parent
            width: 32
            height: 32
            easing: thumb.preset
            color: thumb.current ? Theme.accent : Theme.textSecondary
        }
        MouseArea {
            id: thumbArea
            anchors.fill: parent
            hoverEnabled: true
            onClicked: editor.setCurve(thumb.preset, true)
        }
        Tooltip { visible: thumbArea.containsMouse; text: EasingMath.title(thumb.preset) }
        Row {
            visible: thumb.custom && (thumbArea.containsMouse || renameButton.hovered || deleteButton.hovered)
            anchors.top: parent.top
            anchors.right: parent.right
            IconButton {
                id: renameButton
                width: 16; height: 16; iconSize: 10
                iconName: "pencil"
                tooltip: "Redenumește"
                onClicked: {
                    editor.renaming = thumb.slot
                    nameField.text = thumb.preset.name
                    nameField.forceActiveFocus()
                    nameField.selectAll()
                }
            }
            IconButton {
                id: deleteButton
                width: 16; height: 16; iconSize: 10
                iconName: "x"
                tooltip: "Șterge presetarea"
                onClicked: editor.userPresetsEdited(editor.userPresets.filter((p, i) => i !== thumb.slot))
            }
        }
    }

    contentItem: Column {
        spacing: Theme.space3

        Row {
            spacing: Theme.space2
            Text {
                text: "Curba tranziției"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeMd
                font.weight: Font.Medium
            }
            Text {
                anchors.baseline: parent.children[0].baseline
                text: editor.subtitle
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }

        // --- Graficul (timpul pe orizontală, valoarea pe verticală) și previzualizarea ---
        Item {
            id: graph
            readonly property real side: 150
            readonly property real plotTop: side * 0.25
            readonly property real plotLeft: 22
            function gx(x) { return plotLeft + x * side }
            function gy(y) { return plotTop + (1 - y) * side }
            width: parent.width
            height: side * 1.5

            Rectangle { // cutia [0, 1]
                x: graph.plotLeft
                y: graph.plotTop
                width: graph.side
                height: graph.side
                color: "transparent"
                border.color: Theme.borderSubtle
                radius: 2
            }
            Text { x: graph.plotLeft - 14; y: graph.gy(1) - 6; text: "1"; color: Theme.textTertiary; font.pixelSize: 10 }
            Text { x: graph.plotLeft - 14; y: graph.gy(0) - 6; text: "0"; color: Theme.textTertiary; font.pixelSize: 10 }

            Canvas {
                id: curveCanvas
                anchors.fill: parent
                property var e: editor.easing
                onEChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const e = editor.easing
                    // Mânerele.
                    if (!e.step) {
                        ctx.strokeStyle = Theme.textTertiary
                        ctx.lineWidth = 1
                        ctx.beginPath()
                        ctx.moveTo(graph.gx(0), graph.gy(0))
                        ctx.lineTo(graph.gx(e.x1), graph.gy(e.y1))
                        ctx.moveTo(graph.gx(1), graph.gy(1))
                        ctx.lineTo(graph.gx(e.x2), graph.gy(e.y2))
                        ctx.stroke()
                    }
                    // Curba.
                    ctx.strokeStyle = Theme.accent
                    ctx.lineWidth = 2
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(graph.gx(0), graph.gy(0))
                    if (e.step) {
                        ctx.lineTo(graph.gx(1), graph.gy(0))
                        ctx.lineTo(graph.gx(1), graph.gy(1))
                    } else {
                        for (let i = 1; i <= 48; ++i) {
                            const p = EasingMath.point(e, i / 48)
                            ctx.lineTo(graph.gx(p.x), graph.gy(p.y))
                        }
                    }
                    ctx.stroke()
                }
            }

            // Punctul care urmează curba (previzualizarea, în buclă).
            Rectangle {
                readonly property real v: EasingMath.ease(editor.easing, editor.progress)
                x: graph.gx(editor.progress) - 4
                y: graph.gy(v) - 4
                width: 8
                height: 8
                radius: 4
                color: Theme.textPrimary
            }

            // Mânerele curbei.
            Repeater {
                model: editor.easing.step ? 0 : 2
                delegate: Rectangle {
                    id: handle
                    required property int index
                    readonly property string kx: index === 0 ? "x1" : "x2"
                    readonly property string ky: index === 0 ? "y1" : "y2"
                    x: graph.gx(editor.easing[kx]) - 7
                    y: graph.gy(editor.easing[ky]) - 7
                    width: 14
                    height: 14
                    radius: 7
                    color: handleArea.pressed || handleArea.containsMouse ? Theme.accent : Theme.bgElevated
                    border.color: Theme.accent
                    border.width: 2
                    MouseArea {
                        id: handleArea
                        anchors.fill: parent
                        anchors.margins: -4
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        function move(mouse, commit) {
                            const p = mapToItem(graph, mouse.x, mouse.y)
                            const e = editor.copy(editor.easing)
                            e[handle.kx] = EasingMath.clamp01((p.x - graph.plotLeft) / graph.side)
                            e[handle.ky] = Math.max(-0.35, Math.min(1.35, 1 - (p.y - graph.plotTop) / graph.side))
                            e[handle.kx] = Math.round(e[handle.kx] * 100) / 100
                            e[handle.ky] = Math.round(e[handle.ky] * 100) / 100
                            e.name = ""
                            editor.setCurve(e, commit)
                        }
                        onPositionChanged: (mouse) => { if (pressed) move(mouse, false) }
                        onReleased: (mouse) => move(mouse, true)
                    }
                }
            }

            // Previzualizarea, în dreapta: un obiect care urcă după curbă.
            Rectangle {
                id: track
                x: graph.plotLeft + graph.side + Theme.space6
                y: graph.plotTop
                width: 4
                height: graph.side
                radius: 2
                color: Theme.bgHover
                Rectangle {
                    readonly property real v: EasingMath.ease(editor.easing, editor.progress)
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: (1 - v) * graph.side - 8
                    width: 16
                    height: 16
                    radius: 4
                    color: Theme.accent
                }
            }
            Text {
                x: track.x + Theme.space4
                y: graph.plotTop
                width: graph.width - x
                wrapMode: Text.Wrap
                text: EasingMath.title(editor.easing)
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeXs
            }
        }

        // --- Valorile curbei ---
        Row {
            spacing: Theme.space1
            NumberField {
                width: editor.fieldWidth
                compact: true
                label: "x1"
                decimals: 2
                step: 0.05
                from: 0
                to: 1
                enabled: !editor.easing.step
                value: editor.easing.x1
                onValueModified: (v) => editor.setValue("x1", v, true)
            }
            NumberField {
                width: editor.fieldWidth
                compact: true
                label: "y1"
                decimals: 2
                step: 0.05
                from: -1
                to: 2
                enabled: !editor.easing.step
                value: editor.easing.y1
                onValueModified: (v) => editor.setValue("y1", v, true)
            }
            NumberField {
                width: editor.fieldWidth
                compact: true
                label: "x2"
                decimals: 2
                step: 0.05
                from: 0
                to: 1
                enabled: !editor.easing.step
                value: editor.easing.x2
                onValueModified: (v) => editor.setValue("x2", v, true)
            }
            NumberField {
                width: editor.fieldWidth
                compact: true
                label: "y2"
                decimals: 2
                step: 0.05
                from: -1
                to: 2
                enabled: !editor.easing.step
                value: editor.easing.y2
                onValueModified: (v) => editor.setValue("y2", v, true)
            }
        }
        Toggle {
            text: "Salt la final (step)"
            checked: editor.easing.step
            onToggled: {
                const e = editor.copy(editor.easing)
                e.step = checked
                e.name = checked ? "step" : ""
                editor.setCurve(e, true)
            }
        }

        // --- Presetările ---
        Text { text: "Presetări"; color: Theme.textTertiary; font.pixelSize: Theme.fontSizeXs }
        Flow {
            width: parent.width
            spacing: Theme.space1
            Repeater {
                model: editor.builtins
                delegate: Thumb {
                    required property var modelData
                    preset: modelData
                }
            }
        }
        Text {
            visible: editor.userPresets.length > 0
            text: "Ale mele"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Flow {
            visible: editor.userPresets.length > 0
            width: parent.width
            spacing: Theme.space1
            Repeater {
                model: editor.userPresets
                delegate: Thumb {
                    required property var modelData
                    required property int index
                    preset: modelData
                    custom: true
                    slot: index
                }
            }
        }

        // --- Salvarea ca presetare (sau redenumirea uneia proprii) ---
        Row {
            spacing: Theme.space2
            Input {
                id: nameField
                width: 340 - Theme.space3 * 2 - Theme.space2 - saveButton.width
                height: Theme.controlHeightSm
                placeholderText: editor.renaming >= 0 ? "Numele nou al presetării" : "Numele curbei (de exemplu „Ușa mea”)"
                font.pixelSize: Theme.fontSizeSm
                onAccepted: editor.savePreset()
            }
            Button {
                id: saveButton
                small: true
                text: editor.renaming >= 0 ? "Redenumește" : "Salvează"
                iconName: editor.renaming >= 0 ? "pencil" : "plus"
                enabled: nameField.text.trim().length > 0
                onClicked: editor.savePreset()
            }
        }
    }
}
