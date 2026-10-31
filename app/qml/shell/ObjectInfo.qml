import QtQuick
import Aion
import "../components/EasingMath.js" as EasingMath

// Inspector-ul pentru o cameră, o lumină sau o cale de parcurgere selectată: poziția, ținta / direcția și setările
// fiecărui tip.
Column {
    id: root

    property var document
    property var viewport
    property var timeline // editorul de curbă (easing) e al timeline-ului

    readonly property var info: document ? document.selectedObject : ({ kind: "none" })
    readonly property bool isCamera: info.kind === "camera"
    readonly property bool isLight: info.kind === "light"
    readonly property string lightKind: isLight ? info.lightKind : ""
    readonly property bool isPath: info.kind === "path"

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
            model: [qsTrId("ObjectInfo.x"), qsTrId("ObjectInfo.y"), qsTrId("ObjectInfo.z")]
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
            name: root.isCamera ? "camera" : root.isPath ? "route"
                : ({ point: "lightbulb", spot: "light-spot", directional: "sun", area: "light-area" })[root.lightKind] ?? "lightbulb"
            size: Theme.iconSizeMd
            color: root.isLight ? root.info.color : Theme.textPrimary
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.isCamera ? qsTrId("ObjectInfo.camera") : root.isPath ? qsTrId("ObjectInfo.cale") : qsTrId("ObjectInfo.lumina")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
    }

    Input {
        width: parent.width
        placeholderText: qsTrId("ObjectInfo.nume")
        text: root.info.name ?? ""
        onEditingFinished: if (text !== root.info.name) root.set({ name: text })
    }

    // --- Lumina: tipul ---
    Tabs {
        visible: root.isLight
        width: parent.width
        model: [qsTrId("ObjectInfo.bec"), qsTrId("ObjectInfo.spot"), qsTrId("ObjectInfo.soare"), qsTrId("ObjectInfo.panou")]
        readonly property var kinds: ["point", "spot", "directional", "area"]
        currentIndex: Math.max(0, kinds.indexOf(root.lightKind))
        onCurrentIndexChanged: if (root.isLight && kinds[currentIndex] !== root.lightKind) root.set({ lightKind: kinds[currentIndex] })
    }

    Caption { text: qsTrId("ObjectInfo.pozitie_mm"); visible: !(root.isLight && root.lightKind === "directional") }
    VectorFields { key: "position"; visible: !(root.isLight && root.lightKind === "directional") }

    Caption { text: qsTrId("ObjectInfo.tinta_spre_ce_priveste_mm"); visible: root.isCamera }
    VectorFields { key: "target"; visible: root.isCamera }

    Caption { text: qsTrId("ObjectInfo.directia_luminii"); visible: root.isLight && root.lightKind !== "point" }
    VectorFields {
        key: "direction"
        visible: root.isLight && root.lightKind !== "point"
        unit: ""
        decimals: 2
        step: 0.05
    }

    // --- Calea de parcurgere (ADR 0036): transformarea, parcurgerea și obiectele ancorate ---
    Text {
        visible: root.isPath && !(root.info.active ?? true)
        width: parent.width
        wrapMode: Text.Wrap
        text: qsTrId("ObjectInfo.calea_animatiei") + (root.info.animationName ?? "") + qsTrId("ObjectInfo.obiectele_ei_se_misca_doar_cand_e_animatia_activa")
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
    }
    PropertyGroup {
        visible: root.isPath
        width: parent.width
        title: qsTrId("ObjectInfo.rotatie")
        VectorFields { key: "rotation"; decimals: 1; step: 15 }
    }
    PropertyGroup {
        visible: root.isPath
        width: parent.width
        title: qsTrId("ObjectInfo.dimensiuni_mm")
        VectorFields { key: "size" }
    }
    PropertyGroup {
        visible: root.isPath
        width: parent.width
        title: qsTrId("ObjectInfo.parcurgere")
        Row {
            width: parent.width
            spacing: Theme.space1
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.start")
                unit: "s"
                decimals: 2
                step: 0.1
                from: 0
                value: root.info.start ?? 0
                onValueModified: (v) => root.set({ start: v })
            }
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.durata")
                unit: "s"
                decimals: 2
                step: 0.5
                from: 0.05
                value: root.info.duration ?? 5
                onValueModified: (v) => root.set({ duration: v })
            }
        }
        // Curba de tranziție a parcurgerii: același editor ca la chei.
        Rectangle {
            id: pathEasingButton
            width: parent.width
            height: Theme.controlHeightSm
            radius: Theme.radiusMd
            color: pathEasingArea.containsMouse ? Theme.bgHover : Theme.bgInput
            EasingCurve {
                id: pathCurve
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                easing: root.info.easing ?? null
            }
            Text {
                anchors.left: pathCurve.right
                anchors.leftMargin: Theme.space2
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: qsTrId("ObjectInfo.curba") + EasingMath.title(root.info.easing)
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
            }
            MouseArea {
                id: pathEasingArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: if (root.timeline) root.timeline.editEasing({ path: root.info.id }, root.info.easing, "calea", pathEasingButton)
            }
        }
    }
    PropertyGroup {
        visible: root.isPath
        width: parent.width
        title: qsTrId("ObjectInfo.obiecte_ancorate")
        actions: IconButton {
            width: 20; height: 20; iconSize: 13
            iconName: "plus"
            tooltip: qsTrId("ObjectInfo.adauga_click_pe_un_obiect_in_viewport")
            checked: root.viewport && root.viewport.pickingAnchor
            solidChecked: false
            onClicked: if (root.viewport) root.viewport.pickingAnchor = !root.viewport.pickingAnchor
        }
        Repeater {
            model: root.isPath ? (root.info.anchors ?? []) : []
            delegate: Rectangle {
                id: anchorRow
                required property var modelData
                width: parent.width
                height: Theme.controlHeightSm
                radius: Theme.radiusSm
                color: anchorHover.hovered ? Theme.bgHover : "transparent"
                HoverHandler { id: anchorHover }
                Icon {
                    id: anchorIcon
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space1
                    anchors.verticalCenter: parent.verticalCenter
                    name: "box"
                    size: 13
                    color: Theme.textSecondary
                }
                Text {
                    anchors.left: anchorIcon.right
                    anchors.leftMargin: Theme.space2
                    anchors.right: anchorActions.left
                    anchors.verticalCenter: parent.verticalCenter
                    elide: Text.ElideRight
                    text: anchorRow.modelData.name
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeSm
                }
                Row {
                    id: anchorActions
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    RowAction {
                        shown: anchorHover.hovered
                        iconName: "x"
                        tooltip: qsTrId("ObjectInfo.scoate_de_pe_cale_revine_unde_era")
                        onClicked: root.document.detachFromPath(root.info.id, anchorRow.modelData.instance)
                    }
                    RowAction {
                        shown: anchorHover.hovered
                        iconName: "rotate-cw"
                        tooltip: qsTrId("ObjectInfo.roteste_fata_cu_90")
                        onClicked: root.document.rotateAnchor(root.info.id, anchorRow.modelData.instance)
                    }
                }
            }
        }
        Text {
            visible: root.isPath && (root.info.anchors ?? []).length === 0
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("ObjectInfo.niciun_obiect_apasa_si_da_click_pe_un_obiect_mai_")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    Button {
        visible: !root.isPath
        width: parent.width
        text: root.isCamera ? qsTrId("ObjectInfo.pune_camera_in_vederea_curenta") : qsTrId("ObjectInfo.pune_lumina_in_vederea_curenta")
        onClicked: root.document.placeSelectedObjectAtView(root.viewport.cameraState())
    }

    // --- Camera (plan Animare, secțiunea 9): formatul, lentila, depth of field, planurile de tăiere ---
    PropertyGroup {
        visible: root.isCamera
        title: qsTrId("ObjectInfo.format")
        Rectangle {
            id: formatButton
            width: parent.width
            height: Theme.controlHeightSm
            radius: Theme.radiusMd
            color: formatArea.containsMouse ? Theme.bgHover : Theme.bgInput
            readonly property var preset: (root.document ? root.document.cameraFormatPresets() : [])
                .find(p => p.width === root.info.formatWidth && p.height === root.info.formatHeight)
            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                text: formatButton.preset ? formatButton.preset.name : qsTrId("ObjectInfo.personalizat")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
            }
            Icon {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron-down"
                size: 12
                color: Theme.textTertiary
            }
            MouseArea {
                id: formatArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: formatMenu.openAt(formatButton.x, formatButton.y + formatButton.height + Theme.space1)
            }
            ContextMenu {
                id: formatMenu
                items: (root.document ? root.document.cameraFormatPresets() : [])
                    .map(p => ({ id: p.width + "x" + p.height, name: p.name, shortcut: p.width + " × " + p.height }))
                onTriggered: (id) => {
                    const wh = id.split("x")
                    root.set({ formatWidth: parseInt(wh[0]), formatHeight: parseInt(wh[1]) })
                }
            }
        }
        Row {
            width: parent.width
            spacing: Theme.space1
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.l")
                unit: "px"
                from: 16
                to: 16384
                value: root.info.formatWidth ?? 1920
                onValueModified: (v) => root.set({ formatWidth: Math.round(v) })
            }
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.h")
                unit: "px"
                from: 16
                to: 16384
                value: root.info.formatHeight ?? 1080
                onValueModified: (v) => root.set({ formatHeight: Math.round(v) })
            }
        }
    }

    PropertyGroup {
        visible: root.isCamera
        title: (root.info.orthographic ?? false) ? qsTrId("ObjectInfo.lentila_paralela")
              : qsTrId("ObjectInfo.lentila") + Math.round(root.info.fovH ?? 0) + "° × " + Math.round(root.info.fovV ?? 0) + "°"
        Row {
            width: parent.width
            spacing: Theme.space1
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.focala")
                unit: "mm"
                from: 1
                to: 2000
                decimals: 0
                visible: !(root.info.orthographic ?? false)
                value: root.info.focalLength ?? 35
                onValueModified: (v) => root.set({ focalLength: v })
            }
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.scara")
                unit: "mm"
                from: 1
                visible: root.info.orthographic ?? false
                value: root.info.orthoScale ?? 10000
                onValueModified: (v) => root.set({ orthoScale: v })
            }
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.roll")
                unit: "°"
                decimals: 1
                value: root.info.roll ?? 0
                onValueModified: (v) => root.set({ roll: v })
            }
        }
        // Focalele uzuale.
        Row {
            visible: !(root.info.orthographic ?? false)
            spacing: Theme.space1
            Repeater {
                model: [16, 24, 35, 50, 85, 135]
                delegate: Button {
                    required property int modelData
                    small: true
                    text: modelData
                    variant: Math.abs((root.info.focalLength ?? 0) - modelData) < 0.5 ? "primary" : "secondary"
                    onClicked: root.set({ focalLength: modelData })
                }
            }
        }
        Rectangle {
            id: sensorButton
            visible: !(root.info.orthographic ?? false)
            width: parent.width
            height: Theme.controlHeightSm
            radius: Theme.radiusMd
            color: sensorArea.containsMouse ? Theme.bgHover : Theme.bgInput
            readonly property var preset: (root.document ? root.document.cameraSensorPresets() : [])
                .find(p => Math.abs(p.width - (root.info.sensorWidth ?? 36)) < 0.01)
            Text {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                text: qsTrId("ObjectInfo.senzor") + (sensorButton.preset ? sensorButton.preset.name : (root.info.sensorWidth ?? 36) + qsTrId("ObjectInfo.mm"))
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
            }
            Icon {
                anchors.right: parent.right
                anchors.rightMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                name: "chevron-down"
                size: 12
                color: Theme.textTertiary
            }
            MouseArea {
                id: sensorArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: sensorMenu.openAt(sensorButton.x, sensorButton.y + sensorButton.height + Theme.space1)
            }
            ContextMenu {
                id: sensorMenu
                items: (root.document ? root.document.cameraSensorPresets() : []).map(p => ({ id: String(p.width), name: p.name }))
                onTriggered: (id) => root.set({ sensorWidth: parseFloat(id) })
            }
        }
        Row {
            width: parent.width
            spacing: Theme.space1
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.shift_x")
                decimals: 2
                step: 0.01
                from: -2
                to: 2
                value: root.info.shiftX ?? 0
                onValueModified: (v) => root.set({ shiftX: v })
            }
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.shift_y")
                decimals: 2
                step: 0.01
                from: -2
                to: 2
                value: root.info.shiftY ?? 0
                onValueModified: (v) => root.set({ shiftY: v })
            }
        }
        Toggle {
            width: parent.width
            text: qsTrId("ObjectInfo.proiectie_paralela")
            checked: root.info.orthographic ?? false
            onToggled: root.set({ orthographic: checked })
        }
    }

    PropertyGroup {
        visible: root.isCamera
        title: qsTrId("ObjectInfo.depth_of_field")
        Toggle {
            width: parent.width
            text: qsTrId("ObjectInfo.claritate_doar_la_distanta_de_focus")
            checked: root.info.dofEnabled ?? false
            onToggled: root.set({ dofEnabled: checked })
        }
        Row {
            visible: root.info.dofEnabled ?? false
            width: parent.width
            spacing: Theme.space1
            NumberField {
                width: (parent.width - Theme.space1) * 0.62
                label: qsTrId("ObjectInfo.focus")
                unit: "mm"
                from: 1
                step: 100
                value: root.info.focusDistance ?? 5000
                onValueModified: (v) => root.set({ focusDistance: v })
            }
            NumberField {
                width: (parent.width - Theme.space1) * 0.38
                label: "f/"
                decimals: 1
                step: 0.1
                from: 0.5
                to: 64
                value: root.info.fStop ?? 2.8
                onValueModified: (v) => root.set({ fStop: v })
            }
        }
        Button {
            visible: root.info.dofEnabled ?? false
            width: parent.width
            iconName: "focus"
            text: root.viewport && root.viewport.pickingFocus ? qsTrId("ObjectInfo.click_in_viewport_pe_punctul_clar") : qsTrId("ObjectInfo.alege_punctul_de_focus")
            onClicked: if (root.viewport) root.viewport.pickingFocus = !root.viewport.pickingFocus
        }
    }

    PropertyGroup {
        visible: root.isCamera
        title: qsTrId("ObjectInfo.planuri_de_taiere")
        Row {
            width: parent.width
            spacing: Theme.space1
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.clip")
                unit: "mm"
                from: 0
                value: root.info.nearClip ?? 0
                onValueModified: (v) => root.set({ nearClip: v })
            }
            NumberField {
                width: (parent.width - Theme.space1) / 2
                label: qsTrId("ObjectInfo.clip_2")
                unit: "mm"
                from: 0
                value: root.info.farClip ?? 0
                onValueModified: (v) => root.set({ farClip: v })
            }
        }
    }
    Button {
        visible: root.isCamera
        width: parent.width
        variant: "primary"
        text: root.viewport && root.viewport.lookThrough === root.info.id ? qsTrId("ObjectInfo.iesi_din_camera_esc") : qsTrId("ObjectInfo.priveste_prin_camera")
        onClicked: root.viewport.lookThrough = root.viewport.lookThrough === root.info.id ? 0 : root.info.id
    }

    // --- Lumina ---
    Toggle {
        visible: root.isLight
        width: parent.width
        text: qsTrId("ObjectInfo.aprinsa")
        checked: root.info.enabled ?? true
        onToggled: root.set({ enabled: checked })
    }
    Caption { text: qsTrId("ObjectInfo.culoare"); visible: root.isLight }
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
        label: qsTrId("ObjectInfo.intensitate")
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
        label: qsTrId("ObjectInfo.raza")
        unit: "mm"
        from: 1
        step: 100
        value: root.info.range ?? 6000
        onValueModified: (v) => root.set({ range: v })
    }
    Slider {
        visible: root.isLight && root.lightKind === "spot"
        width: parent.width
        label: qsTrId("ObjectInfo.con")
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
        label: qsTrId("ObjectInfo.margine_estompata")
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
            label: qsTrId("ObjectInfo.l")
            unit: "mm"
            from: 1
            step: 50
            value: root.info.width ?? 1000
            onValueModified: (v) => root.set({ width: v })
        }
        NumberField {
            width: (parent.width - Theme.space1) / 2
            label: qsTrId("ObjectInfo.h")
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
        text: qsTrId("ObjectInfo.arunca_umbre")
        checked: root.info.castShadows ?? false
        onToggled: root.set({ castShadows: checked })
    }
    Text {
        visible: root.isLight && root.lightKind === "spot" && (root.info.castShadows ?? false)
        width: parent.width
        wrapMode: Text.Wrap
        text: qsTrId("ObjectInfo.un_singur_spot_cu_umbre_la_un_moment_dat_primul_d")
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
    }
    Toggle {
        width: parent.width
        text: root.isPath ? qsTrId("ObjectInfo.calea_in_viewport") : qsTrId("ObjectInfo.marcaj_in_viewport")
        checked: root.info.visible ?? true
        onToggled: root.set({ visible: checked })
    }
    Button {
        width: parent.width
        variant: "secondary"
        text: root.isCamera ? qsTrId("ObjectInfo.sterge_camera") : root.isPath ? qsTrId("ObjectInfo.sterge_calea") : qsTrId("ObjectInfo.sterge_lumina")
        onClicked: root.document.deleteSelectedObject()
    }
}
