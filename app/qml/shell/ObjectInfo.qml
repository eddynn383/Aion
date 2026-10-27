import QtQuick
import Aion

// Inspector-ul pentru o cameră sau o lumină selectată: poziția, ținta / direcția și setările fiecărui tip.
Column {
    id: root

    property var document
    property var viewport

    readonly property var info: document ? document.selectedObject : ({ kind: "none" })
    readonly property bool isCamera: info.kind === "camera"
    readonly property bool isLight: info.kind === "light"
    readonly property string lightKind: isLight ? info.lightKind : ""

    visible: info.kind !== "none"
    spacing: Theme.space3

    function set(changes) { root.document.setObjectProperties(changes) }
    function setVector(key, axis, value) {
        const v = root.info[key].slice()
        v[axis] = value
        const changes = {}
        changes[key] = v
        set(changes)
    }

    // Trei câmpuri X / Y / Z pentru un vector al obiectului.
    component VectorFields: Row {
        id: vectorRow
        property string key
        property string unit: ""
        property int decimals: 0
        property real step: 10
        width: root.width
        spacing: Theme.space1
        Repeater {
            model: ["X", "Y", "Z"]
            delegate: NumberField {
                required property string modelData
                required property int index
                width: (vectorRow.width - Theme.space1 * 2) / 3
                label: modelData
                unit: vectorRow.unit
                decimals: vectorRow.decimals
                step: vectorRow.step
                value: root.info[vectorRow.key] ? root.info[vectorRow.key][index] : 0
                onValueModified: (v) => root.setVector(vectorRow.key, index, v)
            }
        }
    }

    component Caption: Text {
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSm
    }

    Row {
        width: parent.width
        spacing: Theme.space2
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            name: root.isCamera ? "camera" : ({ point: "lightbulb", spot: "light-spot", directional: "sun", area: "light-area" })[root.lightKind] ?? "lightbulb"
            size: Theme.iconSizeMd
            color: root.isLight ? root.info.color : Theme.textPrimary
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.isCamera ? "Cameră" : "Lumină"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
    }

    Input {
        width: parent.width
        placeholderText: "Nume"
        text: root.info.name ?? ""
        onEditingFinished: if (text !== root.info.name) root.set({ name: text })
    }

    // --- Lumina: tipul ---
    Tabs {
        visible: root.isLight
        width: parent.width
        model: ["Bec", "Spot", "Soare", "Panou"]
        readonly property var kinds: ["point", "spot", "directional", "area"]
        currentIndex: Math.max(0, kinds.indexOf(root.lightKind))
        onCurrentIndexChanged: if (root.isLight && kinds[currentIndex] !== root.lightKind) root.set({ lightKind: kinds[currentIndex] })
    }

    Caption { text: "Poziție (mm)"; visible: !(root.isLight && root.lightKind === "directional") }
    VectorFields { key: "position"; visible: !(root.isLight && root.lightKind === "directional") }

    Caption { text: "Ținta, spre ce privește (mm)"; visible: root.isCamera }
    VectorFields { key: "target"; visible: root.isCamera }

    Caption { text: "Direcția luminii"; visible: root.isLight && root.lightKind !== "point" }
    VectorFields {
        key: "direction"
        visible: root.isLight && root.lightKind !== "point"
        unit: ""
        decimals: 2
        step: 0.05
    }

    Button {
        width: parent.width
        text: root.isCamera ? "Pune camera în vederea curentă" : "Pune lumina în vederea curentă"
        onClicked: root.document.placeSelectedObjectAtView(root.viewport.cameraState())
    }

    // --- Camera ---
    Slider {
        visible: root.isCamera
        width: parent.width
        label: "Unghi de vedere"
        from: 10
        to: 120
        stepSize: 1
        value: root.info.fov ?? 50
        valueText: Math.round(value) + "°"
        onMoved: root.set({ fov: value })
    }
    Toggle {
        visible: root.isCamera
        width: parent.width
        text: "Proiecție paralelă"
        checked: root.info.orthographic ?? false
        onToggled: root.set({ orthographic: checked })
    }
    Row {
        visible: root.isCamera
        width: parent.width
        spacing: Theme.space1
        NumberField {
            width: (parent.width - Theme.space1) / 2
            label: "Clip ↑"
            unit: "mm"
            from: 0
            value: root.info.nearClip ?? 0
            onValueModified: (v) => root.set({ nearClip: v })
        }
        NumberField {
            width: (parent.width - Theme.space1) / 2
            label: "Clip ↓"
            unit: "mm"
            from: 0
            value: root.info.farClip ?? 0
            onValueModified: (v) => root.set({ farClip: v })
        }
    }
    Button {
        visible: root.isCamera
        width: parent.width
        variant: "primary"
        text: "Privește prin cameră"
        onClicked: root.viewport.animateToCamera(root.document.cameraView(root.info.id), 0.8)
    }

    // --- Lumina ---
    Toggle {
        visible: root.isLight
        width: parent.width
        text: "Aprinsă"
        checked: root.info.enabled ?? true
        onToggled: root.set({ enabled: checked })
    }
    Caption { text: "Culoare"; visible: root.isLight }
    Flow {
        visible: root.isLight
        width: parent.width
        spacing: Theme.space1
        Repeater {
            // Alb cald (2700 K), neutru, rece (6500 K), chihlimbar, apoi culori de scenă.
            model: ["#FFB46B", "#FFF4E5", "#FFFFFF", "#CCDDFF", "#FF9F1C", "#EF4444", "#22C55E", "#3B82F6", "#A855F7"]
            delegate: Rectangle {
                required property string modelData
                readonly property bool current: (root.info.color ?? "").toUpperCase() === modelData
                width: 22
                height: 22
                radius: 11
                color: modelData
                border.width: current ? 2 : 1
                border.color: current ? Theme.accent : Theme.borderStrong
                MouseArea {
                    anchors.fill: parent
                    onClicked: root.set({ color: parent.modelData })
                }
            }
        }
        Input {
            width: 86
            text: root.info.color ?? ""
            onEditingFinished: if (/^#[0-9a-fA-F]{6}$/.test(text)) root.set({ color: text })
        }
    }
    Slider {
        visible: root.isLight
        width: parent.width
        label: "Intensitate"
        from: 0
        to: 10
        stepSize: 0.1
        value: root.info.intensity ?? 1
        valueText: Number(value).toFixed(1)
        onMoved: root.set({ intensity: value })
    }
    NumberField {
        visible: root.isLight && root.lightKind !== "directional"
        width: parent.width
        label: "Rază"
        unit: "mm"
        from: 1
        step: 100
        value: root.info.range ?? 6000
        onValueModified: (v) => root.set({ range: v })
    }
    Slider {
        visible: root.isLight && root.lightKind === "spot"
        width: parent.width
        label: "Con"
        from: 5
        to: 170
        stepSize: 1
        value: root.info.cone ?? 45
        valueText: Math.round(value) + "°"
        onMoved: root.set({ cone: value })
    }
    Slider {
        visible: root.isLight && root.lightKind === "spot"
        width: parent.width
        label: "Margine estompată"
        from: 0
        to: 1
        stepSize: 0.05
        value: root.info.softness ?? 0.25
        valueText: Math.round(value * 100) + "%"
        onMoved: root.set({ softness: value })
    }
    Row {
        visible: root.isLight && root.lightKind === "area"
        width: parent.width
        spacing: Theme.space1
        NumberField {
            width: (parent.width - Theme.space1) / 2
            label: "L"
            unit: "mm"
            from: 1
            step: 50
            value: root.info.width ?? 1000
            onValueModified: (v) => root.set({ width: v })
        }
        NumberField {
            width: (parent.width - Theme.space1) / 2
            label: "H"
            unit: "mm"
            from: 1
            step: 50
            value: root.info.height ?? 1000
            onValueModified: (v) => root.set({ height: v })
        }
    }
    Toggle {
        visible: root.isLight && root.lightKind === "spot"
        width: parent.width
        text: "Aruncă umbre"
        checked: root.info.castShadows ?? false
        onToggled: root.set({ castShadows: checked })
    }
    Text {
        visible: root.isLight && root.lightKind === "spot" && (root.info.castShadows ?? false)
        width: parent.width
        wrapMode: Text.Wrap
        text: "Un singur spot cu umbre la un moment dat (primul din listă). Marcajul se trage în viewport (Shift = pe verticală)."
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
    }
    Toggle {
        width: parent.width
        text: "Marcaj în viewport"
        checked: root.info.visible ?? true
        onToggled: root.set({ visible: checked })
    }
    Button {
        width: parent.width
        variant: "secondary"
        text: root.isCamera ? "Șterge camera" : "Șterge lumina"
        onClicked: root.document.deleteSelectedObject()
    }
}
