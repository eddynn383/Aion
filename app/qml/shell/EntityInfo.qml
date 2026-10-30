import QtQuick
import Aion

// Entity Info: proprietățile selecției. Pentru un grup sau o componentă: nume, tag, dimensiuni, blocare și
// comenzile de organizare; pentru fețe aria, pentru muchii lungimea.
Column {
    id: root

    property var document
    // Timeline-ul e deschis: la hover pe o valoare de plasare, eticheta devine butonul de keyframe.
    property bool timelineActive: false
    // Modul de lucru (0 Modelare, 1 Animare, 2 Interacțiune): fiecare arată doar grupurile lui.
    property int workMode: 0
    readonly property bool modeling: workMode === 0
    signal editRules()
    signal toolRequested(string id)
    signal commandRequested(string id)
    // Un câmp al unui obiect dinamic: Main.qml cere confirmarea dacă refacerea unui corp pierde modificări de mână.
    signal parameterRequested(string key, real value)

    readonly property var info: document ? document.entityInfo : ({ kind: "none" })
    readonly property bool isInstance: info.kind === "instance"

    // Spațierea: între categorii Theme.space4; în interiorul unei categorii (PropertyGroup) titlu → câmpuri mai puțin.
    spacing: Theme.space4

    // Titlul selecției și numele (pentru obiecte), împreună.
    Column {
        width: parent.width
        spacing: Theme.space2
    Text {
        width: parent.width
        wrapMode: Text.Wrap
        text: root.info.kind === "none" ? "Nimic selectat" : root.info.title
        color: root.info.kind === "none" ? Theme.textTertiary : Theme.textPrimary
        font.pixelSize: Theme.fontSizeMd
        font.weight: root.info.kind === "none" ? Font.Normal : Font.DemiBold
    }

    Input {
        visible: root.isInstance
        width: parent.width
        placeholderText: root.info.title === "Grup" ? "Numele grupului" : "Numele obiectului"
        text: root.isInstance ? root.info.name : ""
        onEditingFinished: if (root.isInstance && text !== root.info.name) root.document.renameInstance(root.info.instanceId, text)
    }
    }

    // Detaliile: eticheta în stânga și valoarea în dreapta; dacă nu încap pe un rând, valoarea trece dedesubt.
    // Pozițiile sunt calculate direct (nu prin ancore schimbate din mers, care pot rămâne agățate).
    Column {
        visible: (root.info.details || []).length > 0
        width: parent.width
    Repeater {
        model: root.info.details || []
        delegate: Item {
            id: detailRow
            required property var modelData
            readonly property real gap: Theme.space3
            readonly property bool stacked: label.implicitWidth + valueMetrics.advanceWidth + gap > width

            width: root.width
            height: stacked ? label.implicitHeight + Theme.space1 + value.implicitHeight
                            : Math.max(Theme.controlHeightSm, label.implicitHeight, value.implicitHeight)

            TextMetrics {
                id: valueMetrics
                font: value.font
                text: detailRow.modelData.value
            }
            Text {
                id: label
                x: 0
                y: detailRow.stacked ? 0 : (detailRow.height - implicitHeight) / 2
                text: detailRow.modelData.label
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
            }
            Text {
                id: value
                width: detailRow.stacked ? detailRow.width : Math.ceil(valueMetrics.advanceWidth) + 1
                x: detailRow.stacked ? 0 : detailRow.width - width
                y: detailRow.stacked ? label.implicitHeight + Theme.space1 : (detailRow.height - implicitHeight) / 2
                horizontalAlignment: detailRow.stacked ? Text.AlignLeft : Text.AlignRight
                wrapMode: Text.Wrap
                text: detailRow.modelData.value
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
                font.family: Theme.fontMono
            }
        }
    }
    }

    // Materialul grupului sau al obiectului (fețele fără material proprii îl preiau).
    MaterialSwatch {
        visible: root.isInstance && root.modeling
        width: parent.width
        label: "Material"
        document: root.document
        material: root.isInstance && root.info.material ? root.info.material : ({ id: 0, name: "Implicit" })
        onPicked: (m) => root.document.assignMaterial(m, "front")
    }

    // Fețele: materialul pe față și pe verso (Paint Bucket vopsește partea pe care o vezi).
    Column {
        visible: root.info.kind === "faces" && root.modeling
        width: parent.width
        spacing: Theme.space2
        MaterialSwatch {
            width: parent.width
            label: "Față"
            document: root.document
            material: root.info.frontMaterial ?? ({ id: 0, name: "Implicit" })
            onPicked: (m) => root.document.assignMaterial(m, "front")
        }
        MaterialSwatch {
            width: parent.width
            label: "Verso"
            document: root.document
            material: root.info.backMaterial ?? ({ id: 0, name: "Implicit" })
            onPicked: (m) => root.document.assignMaterial(m, "back")
        }
    }

    // „Placă pe față” activă: tipul și grosimea plăcii noi.
    PropertyGroup {
        id: newBoardBox
        readonly property var defaults: root.document ? root.document.boardDefaults : null
        readonly property var currentType: defaults ? defaults.types.find(t => t.key === defaults.type) : null
        visible: root.document !== null && root.document.activeTool === "board" && root.modeling
        title: "Placă nouă"
        Flow {
            width: newBoardBox.width
            spacing: 4
            Repeater {
                model: newBoardBox.defaults ? newBoardBox.defaults.types : []
                delegate: Button {
                    required property var modelData
                    text: modelData.label
                    small: true
                    variant: newBoardBox.defaults && newBoardBox.defaults.type === modelData.key ? "primary" : "secondary"
                    onClicked: root.document.setBoardDefaults(modelData.key, newBoardBox.defaults.thickness)
                }
            }
        }
        Flow {
            width: newBoardBox.width
            spacing: 4
            Repeater {
                model: newBoardBox.currentType ? newBoardBox.currentType.thicknesses : []
                delegate: Button {
                    required property var modelData
                    text: modelData + " mm"
                    small: true
                    variant: newBoardBox.defaults && Math.abs(newBoardBox.defaults.thickness - modelData) < 0.01 ? "primary" : "secondary"
                    onClicked: root.document.setBoardDefaults(newBoardBox.defaults.type, modelData)
                }
            }
        }
    }

    // Obiect dinamic: parametrii generatorului (lățime, înălțime, număr de polițe…). Copiile legate se schimbă împreună.
    PropertyGroup {
        id: parametersBox
        readonly property var parametric: root.isInstance ? (root.info.parametric ?? null) : null
        visible: parametric !== null && root.modeling
        title: "Parametri · " + (parametric ? parametric.name : "")
        Repeater {
            model: parametersBox.parametric ? parametersBox.parametric.parameters : []
            delegate: NumberField {
                required property var modelData
                width: parametersBox.width
                label: modelData.label
                unit: modelData.unit
                from: modelData.min
                to: modelData.max
                step: modelData.step
                decimals: 0
                value: modelData.value
                onValueModified: (v) => root.parameterRequested(modelData.key, v)
            }
        }
        Text {
            visible: parametersBox.parametric !== null && parametersBox.parametric.copies > 1
            width: parent.width
            wrapMode: Text.Wrap
            text: "Se schimbă și celelalte " + ((parametersBox.parametric ? parametersBox.parametric.copies : 1) - 1)
                  + " copii legate. „Fă unic” pentru o variantă separată."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        // Corpurile din bibliotecă (ADR 0043): separator cu clic, oprirea refacerii; generatoarele vechi.
        Text {
            visible: parametersBox.parametric !== null && parametersBox.parametric.legacy
            width: parent.width
            wrapMode: Text.Wrap
            text: "Generator vechi: plăcile nu au tip, spate HDF și feronerie. „Actualizează” îl reface ca "
                  + "„Dulap cu uși” / „Raft”, cu aceleași dimensiuni."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Button {
            visible: parametersBox.parametric !== null && parametersBox.parametric.legacy
            text: "Actualizează"
            onClicked: root.commandRequested("furniture.upgrade")
        }
        Flow {
            visible: parametersBox.parametric !== null && parametersBox.parametric.carcass
            width: parametersBox.width
            spacing: Theme.space2
            Button {
                text: "Adaugă separator"
                onClicked: root.commandRequested("furniture.addDivider")
            }
            Button {
                text: "Transformă în corp obișnuit"
                onClicked: root.commandRequested("furniture.makePlain")
            }
        }
    }

    // Placă (ADR 0044): tipul, grosimea din lista tipului, lungimea și lățimea; starea față de parametri.
    PropertyGroup {
        id: boardBox
        readonly property var board: root.isInstance ? (root.info.board ?? null) : null
        visible: board !== null && root.modeling
        title: board && board.parametric ? "Parametri · Placă" : "Placă (desprinsă de parametri)"

        component Chip: Rectangle {
            id: chip
            property string label: ""
            property bool current: false
            property bool enabledChip: true
            signal clicked()
            width: chipLabel.implicitWidth + Theme.space3 * 2
            height: 24
            radius: 12
            opacity: enabledChip ? 1 : 0.5
            color: current ? Theme.accentSubtle : chipArea.containsMouse ? Theme.bgHover : "transparent"
            border.color: current ? Theme.accent : Theme.borderStrong
            Text {
                id: chipLabel
                anchors.centerIn: parent
                text: chip.label
                color: chip.current ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontSizeXs
            }
            MouseArea {
                id: chipArea
                anchors.fill: parent
                enabled: chip.enabledChip
                hoverEnabled: true
                onClicked: chip.clicked()
            }
        }

        Flow {
            width: boardBox.width
            spacing: 4
            visible: boardBox.board && boardBox.board.parametric
            Repeater {
                model: boardBox.board ? boardBox.board.types : []
                delegate: Chip {
                    required property var modelData
                    label: modelData.label
                    current: boardBox.board && boardBox.board.type === modelData.key
                    enabledChip: boardBox.board && boardBox.board.matches
                    onClicked: root.document.setBoardValue("type", modelData.key)
                }
            }
        }
        Flow {
            width: boardBox.width
            spacing: 4
            visible: boardBox.board && boardBox.board.parametric
            Repeater {
                model: boardBox.board ? boardBox.board.thicknesses : []
                delegate: Chip {
                    required property var modelData
                    label: modelData + " mm"
                    current: boardBox.board && Math.abs(boardBox.board.thickness - modelData) < 0.01
                    enabledChip: boardBox.board && boardBox.board.matches
                    onClicked: root.document.setBoardValue("thickness", modelData)
                }
            }
        }
        NumberField {
            width: boardBox.width
            visible: boardBox.board && boardBox.board.parametric
            enabled: boardBox.board && boardBox.board.matches
            label: "Lungime (fibra)"
            unit: "mm"
            from: 1
            to: 10000
            decimals: 1
            value: boardBox.board ? boardBox.board.length : 0
            onValueModified: (v) => root.document.setBoardValue("length", v)
        }
        NumberField {
            width: boardBox.width
            visible: boardBox.board && boardBox.board.parametric
            enabled: boardBox.board && boardBox.board.matches
            label: "Lățime"
            unit: "mm"
            from: 1
            to: 10000
            decimals: 1
            value: boardBox.board ? boardBox.board.width : 0
            onValueModified: (v) => root.document.setBoardValue("width", v)
        }
        Text {
            visible: boardBox.board && boardBox.board.parametric && !boardBox.board.matches
            width: parent.width
            wrapMode: Text.Wrap
            text: "Modificată manual: " + (boardBox.board ? boardBox.board.reason : "") + "."
            color: Theme.danger
            font.pixelSize: Theme.fontSizeXs
        }
        Row {
            visible: boardBox.board && boardBox.board.parametric && !boardBox.board.matches
            spacing: Theme.space2
            Button {
                text: "Readu la parametri"
                onClicked: root.document.restoreBoard()
            }
            Button {
                text: "Desprinde"
                onClicked: root.document.detachBoard()
            }
        }
        Text {
            visible: boardBox.board && !boardBox.board.parametric
            width: parent.width
            wrapMode: Text.Wrap
            text: "Dimensiunile se măsoară din geometrie; tipul și grosimea rămân pentru lista de debitare."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Text {
            visible: boardBox.board !== null && boardBox.board.copies > 1
            width: parent.width
            wrapMode: Text.Wrap
            text: "Afectează " + (boardBox.board ? boardBox.board.copies : 1) + " apariții legate."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    // Feronerie (pasul 9): varianta (balama aplicată / încastrată…, lungimea glisierei); se schimbă pe toate aparițiile.
    PropertyGroup {
        id: hardwareBox
        readonly property var hardware: root.isInstance ? (root.info.hardware ?? null) : null
        visible: hardware !== null && root.modeling
        title: "Feronerie"
        Flow {
            width: hardwareBox.width
            spacing: 4
            Repeater {
                model: hardwareBox.hardware ? hardwareBox.hardware.variants : []
                delegate: Rectangle {
                    id: variant
                    required property var modelData
                    readonly property bool current: hardwareBox.hardware && hardwareBox.hardware.type === modelData.key
                    width: variantLabel.implicitWidth + Theme.space3 * 2
                    height: 24
                    radius: 12
                    color: current ? Theme.accentSubtle : variantArea.containsMouse ? Theme.bgHover : "transparent"
                    border.color: current ? Theme.accent : Theme.borderStrong
                    Text {
                        id: variantLabel
                        anchors.centerIn: parent
                        text: variant.modelData.label
                        color: variant.current ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeXs
                    }
                    MouseArea {
                        id: variantArea
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.document.setHardwareVariant(variant.modelData.key)
                    }
                }
            }
        }
        Text {
            visible: hardwareBox.hardware !== null && hardwareBox.hardware.copies > 1
            width: parent.width
            wrapMode: Text.Wrap
            text: "Afectează " + (hardwareBox.hardware ? hardwareBox.hardware.copies : 1) + " apariții legate."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    // Cantul plăcii (pasul 7): pe fiecare latură, tipul și decorul; presetări.
    PropertyGroup {
        id: bandBox
        readonly property var board: root.isInstance ? (root.info.board ?? null) : null
        visible: board !== null && root.modeling
        title: "Cant"
        BandPicker {
            width: bandBox.width
            document: root.document
            board: bandBox.board
        }
    }

    // Sferă / cilindru / con: câte laturi au cercurile (mai multe = mai fină). Doar cât geometria e a primitivei.
    PropertyGroup {
        id: segmentsBox
        readonly property int segments: root.isInstance ? (root.info.segments ?? 0) : 0
        visible: segments > 0 && root.modeling
        title: "Formă"
        NumberField {
            width: segmentsBox.width
            label: "Segmente"
            unit: "buc."
            from: 3
            to: 128
            step: 4
            decimals: 0
            value: segmentsBox.segments
            onValueModified: (v) => root.document.setPrimitiveSegments(Math.round(v))
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Mai multe segmente fac curba mai fină. După Push/Pull sau Subdivide nu se mai pot schimba."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    // Transformarea: poziția (a pivotului, ca în timeline), dimensiunile, rotația (Euler XYZ) și scara, câte trei
    // valori. Fiecare are un lacăt (blochează valoarea, inclusiv pentru Move / Rotate / Scale) și, cu timeline-ul
    // deschis, un keyframe la hover.
    Column {
        id: transformBox
        visible: root.isInstance && root.workMode !== 2
        width: parent.width
        spacing: Theme.space4

        readonly property var p: root.isInstance && root.info.placement ? root.info.placement : ({})
        readonly property int locks: root.isInstance ? (root.info.locks ?? 0) : 0
        // Obiectul e pe o cale de parcurgere a animației active: poziția și rotirea vin de la ea (ADR 0036).
        readonly property var anchor: root.isInstance ? (root.info.anchor ?? null) : null
        readonly property real fourth: (width - Theme.space1 * 3) / 4
        property bool linkProportions: false
        readonly property real third: (width - Theme.space1 * 2) / 3
        // Keyframe la momentul curent pentru obiect (diamantul plin).
        readonly property bool keyed: root.isInstance && root.timelineActive && root.document.timeline !== undefined
                                      && root.document.timelineTime >= 0 && root.document.hasTransformKey(root.info.instanceId)

        // Dimensiunea nouă pe o axă; cu proporțiile legate, celelalte se scalează la fel (fără cele blocate).
        function setSize(axis, value) {
            const old = { dx: p.dx, dy: p.dy, dz: p.dz }
            if (!(old[axis] > 0) || !(value > 0))
                return
            const sizes = { dx: old.dx, dy: old.dy, dz: old.dz }
            if (linkProportions) {
                const k = value / old[axis]
                const bits = { dx: 3, dy: 4, dz: 5 }
                for (const a of ["dx", "dy", "dz"])
                    if (a === axis || !(locks & (1 << bits[a])))
                        sizes[a] = old[a] * k
            } else {
                sizes[axis] = value
            }
            root.document.setPlacement(sizes)
        }
        function key() {
            if (root.document.activeAnimation <= 0)
                root.document.createAnimation()
            root.document.keySelection()
        }

        component PlacementField: Item {
            id: field
            property string label
            property string unit: ""
            property int decimals: 0
            property real step: 1
            property real from: -1e12
            property real to: 1e12
            property real value: 0
            property int lockBit: 0
            readonly property bool locked: (transformBox.locks & (1 << lockBit)) !== 0
            readonly property bool keyMode: root.timelineActive && hover.hovered
            signal valueModified(real value)

            width: transformBox.third
            height: input.implicitHeight

            HoverHandler { id: hover }
            NumberField {
                id: input
                anchors.fill: parent
                label: field.label
                labelVisible: !field.keyMode
                unit: field.unit
                decimals: field.decimals
                step: field.step
                from: field.from
                to: field.to
                value: field.value
                readOnly: field.locked
                color: field.locked ? Theme.textTertiary : Theme.textPrimary
                compact: true
                extraRightPadding: hover.hovered || field.locked ? 16 : 0
                onValueModified: (v) => { if (!field.locked) field.valueModified(v) }
            }
            // Keyframe (în locul etichetei).
            IconButton {
                visible: field.keyMode
                anchors.left: parent.left
                anchors.leftMargin: 1
                anchors.verticalCenter: parent.verticalCenter
                width: 18; height: 18; iconSize: 11
                iconName: "diamond"
                checked: transformBox.keyed
                tooltip: transformBox.keyed ? "Actualizează keyframe-ul la momentul curent" : "Pune un keyframe la momentul curent"
                // La apăsare: dedesubt e eticheta câmpului, al cărei DragHandler preia eliberarea.
                onPressed: { input.focus = false; transformBox.key() }
            }
            // Lacătul (se vede la hover sau când e blocat).
            IconButton {
                visible: hover.hovered || field.locked
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: 16; height: 16; iconSize: 11
                iconName: field.locked ? "lock" : "lock-open"
                checked: field.locked
                solidChecked: false
                tooltip: field.locked ? "Deblochează valoarea" : "Blochează valoarea"
                onClicked: root.document.setPlacementLock(field.lockBit, !field.locked)
            }
        }

        PropertyGroup {
            visible: transformBox.anchor !== null && root.workMode === 1
            title: "Față de cale (mm, °)"
            Row {
                spacing: Theme.space1
                NumberField { width: transformBox.fourth; compact: true; label: "↑"; step: 50; value: transformBox.anchor ? transformBox.anchor.along : 0; onValueModified: (v) => root.document.setAnchorOffsets({ along: v }) }
                NumberField { width: transformBox.fourth; compact: true; label: "←"; step: 50; value: transformBox.anchor ? transformBox.anchor.lateral : 0; onValueModified: (v) => root.document.setAnchorOffsets({ lateral: v }) }
                NumberField { width: transformBox.fourth; compact: true; label: "Z"; step: 50; value: transformBox.anchor ? transformBox.anchor.up : 0; onValueModified: (v) => root.document.setAnchorOffsets({ up: v }) }
                NumberField { width: transformBox.fourth; compact: true; label: "⟳"; decimals: 1; step: 15; value: transformBox.anchor ? transformBox.anchor.yaw : 0; onValueModified: (v) => root.document.setAnchorOffsets({ yaw: v }) }
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "Pe „" + (transformBox.anchor ? transformBox.anchor.pathName : "") + "”: înainte (de-a lungul căii), la stânga, "
                      + "în sus și rotirea față de direcția de mers. Move / Rotate mută obiectul tot față de cale."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
            Button {
                width: parent.width
                small: true
                variant: "secondary"
                text: "Scoate de pe cale"
                onClicked: root.document.detachFromPath(transformBox.anchor.path, root.info.instanceId)
            }
        }

        // În Modelare, un obiect de pe o cale își arată locul de la startul ei (o mutare rămâne relativă la cale).
        PropertyGroup {
        visible: transformBox.anchor === null || root.workMode !== 1
        title: "Poziție (mm) · a pivotului"
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 0; value: transformBox.p.px ?? 0; onValueModified: (v) => root.document.setPlacement({ px: v }) }
            PlacementField { label: "Y"; lockBit: 1; value: transformBox.p.py ?? 0; onValueModified: (v) => root.document.setPlacement({ py: v }) }
            PlacementField { label: "Z"; lockBit: 2; value: transformBox.p.pz ?? 0; onValueModified: (v) => root.document.setPlacement({ pz: v }) }
        }
        }

        PropertyGroup {
            visible: root.modeling
            title: "Dimensiuni (mm)"
            actions: IconButton {
                width: 20; height: 20; iconSize: 13
                iconName: "link"
                tooltip: transformBox.linkProportions ? "Proporții legate" : "Leagă proporțiile"
                checked: transformBox.linkProportions
                solidChecked: false
                onClicked: transformBox.linkProportions = !transformBox.linkProportions
            }
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 3; from: 0.01; value: transformBox.p.dx ?? 0; onValueModified: (v) => transformBox.setSize("dx", v) }
            PlacementField { label: "Y"; lockBit: 4; from: 0.01; value: transformBox.p.dy ?? 0; onValueModified: (v) => transformBox.setSize("dy", v) }
            PlacementField { label: "Z"; lockBit: 5; from: 0.01; value: transformBox.p.dz ?? 0; onValueModified: (v) => transformBox.setSize("dz", v) }
        }
        }

        PropertyGroup {
        visible: transformBox.anchor === null || root.workMode !== 1
        title: "Rotație (°)"
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 6; decimals: 1; from: -360; to: 360; value: transformBox.p.rx ?? 0; onValueModified: (v) => root.document.setPlacement({ rx: v }) }
            PlacementField { label: "Y"; lockBit: 7; decimals: 1; from: -360; to: 360; value: transformBox.p.ry ?? 0; onValueModified: (v) => root.document.setPlacement({ ry: v }) }
            PlacementField { label: "Z"; lockBit: 8; decimals: 1; from: -360; to: 360; value: transformBox.p.rz ?? 0; onValueModified: (v) => root.document.setPlacement({ rz: v }) }
        }
        }

        PropertyGroup {
            title: "Scară (×)"
            actions: Repeater {
                model: [{ icon: "flip-horizontal", tip: "Oglindește pe X (roșu)" },
                        { icon: "flip-horizontal", tip: "Oglindește pe Y (verde)" },
                        { icon: "flip-vertical", tip: "Oglindește pe Z (albastru)" }]
                delegate: IconButton {
                    required property var modelData
                    required property int index
                    width: 20; height: 20; iconSize: 13
                    iconName: modelData.icon
                    rotation: index === 1 ? 90 : 0
                    visible: root.modeling // oglindirea modifică modelul
                    tooltip: modelData.tip
                    onClicked: root.document.mirrorSelection(index)
                }
            }
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 9; decimals: 2; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sx ?? 1); onValueModified: (v) => root.document.setPlacement({ sx: v }) }
            PlacementField { label: "Y"; lockBit: 10; decimals: 2; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sy ?? 1); onValueModified: (v) => root.document.setPlacement({ sy: v }) }
            PlacementField { label: "Z"; lockBit: 11; decimals: 2; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sz ?? 1); onValueModified: (v) => root.document.setPlacement({ sz: v }) }
        }
        }

        // Pivotul: punctul fix la rotire și scalare, ca distanță de la colțul minim (pe axele obiectului).
        PropertyGroup {
            title: "Pivot (mm, de la colțul minim)"
            Row {
                spacing: Theme.space1
                NumberField { width: transformBox.third; compact: true; label: "X"; step: 10; value: transformBox.p.vx ?? 0; onValueModified: (v) => root.document.setPlacement({ vx: v }) }
                NumberField { width: transformBox.third; compact: true; label: "Y"; step: 10; value: transformBox.p.vy ?? 0; onValueModified: (v) => root.document.setPlacement({ vy: v }) }
                NumberField { width: transformBox.third; compact: true; label: "Z"; step: 10; value: transformBox.p.vz ?? 0; onValueModified: (v) => root.document.setPlacement({ vz: v }) }
            }
            Row {
                spacing: Theme.space1
                Button { width: transformBox.third; small: true; text: "Centru"; onClicked: root.document.setPlacement({ pivot: "center" }) }
                Button { width: transformBox.third; small: true; text: "Bază"; onClicked: root.document.setPlacement({ pivot: "base" }) }
                Button { width: transformBox.third; small: true; text: "Colț"; onClicked: root.document.setPlacement({ pivot: "corner" }) }
            }
        }
    }

    // Forma selectată: linie (lungime), arc (rază, segmente), cerc / poligon (rază, segmente / laturi), dreptunghi
    // (lățime, înălțime). Mărimile se schimbă direct; un pas de undo fiecare.
    PropertyGroup {
        id: shapeBox
        readonly property var shape: root.info.shape ?? null
        readonly property string kind: shape ? shape.kind : ""
        readonly property real half: (width - Theme.space1) / 2
        visible: shape !== null && root.modeling
        title: kind === "line" ? "Lungime (mm)" : kind === "rectangle" ? "Laturi (mm)" : "Mărime"
        NumberField {
            visible: shapeBox.kind === "line"
            width: parent.width
            compact: true
            label: "L"
            from: 0.01
            step: 10
            value: shapeBox.shape ? shapeBox.shape.length : 0
            onValueModified: (v) => root.document.setShapeValue("length", v)
        }
        Row {
            visible: shapeBox.kind === "rectangle"
            spacing: Theme.space1
            NumberField { width: shapeBox.half; compact: true; label: "Lățime"; from: 0.01; step: 10; value: shapeBox.shape ? shapeBox.shape.width : 0; onValueModified: (v) => root.document.setShapeValue("width", v) }
            NumberField { width: shapeBox.half; compact: true; label: "Înălțime"; from: 0.01; step: 10; value: shapeBox.shape ? shapeBox.shape.height : 0; onValueModified: (v) => root.document.setShapeValue("height", v) }
        }
        Row {
            visible: shapeBox.kind === "arc" || shapeBox.kind === "circle" || shapeBox.kind === "polygon"
            spacing: Theme.space1
            NumberField { width: shapeBox.half; compact: true; label: "Rază"; unit: "mm"; from: 0.01; step: 10; value: shapeBox.shape ? shapeBox.shape.radius : 0; onValueModified: (v) => root.document.setShapeValue("radius", v) }
            NumberField {
                width: shapeBox.half
                compact: true
                label: shapeBox.kind === "polygon" ? "Laturi" : "Segmente"
                from: shapeBox.kind === "arc" ? 1 : 3
                to: 720
                enabled: shapeBox.shape ? shapeBox.shape.segmentsEditable : false
                opacity: enabled ? 1 : 0.55
                value: shapeBox.shape ? shapeBox.shape.segments : 0
                onValueModified: (v) => root.document.setShapeValue("segments", v)
            }
        }
        Text {
            visible: shapeBox.kind === "arc"
            text: "Unghi: " + (shapeBox.shape ? shapeBox.shape.angle.toFixed(1) : 0) + "°"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Text {
            visible: shapeBox.shape !== null && shapeBox.kind !== "line" && shapeBox.kind !== "rectangle"
                     && !shapeBox.shape.segmentsEditable
            width: parent.width
            wrapMode: Text.Wrap
            text: "Segmentele se pot schimba doar înainte de Push/Pull (când forma nu face parte dintr-un volum)."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    // Geometria liberă (fețe, muchii, forme): poziția (colțul minim) și dimensiunile cutiei ei; rotirea și scara sunt
    // relative (geometria nu are o orientare salvată): valoarea tastată se aplică, apoi câmpul revine la 0° / 1×.
    Column {
        id: geometryBox
        readonly property var g: root.info.geometry ?? null
        readonly property real third: (width - Theme.space1 * 2) / 3
        visible: g !== null && root.modeling
        width: parent.width
        spacing: Theme.space4
        PropertyGroup {
        title: "Poziție (mm) · a pivotului"
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: "X"; step: 10; value: geometryBox.g ? geometryBox.g.px : 0; onValueModified: (v) => root.document.setGeometryPlacement({ px: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Y"; step: 10; value: geometryBox.g ? geometryBox.g.py : 0; onValueModified: (v) => root.document.setGeometryPlacement({ py: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Z"; step: 10; value: geometryBox.g ? geometryBox.g.pz : 0; onValueModified: (v) => root.document.setGeometryPlacement({ pz: v }) }
        }
        }
        PropertyGroup {
        title: "Dimensiuni (mm)"
        Row {
            spacing: Theme.space1
            Repeater {
                model: [{ key: "dx", label: "X" }, { key: "dy", label: "Y" }, { key: "dz", label: "Z" }]
                delegate: NumberField {
                    required property var modelData
                    readonly property real size: geometryBox.g ? geometryBox.g[modelData.key] : 0
                    width: geometryBox.third
                    compact: true
                    label: modelData.label
                    step: 10
                    from: 0.01
                    value: size
                    // O axă plată (un dreptunghi pe sol, pe Z) nu se poate întinde prin scară.
                    readOnly: size < 1e-6
                    opacity: readOnly ? 0.55 : 1
                    onValueModified: (v) => {
                        const c = {}
                        c[modelData.key] = v
                        root.document.setGeometryPlacement(c)
                    }
                }
            }
        }
        }
        PropertyGroup {
        title: "Rotație (°)"
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: "X"; decimals: 1; step: 15; from: -360; to: 360; value: geometryBox.g ? geometryBox.g.rx : 0; onValueModified: (v) => root.document.setGeometryPlacement({ rx: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Y"; decimals: 1; step: 15; from: -360; to: 360; value: geometryBox.g ? geometryBox.g.ry : 0; onValueModified: (v) => root.document.setGeometryPlacement({ ry: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Z"; decimals: 1; step: 15; from: -360; to: 360; value: geometryBox.g ? geometryBox.g.rz : 0; onValueModified: (v) => root.document.setGeometryPlacement({ rz: v }) }
        }
        }
        PropertyGroup {
        title: "Scară (×)"
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: "X"; decimals: 2; step: 0.1; from: -100; to: 100; value: geometryBox.g ? geometryBox.g.sx : 1; onValueModified: (v) => root.document.setGeometryPlacement({ sx: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Y"; decimals: 2; step: 0.1; from: -100; to: 100; value: geometryBox.g ? geometryBox.g.sy : 1; onValueModified: (v) => root.document.setGeometryPlacement({ sy: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Z"; decimals: 2; step: 0.1; from: -100; to: 100; value: geometryBox.g ? geometryBox.g.sz : 1; onValueModified: (v) => root.document.setGeometryPlacement({ sz: v }) }
        }
        }
        PropertyGroup {
        title: "Pivot (mm, de la colțul minim)"
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: "X"; step: 10; value: geometryBox.g ? geometryBox.g.vx : 0; onValueModified: (v) => root.document.setGeometryPlacement({ vx: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Y"; step: 10; value: geometryBox.g ? geometryBox.g.vy : 0; onValueModified: (v) => root.document.setGeometryPlacement({ vy: v }) }
            NumberField { width: geometryBox.third; compact: true; label: "Z"; step: 10; value: geometryBox.g ? geometryBox.g.vz : 0; onValueModified: (v) => root.document.setGeometryPlacement({ vz: v }) }
        }
        Row {
            spacing: Theme.space1
            Button { width: geometryBox.third; small: true; text: "Centru"; onClicked: root.document.setGeometryPlacement({ pivot: "center" }) }
            Button { width: geometryBox.third; small: true; text: "Bază"; onClicked: root.document.setGeometryPlacement({ pivot: "base" }) }
            Button { width: geometryBox.third; small: true; text: "Colț"; onClicked: root.document.setGeometryPlacement({ pivot: "corner" }) }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Rotația și scara se fac în jurul pivotului (implicit centrul). Rotația și scara pornesc de la 0° / 1 pentru fiecare selecție nouă."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        }
    }

    Toggle {
        visible: root.info.edgesSoft !== undefined && root.modeling
        width: parent.width
        text: root.info.edgesSoft === -1 ? "Muchii netede (mixt)" : "Muchii netede (soft)"
        checked: root.info.edgesSoft === 1
        onToggled: root.document.setSelectedEdgesSoft(checked)
    }

    // Poziționarea texturii pe fețele selectate (Position Texture): deplasare, rotație, scară.
    PropertyGroup {
        id: textureBox
        visible: root.info.kind === "faces" && root.info.texture !== undefined && root.modeling
        readonly property var t: root.info.texture ?? ({})
        readonly property real half: (width - Theme.space1) / 2
        title: "Poziția texturii"
        actions: [
            IconButton {
                width: 20; height: 20; iconSize: 13
                iconName: "image"
                tooltip: "Poziționează cu mouse-ul (trage; Shift rotește, ⌘ scalează)"
                onClicked: root.toolRequested("texture")
            },
            IconButton {
                width: 20; height: 20; iconSize: 13
                iconName: "rotate-ccw"
                tooltip: "Resetează poziția texturii"
                onClicked: root.document.setFaceTexture({ reset: true })
            }
        ]
        Row {
            spacing: Theme.space1
            NumberField { width: textureBox.half; compact: true; label: "X"; unit: "mm"; step: 10; value: textureBox.t.ox ?? 0; onValueModified: (v) => root.document.setFaceTexture({ ox: v }) }
            NumberField { width: textureBox.half; compact: true; label: "Y"; unit: "mm"; step: 10; value: textureBox.t.oy ?? 0; onValueModified: (v) => root.document.setFaceTexture({ oy: v }) }
        }
        Row {
            spacing: Theme.space1
            NumberField { width: textureBox.half; compact: true; label: "Rot."; unit: "°"; decimals: 1; step: 5; value: textureBox.t.rotation ?? 0; onValueModified: (v) => root.document.setFaceTexture({ rotation: v }) }
            NumberField { width: textureBox.half; compact: true; label: "Scară"; unit: "×"; decimals: 2; step: 0.1; from: 0.01; value: textureBox.t.sx ?? 1; onValueModified: (v) => root.document.setFaceTexture({ sx: v, sy: v }) }
        }
    }

    // Tag-ul: „pastile” cu tag-urile modelului (pentru obiecte, grupuri, fețe și muchii).
    PropertyGroup {
        visible: (root.isInstance || root.info.kind === "faces" || root.info.kind === "edges") && root.modeling
        title: "Tag"
        Flow {
            width: parent.width
            spacing: Theme.space1 + 2

            Repeater {
                model: [{ id: 0, name: "Fără tag", color: "transparent" }].concat(root.document ? root.document.tags : [])
                delegate: Rectangle {
                    id: chip
                    required property var modelData
                    readonly property bool current: root.info.tagId === modelData.id
                    width: chipRow.implicitWidth + Theme.space3 * 2
                    height: 24
                    radius: 12
                    color: current ? Theme.accentSubtle : chipArea.containsMouse ? Theme.bgHover : "transparent"
                    border.color: current ? Theme.accent : Theme.borderStrong

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: Theme.space1 + 2
                        Rectangle {
                            visible: chip.modelData.id !== 0
                            anchors.verticalCenter: parent.verticalCenter
                            width: 8
                            height: 8
                            radius: 4
                            color: chip.modelData.color
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: chip.modelData.name
                            color: chip.current ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }
                    MouseArea {
                        id: chipArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.document.assignTag(chip.modelData.id)
                    }
                }
            }
        }
    }

    // Interacțiunea (unealta Interact): ușă pe balama sau sertar care glisează.
    PropertyGroup {
        id: behaviorBox
        visible: root.isInstance && root.workMode === 2
        title: "Interacțiune"

        readonly property var behavior: root.isInstance && root.info.behavior ? root.info.behavior : ({ kind: "none" })
        readonly property int kindIndex: behavior.kind === "rotate" ? 1 : behavior.kind === "slide" ? 2 : 0
        // Rotate: Stânga, Dreapta, Sus, Jos → ancorele 0, 1, 4, 5. Slide: Față, Spate, Stânga, Dreapta, Sus, Jos → 2, 3, 0, 1, 4, 5.
        readonly property var rotateAnchors: [0, 1, 4, 5]
        readonly property var slideAnchors: [2, 3, 0, 1, 4, 5]

        function apply(changes) {
            const b = Object.assign({ kind: behavior.kind, anchor: behavior.anchor ?? 0, amount: behavior.amount ?? 90,
                                      duration: behavior.duration ?? 0.6 }, changes)
            root.document.setBehavior(b)
        }

        Tabs {
            width: parent.width
            model: ["Niciuna", "Balama", "Glisare"]
            currentIndex: behaviorBox.kindIndex
            onCurrentIndexChanged: {
                if (currentIndex === behaviorBox.kindIndex)
                    return
                if (currentIndex === 0) behaviorBox.apply({ kind: "none" })
                else if (currentIndex === 1) behaviorBox.apply({ kind: "rotate", anchor: 0, amount: 90 })
                else behaviorBox.apply({ kind: "slide", anchor: 2, amount: 400 })
            }
        }
        Tabs {
            visible: behaviorBox.kindIndex === 1
            width: parent.width
            model: ["Stânga", "Dreapta", "Sus", "Jos"]
            currentIndex: Math.max(0, behaviorBox.rotateAnchors.indexOf(behaviorBox.behavior.anchor ?? 0))
            onCurrentIndexChanged: if (behaviorBox.kindIndex === 1 && behaviorBox.rotateAnchors[currentIndex] !== behaviorBox.behavior.anchor)
                behaviorBox.apply({ anchor: behaviorBox.rotateAnchors[currentIndex] })
        }
        Flow {
            visible: behaviorBox.kindIndex === 2
            width: parent.width
            spacing: Theme.space1
            Repeater {
                model: ["Față", "Spate", "Stânga", "Dreapta", "Sus", "Jos"]
                delegate: Rectangle {
                    id: dirChip
                    required property string modelData
                    required property int index
                    readonly property bool current: behaviorBox.slideAnchors[index] === behaviorBox.behavior.anchor
                    width: dirText.implicitWidth + Theme.space3 * 2
                    height: 24
                    radius: 12
                    color: current ? Theme.accentSubtle : dirArea.containsMouse ? Theme.bgHover : "transparent"
                    border.color: current ? Theme.accent : Theme.borderStrong
                    Text {
                        id: dirText
                        anchors.centerIn: parent
                        text: dirChip.modelData
                        color: dirChip.current ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeXs
                    }
                    MouseArea {
                        id: dirArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: behaviorBox.apply({ anchor: behaviorBox.slideAnchors[dirChip.index] })
                    }
                }
            }
        }
        Row {
            visible: behaviorBox.kindIndex !== 0
            width: parent.width
            spacing: Theme.space2
            NumberField {
                width: (parent.width - Theme.space2) * 0.55
                label: behaviorBox.kindIndex === 1 ? "Unghi" : "Distanță"
                unit: behaviorBox.kindIndex === 1 ? "°" : "mm"
                from: -100000
                to: 100000
                value: behaviorBox.behavior.amount ?? 0
                onValueModified: (v) => behaviorBox.apply({ amount: v })
            }
            NumberField {
                width: (parent.width - Theme.space2) * 0.45
                label: "Durată"
                unit: "s"
                from: 0
                to: 10
                step: 0.1
                decimals: 1
                value: behaviorBox.behavior.duration ?? 0.6
                onValueModified: (v) => behaviorBox.apply({ duration: v })
            }
        }
        Text {
            visible: behaviorBox.kindIndex !== 0
            width: parent.width
            wrapMode: Text.Wrap
            text: "Cu unealta Interact (mâna din toolbar), un click pe obiect îl deschide sau îl închide. Unghi negativ = sens invers."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Button {
            width: parent.width
            text: root.isInstance && (root.info.ruleCount ?? 0) > 0 ? "Interacțiuni cu blocuri (" + root.info.ruleCount + ")…"
                                                                    : "Interacțiuni cu blocuri…"
            onClicked: root.editRules()
        }
    }

    Toggle {
        visible: root.isInstance && root.modeling
        width: parent.width
        text: "Blocat"
        checked: root.isInstance && root.info.locked === true
        onToggled: root.document.setSelectionLocked(checked)
    }

    Flow {
        visible: root.document && root.document.hasInstanceSelected && root.modeling
        width: parent.width
        spacing: Theme.space2

        Button {
            small: true
            text: "Editează"
            visible: root.isInstance && root.info.locked !== true
            onClicked: root.document.enterInstance(0)
        }
        Button {
            small: true
            text: "Desface"
            onClicked: root.commandRequested("org.explode")
        }
        Button {
            small: true
            text: "Fă unic"
            // Copii legate (×N) sau părți comune cu alte obiecte: separarea îi dă o copie proprie, cu animațiile
            // și interacțiunile ei.
            visible: root.isInstance && root.info.canMakeUnique === true
            onClicked: root.document.makeUniqueSelection()
        }
    }
}
