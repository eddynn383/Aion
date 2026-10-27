import QtQuick
import Aion

// Materialele din model și biblioteca inclusă. Click pe un material îl alege și activează Paint Bucket; „+” din antet
// creează un material personalizat, iar dublu-click (sau click dreapta) pe unul din model îl editează.
Item {
    id: root

    property var document
    signal paintRequested()

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
                placeholderText: "Caută un material"
            }

            Text {
                visible: root.document.modelMaterials.length > 0 || root.query.length === 0
                text: "În model"
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
                text: "Niciun material în model. Alege unul din bibliotecă sau creează-l pe al tău cu +."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }

            Text {
                text: "Bibliotecă"
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
            hex.text = color.toString().toUpperCase()
            open()
        }
        function properties() {
            return { name: editor.name, color: editor.color.toString(), opacity: editor.alpha,
                     roughness: editor.roughness, metallic: editor.metallic }
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
                    text: editor.materialId > 0 ? "Editează materialul" : "Material nou"
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    font.weight: Font.DemiBold
                }
            }
            Input {
                width: parent.width
                placeholderText: "Nume (de exemplu „Stejar natur”)"
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
            Slider {
                width: parent.width
                label: "Opacitate"
                from: 0.05
                to: 1
                value: editor.alpha
                valueText: Math.round(value * 100) + "%"
                onMoved: editor.alpha = value
            }
            Slider {
                width: parent.width
                label: "Rugozitate"
                from: 0
                to: 1
                value: editor.roughness
                valueText: Math.round(value * 100) + "%"
                onMoved: editor.roughness = value
            }
            Slider {
                width: parent.width
                label: "Metalic"
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
                    text: "Șterge"
                    onClicked: { root.document.removeMaterial(editor.materialId); editor.close() }
                }
                Item { width: parent.width - (editor.materialId > 0 ? 180 : 0) - 180; height: 1 }
                Button {
                    text: "Anulează"
                    onClicked: editor.close()
                }
                Button {
                    variant: "primary"
                    text: editor.materialId > 0 ? "Salvează" : "Creează"
                    onClicked: editor.save()
                }
            }
        }
    }
}
