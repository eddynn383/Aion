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
        text: root.info.kind === "none" ? qsTrId("EntityInfo.nimic_selectat") : root.info.title
        color: root.info.kind === "none" ? Theme.textTertiary : Theme.textPrimary
        font.pixelSize: Theme.fontSizeMd
        font.weight: root.info.kind === "none" ? Font.Normal : Font.DemiBold
    }

    Input {
        visible: root.isInstance
        width: parent.width
        placeholderText: root.info.title === "Grup" ? qsTrId("EntityInfo.numele_grupului") : qsTrId("EntityInfo.numele_obiectului")
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
        label: qsTrId("EntityInfo.material")
        document: root.document
        material: root.isInstance && root.info.material ? root.info.material : ({ id: 0, name: qsTrId("EntityInfo.implicit") })
        onPicked: (m) => root.document.assignMaterial(m, "front")
    }

    // Fețele: materialul pe față și pe verso (Paint Bucket vopsește partea pe care o vezi).
    Column {
        visible: root.info.kind === "faces" && root.modeling
        width: parent.width
        spacing: Theme.space2
        MaterialSwatch {
            width: parent.width
            label: qsTrId("EntityInfo.fata")
            document: root.document
            material: root.info.frontMaterial ?? ({ id: 0, name: qsTrId("EntityInfo.implicit") })
            onPicked: (m) => root.document.assignMaterial(m, "front")
        }
        MaterialSwatch {
            width: parent.width
            label: qsTrId("EntityInfo.verso")
            document: root.document
            material: root.info.backMaterial ?? ({ id: 0, name: qsTrId("EntityInfo.implicit") })
            onPicked: (m) => root.document.assignMaterial(m, "back")
        }
    }

    // „Placă pe față” activă: tipul și grosimea plăcii noi.
    PropertyGroup {
        id: newBoardBox
        readonly property var defaults: root.document ? root.document.boardDefaults : null
        readonly property var currentType: defaults ? defaults.types.find(t => t.key === defaults.type) : null
        visible: root.document !== null && root.document.activeTool === "board" && root.modeling
        title: qsTrId("EntityInfo.placa_noua")
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
                    text: modelData + qsTrId("EntityInfo.mm")
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
        title: qsTrId("EntityInfo.parametri") + (parametric ? parametric.name : "")
        Repeater {
            model: parametersBox.parametric ? parametersBox.parametric.parameters : []
            delegate: NumberField {
                required property var modelData
                width: parametersBox.width
                label: modelData.label
                unit: modelData.unit
                // Fără from / to: o valoare în afara limitelor ajunge la document, care o aduce în limite și spune
                // de ce (în loc să fie tăiată în tăcere).
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
            text: qsTrId("EntityInfo.se_schimba_si_celelalte") + ((parametersBox.parametric ? parametersBox.parametric.copies : 1) - 1)
                  + qsTrId("EntityInfo.copii_legate_fa_unic_pentru_o_varianta_separata")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        // Corpurile din bibliotecă (ADR 0043): separator cu clic, oprirea refacerii; generatoarele vechi.
        Text {
            visible: parametersBox.parametric !== null && parametersBox.parametric.legacy
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("EntityInfo.generator_vechi_placile_nu_au_tip_spate_hdf_si_fe")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Button {
            visible: parametersBox.parametric !== null && parametersBox.parametric.legacy
            text: qsTrId("EntityInfo.actualizeaza")
            onClicked: root.commandRequested("furniture.upgrade")
        }
        Flow {
            visible: parametersBox.parametric !== null && parametersBox.parametric.carcass
            width: parametersBox.width
            spacing: Theme.space2
            Button {
                text: qsTrId("EntityInfo.adauga_separator")
                onClicked: root.commandRequested("furniture.addDivider")
            }
            Button {
                text: qsTrId("EntityInfo.transforma_in_corp_obisnuit")
                onClicked: root.commandRequested("furniture.makePlain")
            }
        }
    }

    // Placă (ADR 0044): tipul, grosimea din lista tipului, lungimea și lățimea; starea față de parametri.
    PropertyGroup {
        id: boardBox
        readonly property var board: root.isInstance ? (root.info.board ?? null) : null
        visible: board !== null && root.modeling
        title: board && board.parametric ? qsTrId("EntityInfo.parametri_placa") : qsTrId("EntityInfo.placa_desprinsa_de_parametri")

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
                    label: modelData + qsTrId("EntityInfo.mm")
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
            label: qsTrId("EntityInfo.lungime_fibra")
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
            label: qsTrId("EntityInfo.latime")
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
            text: qsTrId("EntityInfo.modificata_manual") + (boardBox.board ? boardBox.board.reason : "") + "."
            color: Theme.danger
            font.pixelSize: Theme.fontSizeXs
        }
        Row {
            visible: boardBox.board && boardBox.board.parametric && !boardBox.board.matches
            spacing: Theme.space2
            Button {
                text: qsTrId("EntityInfo.readu_la_parametri")
                onClicked: root.document.restoreBoard()
            }
            Button {
                text: qsTrId("EntityInfo.desprinde")
                onClicked: root.document.detachBoard()
            }
        }
        Text {
            visible: boardBox.board && !boardBox.board.parametric
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("EntityInfo.dimensiunile_se_masoara_din_geometrie_tipul_si_gr")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Text {
            visible: boardBox.board !== null && boardBox.board.copies > 1
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("EntityInfo.afecteaza") + (boardBox.board ? boardBox.board.copies : 1) + qsTrId("EntityInfo.aparitii_legate")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    // Feronerie (pasul 9): varianta (balama aplicată / încastrată…, lungimea glisierei); se schimbă pe toate aparițiile.
    PropertyGroup {
        id: hardwareBox
        readonly property var hardware: root.isInstance ? (root.info.hardware ?? null) : null
        visible: hardware !== null && root.modeling
        title: qsTrId("EntityInfo.feronerie")
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
            text: qsTrId("EntityInfo.afecteaza") + (hardwareBox.hardware ? hardwareBox.hardware.copies : 1) + qsTrId("EntityInfo.aparitii_legate")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    // Cantul plăcii (pasul 7): pe fiecare latură, tipul și decorul; presetări.
    PropertyGroup {
        id: bandBox
        readonly property var board: root.isInstance ? (root.info.board ?? null) : null
        visible: board !== null && root.modeling
        title: qsTrId("EntityInfo.cant")
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
        title: qsTrId("EntityInfo.forma")
        NumberField {
            width: segmentsBox.width
            label: qsTrId("EntityInfo.segmente")
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
            text: qsTrId("EntityInfo.mai_multe_segmente_fac_curba_mai_fina_dupa_push_p")
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
                tooltip: transformBox.keyed ? qsTrId("EntityInfo.actualizeaza_keyframe_ul_la_momentul_curent") : qsTrId("EntityInfo.pune_un_keyframe_la_momentul_curent")
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
                tooltip: field.locked ? qsTrId("EntityInfo.deblocheaza_valoarea") : qsTrId("EntityInfo.blocheaza_valoarea")
                onClicked: root.document.setPlacementLock(field.lockBit, !field.locked)
            }
        }

        PropertyGroup {
            visible: transformBox.anchor !== null && root.workMode === 1
            title: qsTrId("EntityInfo.fata_de_cale_mm")
            Row {
                spacing: Theme.space1
                NumberField { width: transformBox.fourth; compact: true; label: "↑"; step: 50; value: transformBox.anchor ? transformBox.anchor.along : 0; onValueModified: (v) => root.document.setAnchorOffsets({ along: v }) }
                NumberField { width: transformBox.fourth; compact: true; label: "←"; step: 50; value: transformBox.anchor ? transformBox.anchor.lateral : 0; onValueModified: (v) => root.document.setAnchorOffsets({ lateral: v }) }
                NumberField { width: transformBox.fourth; compact: true; label: qsTrId("EntityInfo.z"); step: 50; value: transformBox.anchor ? transformBox.anchor.up : 0; onValueModified: (v) => root.document.setAnchorOffsets({ up: v }) }
                NumberField { width: transformBox.fourth; compact: true; label: "⟳"; decimals: 1; step: 15; value: transformBox.anchor ? transformBox.anchor.yaw : 0; onValueModified: (v) => root.document.setAnchorOffsets({ yaw: v }) }
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTrId("EntityInfo.pe") + (transformBox.anchor ? transformBox.anchor.pathName : "") + qsTrId("EntityInfo.inainte_de_a_lungul_caii_la_stanga_in_sus_si_roti")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
            Button {
                width: parent.width
                small: true
                variant: "secondary"
                text: qsTrId("EntityInfo.scoate_de_pe_cale")
                onClicked: root.document.detachFromPath(transformBox.anchor.path, root.info.instanceId)
            }
        }

        // În Modelare, un obiect de pe o cale își arată locul de la startul ei (o mutare rămâne relativă la cale).
        PropertyGroup {
        visible: transformBox.anchor === null || root.workMode !== 1
        title: qsTrId("EntityInfo.pozitie_mm_a_pivotului")
        Row {
            spacing: Theme.space1
            PlacementField { label: qsTrId("EntityInfo.x"); lockBit: 0; value: transformBox.p.px ?? 0; onValueModified: (v) => root.document.setPlacement({ px: v }) }
            PlacementField { label: qsTrId("EntityInfo.y"); lockBit: 1; value: transformBox.p.py ?? 0; onValueModified: (v) => root.document.setPlacement({ py: v }) }
            PlacementField { label: qsTrId("EntityInfo.z"); lockBit: 2; value: transformBox.p.pz ?? 0; onValueModified: (v) => root.document.setPlacement({ pz: v }) }
        }
        }

        PropertyGroup {
            visible: root.modeling
            title: qsTrId("EntityInfo.dimensiuni_mm")
            actions: IconButton {
                width: 20; height: 20; iconSize: 13
                iconName: "link"
                tooltip: transformBox.linkProportions ? qsTrId("EntityInfo.proportii_legate") : qsTrId("EntityInfo.leaga_proportiile")
                checked: transformBox.linkProportions
                solidChecked: false
                onClicked: transformBox.linkProportions = !transformBox.linkProportions
            }
        Row {
            spacing: Theme.space1
            PlacementField { label: qsTrId("EntityInfo.x"); lockBit: 3; from: 0.01; value: transformBox.p.dx ?? 0; onValueModified: (v) => transformBox.setSize("dx", v) }
            PlacementField { label: qsTrId("EntityInfo.y"); lockBit: 4; from: 0.01; value: transformBox.p.dy ?? 0; onValueModified: (v) => transformBox.setSize("dy", v) }
            PlacementField { label: qsTrId("EntityInfo.z"); lockBit: 5; from: 0.01; value: transformBox.p.dz ?? 0; onValueModified: (v) => transformBox.setSize("dz", v) }
        }
        }

        PropertyGroup {
        visible: transformBox.anchor === null || root.workMode !== 1
        title: qsTrId("EntityInfo.rotatie")
        Row {
            spacing: Theme.space1
            PlacementField { label: qsTrId("EntityInfo.x"); lockBit: 6; decimals: 1; from: -360; to: 360; value: transformBox.p.rx ?? 0; onValueModified: (v) => root.document.setPlacement({ rx: v }) }
            PlacementField { label: qsTrId("EntityInfo.y"); lockBit: 7; decimals: 1; from: -360; to: 360; value: transformBox.p.ry ?? 0; onValueModified: (v) => root.document.setPlacement({ ry: v }) }
            PlacementField { label: qsTrId("EntityInfo.z"); lockBit: 8; decimals: 1; from: -360; to: 360; value: transformBox.p.rz ?? 0; onValueModified: (v) => root.document.setPlacement({ rz: v }) }
        }
        }

        PropertyGroup {
            title: qsTrId("EntityInfo.scara")
            actions: Repeater {
                model: [{ icon: "flip-horizontal", tip: qsTrId("EntityInfo.oglindeste_pe_x_rosu") },
                        { icon: "flip-horizontal", tip: qsTrId("EntityInfo.oglindeste_pe_y_verde") },
                        { icon: "flip-vertical", tip: qsTrId("EntityInfo.oglindeste_pe_z_albastru") }]
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
            PlacementField { label: qsTrId("EntityInfo.x"); lockBit: 9; decimals: 2; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sx ?? 1); onValueModified: (v) => root.document.setPlacement({ sx: v }) }
            PlacementField { label: qsTrId("EntityInfo.y"); lockBit: 10; decimals: 2; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sy ?? 1); onValueModified: (v) => root.document.setPlacement({ sy: v }) }
            PlacementField { label: qsTrId("EntityInfo.z"); lockBit: 11; decimals: 2; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sz ?? 1); onValueModified: (v) => root.document.setPlacement({ sz: v }) }
        }
        }

        // Pivotul: punctul fix la rotire și scalare, ca distanță de la colțul minim (pe axele obiectului).
        PropertyGroup {
            title: qsTrId("EntityInfo.pivot_mm_de_la_coltul_minim")
            Row {
                spacing: Theme.space1
                NumberField { width: transformBox.third; compact: true; label: qsTrId("EntityInfo.x"); step: 10; value: transformBox.p.vx ?? 0; onValueModified: (v) => root.document.setPlacement({ vx: v }) }
                NumberField { width: transformBox.third; compact: true; label: qsTrId("EntityInfo.y"); step: 10; value: transformBox.p.vy ?? 0; onValueModified: (v) => root.document.setPlacement({ vy: v }) }
                NumberField { width: transformBox.third; compact: true; label: qsTrId("EntityInfo.z"); step: 10; value: transformBox.p.vz ?? 0; onValueModified: (v) => root.document.setPlacement({ vz: v }) }
            }
            Row {
                spacing: Theme.space1
                Button { width: transformBox.third; small: true; text: qsTrId("EntityInfo.centru"); onClicked: root.document.setPlacement({ pivot: "center" }) }
                Button { width: transformBox.third; small: true; text: qsTrId("EntityInfo.baza"); onClicked: root.document.setPlacement({ pivot: "base" }) }
                Button { width: transformBox.third; small: true; text: qsTrId("EntityInfo.colt"); onClicked: root.document.setPlacement({ pivot: "corner" }) }
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
        title: kind === "line" ? qsTrId("EntityInfo.lungime_mm") : kind === "rectangle" ? qsTrId("EntityInfo.laturi_mm") : qsTrId("EntityInfo.marime")
        NumberField {
            visible: shapeBox.kind === "line"
            width: parent.width
            compact: true
            label: qsTrId("EntityInfo.l")
            from: 0.01
            step: 10
            value: shapeBox.shape ? shapeBox.shape.length : 0
            onValueModified: (v) => root.document.setShapeValue("length", v)
        }
        Row {
            visible: shapeBox.kind === "rectangle"
            spacing: Theme.space1
            NumberField { width: shapeBox.half; compact: true; label: qsTrId("EntityInfo.latime"); from: 0.01; step: 10; value: shapeBox.shape ? shapeBox.shape.width : 0; onValueModified: (v) => root.document.setShapeValue("width", v) }
            NumberField { width: shapeBox.half; compact: true; label: qsTrId("EntityInfo.inaltime"); from: 0.01; step: 10; value: shapeBox.shape ? shapeBox.shape.height : 0; onValueModified: (v) => root.document.setShapeValue("height", v) }
        }
        Row {
            visible: shapeBox.kind === "arc" || shapeBox.kind === "circle" || shapeBox.kind === "polygon"
            spacing: Theme.space1
            NumberField { width: shapeBox.half; compact: true; label: qsTrId("EntityInfo.raza"); unit: "mm"; from: 0.01; step: 10; value: shapeBox.shape ? shapeBox.shape.radius : 0; onValueModified: (v) => root.document.setShapeValue("radius", v) }
            NumberField {
                width: shapeBox.half
                compact: true
                label: shapeBox.kind === "polygon" ? qsTrId("EntityInfo.laturi") : qsTrId("EntityInfo.segmente")
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
            text: qsTrId("EntityInfo.unghi") + (shapeBox.shape ? shapeBox.shape.angle.toFixed(1) : 0) + "°"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Text {
            visible: shapeBox.shape !== null && shapeBox.kind !== "line" && shapeBox.kind !== "rectangle"
                     && !shapeBox.shape.segmentsEditable
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("EntityInfo.segmentele_se_pot_schimba_doar_inainte_de_push_pu")
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
        title: qsTrId("EntityInfo.pozitie_mm_a_pivotului")
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.x"); step: 10; value: geometryBox.g ? geometryBox.g.px : 0; onValueModified: (v) => root.document.setGeometryPlacement({ px: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.y"); step: 10; value: geometryBox.g ? geometryBox.g.py : 0; onValueModified: (v) => root.document.setGeometryPlacement({ py: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.z"); step: 10; value: geometryBox.g ? geometryBox.g.pz : 0; onValueModified: (v) => root.document.setGeometryPlacement({ pz: v }) }
        }
        }
        PropertyGroup {
        title: qsTrId("EntityInfo.dimensiuni_mm")
        Row {
            spacing: Theme.space1
            Repeater {
                model: [{ key: "dx", label: qsTrId("EntityInfo.x") }, { key: "dy", label: qsTrId("EntityInfo.y") }, { key: "dz", label: qsTrId("EntityInfo.z") }]
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
        title: qsTrId("EntityInfo.rotatie")
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.x"); decimals: 1; step: 15; from: -360; to: 360; value: geometryBox.g ? geometryBox.g.rx : 0; onValueModified: (v) => root.document.setGeometryPlacement({ rx: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.y"); decimals: 1; step: 15; from: -360; to: 360; value: geometryBox.g ? geometryBox.g.ry : 0; onValueModified: (v) => root.document.setGeometryPlacement({ ry: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.z"); decimals: 1; step: 15; from: -360; to: 360; value: geometryBox.g ? geometryBox.g.rz : 0; onValueModified: (v) => root.document.setGeometryPlacement({ rz: v }) }
        }
        }
        PropertyGroup {
        title: qsTrId("EntityInfo.scara")
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.x"); decimals: 2; step: 0.1; from: -100; to: 100; value: geometryBox.g ? geometryBox.g.sx : 1; onValueModified: (v) => root.document.setGeometryPlacement({ sx: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.y"); decimals: 2; step: 0.1; from: -100; to: 100; value: geometryBox.g ? geometryBox.g.sy : 1; onValueModified: (v) => root.document.setGeometryPlacement({ sy: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.z"); decimals: 2; step: 0.1; from: -100; to: 100; value: geometryBox.g ? geometryBox.g.sz : 1; onValueModified: (v) => root.document.setGeometryPlacement({ sz: v }) }
        }
        }
        PropertyGroup {
        title: qsTrId("EntityInfo.pivot_mm_de_la_coltul_minim")
        Row {
            spacing: Theme.space1
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.x"); step: 10; value: geometryBox.g ? geometryBox.g.vx : 0; onValueModified: (v) => root.document.setGeometryPlacement({ vx: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.y"); step: 10; value: geometryBox.g ? geometryBox.g.vy : 0; onValueModified: (v) => root.document.setGeometryPlacement({ vy: v }) }
            NumberField { width: geometryBox.third; compact: true; label: qsTrId("EntityInfo.z"); step: 10; value: geometryBox.g ? geometryBox.g.vz : 0; onValueModified: (v) => root.document.setGeometryPlacement({ vz: v }) }
        }
        Row {
            spacing: Theme.space1
            Button { width: geometryBox.third; small: true; text: qsTrId("EntityInfo.centru"); onClicked: root.document.setGeometryPlacement({ pivot: "center" }) }
            Button { width: geometryBox.third; small: true; text: qsTrId("EntityInfo.baza"); onClicked: root.document.setGeometryPlacement({ pivot: "base" }) }
            Button { width: geometryBox.third; small: true; text: qsTrId("EntityInfo.colt"); onClicked: root.document.setGeometryPlacement({ pivot: "corner" }) }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: qsTrId("EntityInfo.rotatia_si_scara_se_fac_in_jurul_pivotului_implic")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        }
    }

    Toggle {
        visible: root.info.edgesSoft !== undefined && root.modeling
        width: parent.width
        text: root.info.edgesSoft === -1 ? qsTrId("EntityInfo.muchii_netede_mixt") : qsTrId("EntityInfo.muchii_netede_soft")
        checked: root.info.edgesSoft === 1
        onToggled: root.document.setSelectedEdgesSoft(checked)
    }

    // Poziționarea texturii pe fețele selectate (Position Texture): deplasare, rotație, scară.
    PropertyGroup {
        id: textureBox
        visible: root.info.kind === "faces" && root.info.texture !== undefined && root.modeling
        readonly property var t: root.info.texture ?? ({})
        readonly property real half: (width - Theme.space1) / 2
        title: qsTrId("EntityInfo.pozitia_texturii")
        actions: [
            IconButton {
                width: 20; height: 20; iconSize: 13
                iconName: "image"
                tooltip: qsTrId("EntityInfo.pozitioneaza_cu_mouse_ul_trage_shift_roteste_scal")
                onClicked: root.toolRequested("texture")
            },
            IconButton {
                width: 20; height: 20; iconSize: 13
                iconName: "rotate-ccw"
                tooltip: qsTrId("EntityInfo.reseteaza_pozitia_texturii")
                onClicked: root.document.setFaceTexture({ reset: true })
            }
        ]
        Row {
            spacing: Theme.space1
            NumberField { width: textureBox.half; compact: true; label: qsTrId("EntityInfo.x"); unit: "mm"; step: 10; value: textureBox.t.ox ?? 0; onValueModified: (v) => root.document.setFaceTexture({ ox: v }) }
            NumberField { width: textureBox.half; compact: true; label: qsTrId("EntityInfo.y"); unit: "mm"; step: 10; value: textureBox.t.oy ?? 0; onValueModified: (v) => root.document.setFaceTexture({ oy: v }) }
        }
        Row {
            spacing: Theme.space1
            NumberField { width: textureBox.half; compact: true; label: qsTrId("EntityInfo.rot"); unit: "°"; decimals: 1; step: 5; value: textureBox.t.rotation ?? 0; onValueModified: (v) => root.document.setFaceTexture({ rotation: v }) }
            NumberField { width: textureBox.half; compact: true; label: qsTrId("EntityInfo.scara_2"); unit: "×"; decimals: 2; step: 0.1; from: 0.01; value: textureBox.t.sx ?? 1; onValueModified: (v) => root.document.setFaceTexture({ sx: v, sy: v }) }
        }
    }

    // Tag-ul: „pastile” cu tag-urile modelului (pentru obiecte, grupuri, fețe și muchii).
    PropertyGroup {
        visible: (root.isInstance || root.info.kind === "faces" || root.info.kind === "edges") && root.modeling
        title: qsTrId("EntityInfo.tag")
        Flow {
            width: parent.width
            spacing: Theme.space1 + 2

            Repeater {
                model: [{ id: 0, name: qsTrId("EntityInfo.fara_tag"), color: "transparent" }].concat(root.document ? root.document.tags : [])
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
        title: qsTrId("EntityInfo.interactiune")

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
            model: [qsTrId("EntityInfo.niciuna"), qsTrId("EntityInfo.balama"), qsTrId("EntityInfo.glisare")]
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
            model: [qsTrId("EntityInfo.stanga"), qsTrId("EntityInfo.dreapta"), qsTrId("EntityInfo.sus"), qsTrId("EntityInfo.jos")]
            currentIndex: Math.max(0, behaviorBox.rotateAnchors.indexOf(behaviorBox.behavior.anchor ?? 0))
            onCurrentIndexChanged: if (behaviorBox.kindIndex === 1 && behaviorBox.rotateAnchors[currentIndex] !== behaviorBox.behavior.anchor)
                behaviorBox.apply({ anchor: behaviorBox.rotateAnchors[currentIndex] })
        }
        Flow {
            visible: behaviorBox.kindIndex === 2
            width: parent.width
            spacing: Theme.space1
            Repeater {
                model: [qsTrId("EntityInfo.fata"), qsTrId("EntityInfo.spate"), qsTrId("EntityInfo.stanga"), qsTrId("EntityInfo.dreapta"), qsTrId("EntityInfo.sus"), qsTrId("EntityInfo.jos")]
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
                label: behaviorBox.kindIndex === 1 ? qsTrId("EntityInfo.unghi_2") : qsTrId("EntityInfo.distanta")
                unit: behaviorBox.kindIndex === 1 ? "°" : "mm"
                from: -100000
                to: 100000
                value: behaviorBox.behavior.amount ?? 0
                onValueModified: (v) => behaviorBox.apply({ amount: v })
            }
            NumberField {
                width: (parent.width - Theme.space2) * 0.45
                label: qsTrId("EntityInfo.durata")
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
            text: qsTrId("EntityInfo.cu_unealta_interact_mana_din_toolbar_un_click_pe_")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Button {
            width: parent.width
            text: root.isInstance && (root.info.ruleCount ?? 0) > 0 ? qsTrId("EntityInfo.interactiuni_cu_blocuri") + root.info.ruleCount + ")…"
                                                                    : qsTrId("EntityInfo.interactiuni_cu_blocuri_2")
            onClicked: root.editRules()
        }
    }

    Toggle {
        visible: root.isInstance && root.modeling
        width: parent.width
        text: qsTrId("EntityInfo.blocat")
        checked: root.isInstance && root.info.locked === true
        onToggled: root.document.setSelectionLocked(checked)
    }

    Flow {
        visible: root.document && root.document.hasInstanceSelected && root.modeling
        width: parent.width
        spacing: Theme.space2

        Button {
            small: true
            text: qsTrId("EntityInfo.editeaza")
            visible: root.isInstance && root.info.locked !== true
            onClicked: root.document.enterInstance(0)
        }
        Button {
            small: true
            text: qsTrId("EntityInfo.desface")
            onClicked: root.commandRequested("org.explode")
        }
        Button {
            small: true
            text: qsTrId("EntityInfo.fa_unic")
            // Copii legate (×N) sau părți comune cu alte obiecte: separarea îi dă o copie proprie, cu animațiile
            // și interacțiunile ei.
            visible: root.isInstance && root.info.canMakeUnique === true
            onClicked: root.document.makeUniqueSelection()
        }
    }
}
