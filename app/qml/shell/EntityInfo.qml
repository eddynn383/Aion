import QtQuick
import Aion

// Entity Info: proprietățile selecției. Pentru un grup sau o componentă: nume, tag, dimensiuni, blocare și
// comenzile de organizare; pentru fețe aria, pentru muchii lungimea.
Column {
    id: root

    property var document
    // Timeline-ul e deschis: la hover pe o valoare de plasare, eticheta devine butonul de keyframe.
    property bool timelineActive: false
    signal editRules()

    readonly property var info: document ? document.entityInfo : ({ kind: "none" })
    readonly property bool isInstance: info.kind === "instance"

    spacing: Theme.space3

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

    // Detaliile: eticheta în stânga și valoarea în dreapta; dacă nu încap pe un rând, valoarea trece dedesubt.
    // Pozițiile sunt calculate direct (nu prin ancore schimbate din mers, care pot rămâne agățate).
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

    // Materialul grupului sau al obiectului (fețele fără material proprii îl preiau).
    MaterialSwatch {
        visible: root.isInstance
        width: parent.width
        label: "Material"
        document: root.document
        material: root.isInstance && root.info.material ? root.info.material : ({ id: 0, name: "Implicit" })
        onPicked: (m) => root.document.assignMaterial(m, "front")
    }

    // Fețele: materialul pe față și pe verso (Paint Bucket vopsește partea pe care o vezi).
    Column {
        visible: root.info.kind === "faces"
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

    // Transformarea: poziția colțului, dimensiunile, rotația (Euler XYZ) și scara, câte trei valori. Fiecare are un
    // lacăt (blochează valoarea, inclusiv pentru Move / Rotate / Scale) și, cu timeline-ul deschis, un keyframe la hover.
    Column {
        id: transformBox
        visible: root.isInstance
        width: parent.width
        spacing: Theme.space2

        readonly property var p: root.isInstance && root.info.placement ? root.info.placement : ({})
        readonly property int locks: root.isInstance ? (root.info.locks ?? 0) : 0
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

        component Caption: Item {
            property alias text: captionText.text
            default property alias trailing: trailingRow.data
            width: transformBox.width
            height: Theme.controlHeightSm
            Text {
                id: captionText
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
            }
            Row {
                id: trailingRow
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        Caption { text: "Poziție (mm)" }
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 0; value: transformBox.p.px ?? 0; onValueModified: (v) => root.document.setPlacement({ px: v }) }
            PlacementField { label: "Y"; lockBit: 1; value: transformBox.p.py ?? 0; onValueModified: (v) => root.document.setPlacement({ py: v }) }
            PlacementField { label: "Z"; lockBit: 2; value: transformBox.p.pz ?? 0; onValueModified: (v) => root.document.setPlacement({ pz: v }) }
        }

        Caption {
            text: "Dimensiuni (mm)"
            IconButton {
                width: 24; height: 24; iconSize: Theme.iconSizeSm
                iconName: "link"
                tooltip: transformBox.linkProportions ? "Proporții legate" : "Leagă proporțiile"
                checked: transformBox.linkProportions
                solidChecked: false
                onClicked: transformBox.linkProportions = !transformBox.linkProportions
            }
        }
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 3; from: 0.01; value: transformBox.p.dx ?? 0; onValueModified: (v) => transformBox.setSize("dx", v) }
            PlacementField { label: "Y"; lockBit: 4; from: 0.01; value: transformBox.p.dy ?? 0; onValueModified: (v) => transformBox.setSize("dy", v) }
            PlacementField { label: "Z"; lockBit: 5; from: 0.01; value: transformBox.p.dz ?? 0; onValueModified: (v) => transformBox.setSize("dz", v) }
        }

        Caption { text: "Rotație (°)" }
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 6; decimals: 1; from: -360; to: 360; value: transformBox.p.rx ?? 0; onValueModified: (v) => root.document.setPlacement({ rx: v }) }
            PlacementField { label: "Y"; lockBit: 7; decimals: 1; from: -360; to: 360; value: transformBox.p.ry ?? 0; onValueModified: (v) => root.document.setPlacement({ ry: v }) }
            PlacementField { label: "Z"; lockBit: 8; decimals: 1; from: -360; to: 360; value: transformBox.p.rz ?? 0; onValueModified: (v) => root.document.setPlacement({ rz: v }) }
        }

        Caption { text: "Scară (×)" }
        Row {
            spacing: Theme.space1
            PlacementField { label: "X"; lockBit: 9; decimals: 3; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sx ?? 1); onValueModified: (v) => root.document.setPlacement({ sx: v }) }
            PlacementField { label: "Y"; lockBit: 10; decimals: 3; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sy ?? 1); onValueModified: (v) => root.document.setPlacement({ sy: v }) }
            PlacementField { label: "Z"; lockBit: 11; decimals: 3; step: 0.1; from: 0.001; value: Math.abs(transformBox.p.sz ?? 1); onValueModified: (v) => root.document.setPlacement({ sz: v }) }
        }
    }

    // Tag-ul: „pastile” cu tag-urile modelului (pentru obiecte, grupuri, fețe și muchii).
    Column {
        visible: root.isInstance || root.info.kind === "faces" || root.info.kind === "edges"
        width: parent.width
        spacing: Theme.space2

        Text {
            text: "Tag"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeSm
        }
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
    Column {
        id: behaviorBox
        visible: root.isInstance
        width: parent.width
        spacing: Theme.space2

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

        Text {
            text: "Interacțiune"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeSm
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
        visible: root.isInstance
        width: parent.width
        text: "Blocat"
        checked: root.isInstance && root.info.locked === true
        onToggled: root.document.setSelectionLocked(checked)
    }

    Flow {
        visible: root.document && root.document.hasInstanceSelected
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
            onClicked: root.document.explodeSelection()
        }
        Button {
            small: true
            text: "Fă unic"
            visible: root.isInstance && root.info.title === "Obiect" && root.info.copies > 1
            onClicked: root.document.makeUniqueSelection()
        }
    }
}
