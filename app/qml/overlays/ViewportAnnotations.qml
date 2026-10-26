import QtQuick
import Aion

// Etichetele din viewport: tipul inferenței („Endpoint”, „On Red Axis”), cotele live ale segmentelor desenate
// și adnotările din model (cote, texte). Pozițiile vin din Viewport.annotations.
// O adnotare se editează cu dublu-click (sau imediat după unealta Text) și se șterge cu „×”.
Item {
    id: root

    property var annotations: []
    property var document

    Repeater {
        model: root.annotations

        delegate: Rectangle {
            id: pill

            required property var modelData
            readonly property bool inference: modelData.kind === "inference"
            readonly property bool note: modelData.kind === "note"
            readonly property bool editing: note && root.document && root.document.editingAnnotation === modelData.id

            // Inferența: deasupra-dreapta marcajului (sub cursor stă Measurements box); restul: centrate.
            x: inference ? modelData.x + 12 : modelData.x - width / 2
            y: inference ? modelData.y - 12 - height : modelData.y - height / 2
            implicitWidth: (editing ? Math.max(editor.implicitWidth, 120) : row.implicitWidth) + Theme.space2 * 2
            implicitHeight: 22
            radius: note ? Theme.radiusSm : height / 2
            color: Theme.bgElevated
            border.color: editing ? Theme.accent : Theme.borderStrong
            opacity: 0.96

            Row {
                id: row
                visible: !pill.editing
                anchors.centerIn: parent
                spacing: Theme.space1 + 2

                Rectangle {
                    visible: pill.inference
                    width: 8
                    height: 8
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: pill.modelData.color ?? Theme.textPrimary
                }
                Icon {
                    visible: pill.inference && pill.modelData.locked === true
                    name: "lock"
                    size: 11
                    color: Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: pill.modelData.text
                    color: pill.inference || pill.note ? Theme.textPrimary : Theme.textSecondary
                    font.family: pill.inference || (pill.note && !pill.modelData.dimension) ? Theme.fontUi : Theme.fontMono
                    font.pixelSize: Theme.fontSizeXs
                }
            }

            TextInput {
                id: editor
                visible: pill.editing
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
                text: pill.modelData.text
                color: Theme.textPrimary
                selectionColor: Theme.accentSubtle
                font.pixelSize: Theme.fontSizeXs
                onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                onAccepted: root.document.setAnnotationText(pill.modelData.id, text)
                Keys.onEscapePressed: root.document.editingAnnotation = 0
            }

            MouseArea {
                id: noteArea
                visible: pill.note && !pill.editing
                anchors.fill: parent
                hoverEnabled: true
                onDoubleClicked: root.document.editingAnnotation = pill.modelData.id
            }

            // Ștergerea unei adnotări.
            Rectangle {
                visible: pill.note && !pill.editing && (noteArea.containsMouse || deleteArea.containsMouse)
                anchors.left: parent.right
                anchors.leftMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                radius: 9
                color: Theme.bgElevated
                border.color: Theme.borderStrong
                Icon {
                    anchors.centerIn: parent
                    name: "x"
                    size: 11
                    color: Theme.textSecondary
                }
                MouseArea {
                    id: deleteArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.document.removeAnnotation(pill.modelData.id)
                }
            }
        }
    }
}
