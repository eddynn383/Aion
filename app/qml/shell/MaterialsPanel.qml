import QtQuick
import Aion

// Materialele: cel curent (cu care vopsește Paint Bucket), cele din model și biblioteca inclusă.
// Click pe un material îl alege și activează Paint Bucket; dublu-click pe unul din model îl redenumește.
Item {
    id: root

    property var document
    signal paintRequested()

    readonly property string query: search.text.trim().toLowerCase()
    function matches(m) { return root.query.length === 0 || m.name.toLowerCase().indexOf(root.query) >= 0 }

    component Swatch: Item {
        id: swatch
        property var material
        property bool current: false
        signal clicked()
        signal doubleClicked()
        width: 56
        height: 74

        Rectangle {
            id: ball
            anchors.horizontalCenter: parent.horizontalCenter
            width: 48
            height: 48
            radius: 24
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
            onClicked: swatch.clicked()
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
            spacing: Theme.space4

            // Materialul curent.
            Row {
                spacing: Theme.space3
                Rectangle {
                    width: 32
                    height: 32
                    radius: 16
                    anchors.verticalCenter: parent.verticalCenter
                    color: root.document.currentMaterial.color.a === 0 ? Theme.bgHover : root.document.currentMaterial.color
                    border.color: Theme.borderStrong
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: root.document.currentMaterial.name
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeMd
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: root.document.currentMaterial.source === "library" ? "Din bibliotecă" : root.document.currentMaterial.source === "model" ? "În model" : "Fără material"
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
            }

            Input {
                id: search
                width: parent.width
                iconName: "search"
                placeholderText: "Caută un material"
            }

            Text {
                text: "În model"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.DemiBold
            }
            Flow {
                width: parent.width
                spacing: Theme.space2
                Swatch {
                    material: ({ name: "Implicit", color: Qt.rgba(0, 0, 0, 0), metallic: 0 })
                    current: root.document.currentMaterial.source === "default"
                    onClicked: { root.document.selectDefaultMaterial(); root.paintRequested() }
                }
                Repeater {
                    model: root.document.modelMaterials.filter(m => root.matches(m))
                    delegate: Swatch {
                        required property var modelData
                        material: modelData
                        current: root.document.currentMaterial.source === "model" && root.document.currentMaterial.id === modelData.id
                        onClicked: { root.document.selectModelMaterial(modelData.id); root.paintRequested() }
                        onDoubleClicked: { rename.materialId = modelData.id; rename.text = modelData.name; rename.visible = true; rename.forceActiveFocus() }
                    }
                }
            }
            Input {
                id: rename
                property double materialId: 0
                visible: false
                width: parent.width
                placeholderText: "Numele materialului"
                onAccepted: { root.document.renameMaterial(materialId, text); visible = false }
                onActiveFocusChanged: if (!activeFocus) visible = false
            }

            Text {
                text: "Bibliotecă"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.DemiBold
            }
            Flow {
                width: parent.width
                spacing: Theme.space2
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
}
