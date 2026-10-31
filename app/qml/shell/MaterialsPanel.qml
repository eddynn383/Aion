import QtQuick
import QtQuick.Effects
import Aion

// Materialele din model și biblioteca inclusă. Click pe un material îl alege și activează Paint Bucket; „+” din antet
// creează un material personalizat, iar dublu-click (sau click dreapta) pe unul din model îl editează.
Item {
    id: root

    property var document
    signal paintRequested()
    // Un material tras spre viewport (Main desenează fantoma și vopsește la drop): id = „mat:model:ID” sau „mat:lib:INDEX”.
    signal dragStarted(string id, string label)
    signal dragMoved(point windowPosition)
    signal dragEnded()
    // Editorul cere o imagine pentru textură; răspunsul vine prin textureChosen(path).
    signal pickTextureImage()
    function textureChosen(path) {
        editor.texture = "file:" + path
        editor.textureThumbnail = "file://" + path
    }

    // Editorul: un material nou (materialId 0) sau unul din model.
    function newMaterial() { editor.edit(0) }

    readonly property string query: search.text.trim().toLowerCase()
    // Grila umple lățimea: câte coloane încap (minimum 56 px), iar celulele se lățesc egal.
    readonly property real gridSpacing: Theme.space2
    readonly property int columns: Math.max(3, Math.floor((width + gridSpacing) / (56 + gridSpacing)))
    readonly property real cellWidth: Math.floor((width - gridSpacing * (columns - 1)) / columns)
    function matches(m) { return root.query.length === 0 || m.name.toLowerCase().indexOf(root.query) >= 0 }

    component Swatch: Item {
        id: swatch
        property var material
        property bool current: false
        property string dragId: ""
        signal clicked()
        signal doubleClicked()
        signal rightClicked()
        width: root.cellWidth
        height: ball.height + 26

        Rectangle {
            id: ball
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(64, swatch.width - 8)
            height: width
            radius: width / 2
            color: swatch.material.color.a === 0 ? Theme.bgHover : swatch.material.color
            border.color: swatch.current ? Theme.accent : Theme.borderStrong
            border.width: swatch.current ? 2 : 1
            // O „sferă”: lumină sus-stânga, umbră jos-dreapta; metalele au reflexia mai puternică.
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                radius: width / 2
                gradient: Gradient {
                    orientation: Gradient.Vertical
                    GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, swatch.material.metallic > 0.5 ? 0.45 : 0.22) }
                    GradientStop { position: 0.55; color: Qt.rgba(1, 1, 1, 0) }
                    GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.28) }
                }
            }
            // Textura (miniatura), decupată în cerc.
            Image {
                id: thumb
                visible: false
                anchors.fill: parent
                source: swatch.material.thumbnail ?? ""
                fillMode: Image.PreserveAspectCrop
            }
            Rectangle {
                id: thumbMask
                anchors.fill: parent
                radius: width / 2
                visible: false
                layer.enabled: true
            }
            MultiEffect {
                visible: (swatch.material.thumbnail ?? "").length > 0
                anchors.fill: parent
                source: thumb
                maskEnabled: true
                maskSource: thumbMask
            }
            Text {
                visible: swatch.material.color.a === 0
                anchors.centerIn: parent
                text: "—"
                color: Theme.textTertiary
            }
        }
        Text {
            anchors.top: ball.bottom
            anchors.topMargin: Theme.space1
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            text: swatch.material.name
            color: swatch.current ? Theme.textPrimary : Theme.textSecondary
            font.pixelSize: Theme.fontSizeXs
        }
        DragHandler {
            target: null
            enabled: swatch.dragId.length > 0
            onActiveChanged: active ? root.dragStarted(swatch.dragId, swatch.material.name) : root.dragEnded()
            onCentroidChanged: if (active) root.dragMoved(swatch.mapToItem(null, centroid.position.x, centroid.position.y))
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => mouse.button === Qt.RightButton ? swatch.rightClicked() : swatch.clicked()
            onDoubleClicked: swatch.doubleClicked()
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            width: parent.width
            spacing: Theme.space3

            Input {
                id: search
                width: parent.width
                iconName: "search"
                placeholderText: qsTrId("MaterialsPanel.cauta_un_material")
            }

            Text {
                visible: root.document.modelMaterials.length > 0 || root.query.length === 0
                text: qsTrId("MaterialsPanel.in_model")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.Medium
            }
            Flow {
                width: parent.width
                spacing: root.gridSpacing
                Repeater {
                    model: root.document.modelMaterials.filter(m => root.matches(m))
                    delegate: Swatch {
                        required property var modelData
                        material: modelData
                        dragId: "mat:model:" + modelData.id
                        current: root.document.currentMaterial.source === "model" && root.document.currentMaterial.id === modelData.id
                        onClicked: { root.document.selectModelMaterial(modelData.id); root.paintRequested() }
                        onDoubleClicked: editor.edit(modelData.id)
                        onRightClicked: editor.edit(modelData.id)
                    }
                }
            }
            Text {
                visible: root.document.modelMaterials.length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTrId("MaterialsPanel.niciun_material_in_model_alege_unul_din_bibli")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }

            Text {
                text: qsTrId("MaterialsPanel.biblioteca")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.Medium
            }
            Flow {
                width: parent.width
                spacing: root.gridSpacing
                Repeater {
                    model: root.document.libraryMaterials.filter(m => root.matches(m))
                    delegate: Swatch {
                        required property var modelData
                        material: modelData
                        dragId: "mat:lib:" + modelData.index
                        current: root.document.currentMaterial.source === "library" && root.document.currentMaterial.index === modelData.index
                        onClicked: { root.document.selectLibraryMaterial(modelData.index); root.paintRequested() }
                    }
                }
            }
        }
    }

    // Editorul de material: nume, culoare, opacitate, rugozitate, metalic, cu previzualizare.
    Popover {
        id: editor
        property double materialId: 0
        property string name: ""
        property color color: "#B0B0B0"
        property real alpha: 1
        property real roughness: 0.6
        property real metallic: 0
        // Categoria (ADR 0044): decide cum apar în lista de debitare obiectele fără date de piesă.
        property string category: ""
        readonly property var categories: [
            { value: "", label: qsTrId("MaterialsPanel.fara") }, { value: "board-decor", label: qsTrId("MaterialsPanel.decor_de_placa") },
            { value: "wood", label: qsTrId("MaterialsPanel.lemn_masiv") }, { value: "metal", label: qsTrId("MaterialsPanel.metal") }, { value: "glass", label: qsTrId("MaterialsPanel.sticla") },
            { value: "plastic", label: qsTrId("MaterialsPanel.plastic") }, { value: "stone", label: qsTrId("MaterialsPanel.piatra") } ]
        // Textura: "" = fără, „procedural:brick”…, „file:/cale”; mărimea unei repetiții, în mm.
        property string texture: ""
        property string textureThumbnail: ""
        property real textureWidth: 1000
        property real textureHeight: 1000
        readonly property var textures: [
            { value: "", label: qsTrId("MaterialsPanel.fara") }, { value: "procedural:brick", label: qsTrId("MaterialsPanel.caramida") },
            { value: "procedural:parquet", label: qsTrId("MaterialsPanel.parchet") }, { value: "procedural:wood", label: qsTrId("MaterialsPanel.lemn") },
            { value: "procedural:concrete", label: qsTrId("MaterialsPanel.beton") }, { value: "procedural:tiles", label: qsTrId("MaterialsPanel.gresie") } ]
        readonly property var presets: ["#F5F5F4", "#A8A29E", "#44403C", "#1C1917", "#B45309", "#92400E", "#DC2626",
            "#EA580C", "#CA8A04", "#16A34A", "#0891B2", "#2563EB", "#7C3AED", "#DB2777"]

        function edit(id) {
            materialId = id
            const m = id > 0 ? root.document.modelMaterials.find(x => x.id === id) : null
            name = m ? m.name : ""
            color = m ? Qt.rgba(m.color.r, m.color.g, m.color.b, 1) : "#B0B0B0"
            alpha = m ? m.color.a : 1
            roughness = m ? m.roughness : 0.6
            metallic = m ? m.metallic : 0
            category = m ? (m.category ?? "") : ""
            texture = m && m.textureSource ? (m.textureSource.startsWith("procedural:") ? m.textureSource : "file:" + m.textureSource) : ""
            textureThumbnail = m ? (m.thumbnail ?? "") : ""
            textureWidth = m ? (m.textureWidth ?? 1000) : 1000
            textureHeight = m ? (m.textureHeight ?? 1000) : 1000
            hex.text = color.toString().toUpperCase()
            open()
        }
        function properties() {
            return { name: editor.name, color: editor.color.toString(), opacity: editor.alpha,
                     roughness: editor.roughness, metallic: editor.metallic, category: editor.category,
                     texture: editor.texture,
                     textureWidth: editor.textureWidth, textureHeight: editor.textureHeight }
        }
        function save() {
            if (materialId > 0)
                root.document.updateMaterial(materialId, properties())
            else
                root.document.createMaterial(properties())
            close()
            if (materialId === 0)
                root.paintRequested()
        }

        parent: root
        x: 0
        y: Theme.space2
        width: root.width
        padding: Theme.space3
        modal: true

        contentItem: Column {
            spacing: Theme.space3
            Row {
                spacing: Theme.space3
                Rectangle {
                    width: 44
                    height: 44
                    radius: 22
                    color: Qt.rgba(editor.color.r, editor.color.g, editor.color.b, editor.alpha)
                    border.color: Theme.borderStrong
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        radius: width / 2
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, 0.12 + (1 - editor.roughness) * 0.3 + editor.metallic * 0.15) }
                            GradientStop { position: 0.55; color: Qt.rgba(1, 1, 1, 0) }
                            GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, 0.28) }
                        }
                    }
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: editor.materialId > 0 ? qsTrId("MaterialsPanel.editeaza_materialul") : qsTrId("MaterialsPanel.material_nou")
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.DemiBold
                }
            }
            Input {
                width: parent.width
                placeholderText: qsTrId("MaterialsPanel.nume_de_exemplu_stejar_natur")
                text: editor.name
                onTextChanged: editor.name = text
            }
            Flow {
                width: parent.width
                spacing: 6
                Repeater {
                    model: editor.presets
                    delegate: Rectangle {
                        required property string modelData
                        readonly property bool current: editor.color.toString().toUpperCase() === modelData
                        width: 20
                        height: 20
                        radius: 10
                        color: modelData
                        border.width: current ? 2 : 1
                        border.color: current ? Theme.accent : Theme.borderStrong
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { editor.color = parent.modelData; hex.text = parent.modelData }
                        }
                    }
                }
            }
            Input {
                id: hex
                width: parent.width
                placeholderText: "#RRGGBB"
                onTextChanged: if (/^#[0-9a-fA-F]{6}$/.test(text)) editor.color = text
            }
            // Categoria.
            Text {
                text: qsTrId("MaterialsPanel.categorie_pentru_lista_de_debitare")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
            }
            Flow {
                width: parent.width
                spacing: 4
                Repeater {
                    model: editor.categories
                    delegate: Rectangle {
                        id: categoryChip
                        required property var modelData
                        readonly property bool current: editor.category === modelData.value
                        width: categoryText.implicitWidth + Theme.space3 * 2
                        height: 24
                        radius: 12
                        color: current ? Theme.accentSubtle : categoryArea.containsMouse ? Theme.bgHover : "transparent"
                        border.color: current ? Theme.accent : Theme.borderStrong
                        Text {
                            id: categoryText
                            anchors.centerIn: parent
                            text: categoryChip.modelData.label
                            color: categoryChip.current ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeXs
                        }
                        MouseArea {
                            id: categoryArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: editor.category = categoryChip.modelData.value
                        }
                    }
                }
            }
            // Textura.
            Text {
                text: qsTrId("MaterialsPanel.textura")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
            }
            Flow {
                width: parent.width
                spacing: 4
                Repeater {
                    model: editor.textures.concat([{ value: "file", label: qsTrId("MaterialsPanel.imagine") }])
                    delegate: Rectangle {
                        id: chip
                        required property var modelData
                        readonly property bool current: modelData.value === "file" ? editor.texture.startsWith("file:")
                                                                                   : editor.texture === modelData.value
                        width: chipText.implicitWidth + Theme.space3 * 2
                        height: 24
                        radius: 12
                        color: current ? Theme.accentSubtle : chipArea.containsMouse ? Theme.bgHover : "transparent"
                        border.color: current ? Theme.accent : Theme.borderStrong
                        Text {
                            id: chipText
                            anchors.centerIn: parent
                            text: chip.modelData.label
                            color: chip.current ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeXs
                        }
                        MouseArea {
                            id: chipArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: {
                                if (chip.modelData.value === "file") {
                                    root.pickTextureImage()
                                } else {
                                    editor.texture = chip.modelData.value
                                    editor.textureThumbnail = ""
                                    // Materialele cu textură pornesc de la alb (textura neschimbată).
                                    if (editor.texture.length > 0 && editor.materialId === 0) {
                                        editor.color = "#FFFFFF"
                                        hex.text = "#FFFFFF"
                                    }
                                }
                            }
                        }
                    }
                }
            }
            Row {
                visible: editor.texture.length > 0
                width: parent.width
                spacing: Theme.space1
                NumberField {
                    width: (parent.width - Theme.space1) / 2
                    label: qsTrId("MaterialsPanel.l")
                    unit: "mm"
                    from: 1
                    step: 50
                    value: editor.textureWidth
                    onValueModified: (v) => editor.textureWidth = v
                }
                NumberField {
                    width: (parent.width - Theme.space1) / 2
                    label: qsTrId("MaterialsPanel.h")
                    unit: "mm"
                    from: 1
                    step: 50
                    value: editor.textureHeight
                    onValueModified: (v) => editor.textureHeight = v
                }
            }
            Text {
                visible: editor.texture.length > 0
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTrId("MaterialsPanel.marimea_reala_a_unei_repetitii_culoarea_nuant")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
            Slider {
                width: parent.width
                label: qsTrId("MaterialsPanel.opacitate")
                from: 0.05
                to: 1
                value: editor.alpha
                valueText: Math.round(value * 100) + "%"
                onMoved: editor.alpha = value
            }
            Slider {
                width: parent.width
                label: qsTrId("MaterialsPanel.rugozitate")
                from: 0
                to: 1
                value: editor.roughness
                valueText: Math.round(value * 100) + "%"
                onMoved: editor.roughness = value
            }
            Slider {
                width: parent.width
                label: qsTrId("MaterialsPanel.metalic")
                from: 0
                to: 1
                value: editor.metallic
                valueText: Math.round(value * 100) + "%"
                onMoved: editor.metallic = value
            }
            Row {
                width: parent.width
                spacing: Theme.space2
                Button {
                    visible: editor.materialId > 0
                    text: qsTrId("MaterialsPanel.sterge")
                    onClicked: { root.document.removeMaterial(editor.materialId); editor.close() }
                }
                Item { width: parent.width - (editor.materialId > 0 ? 180 : 0) - 180; height: 1 }
                Button {
                    text: qsTrId("MaterialsPanel.anuleaza")
                    onClicked: editor.close()
                }
                Button {
                    variant: "primary"
                    text: editor.materialId > 0 ? qsTrId("MaterialsPanel.salveaza") : qsTrId("MaterialsPanel.creeaza")
                    onClicked: editor.save()
                }
            }
        }
    }
}
