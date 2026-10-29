import QtQuick
import QtQuick.Templates as T
import Aion

// Subdivide: fiecare față se împarte în mai multe și obiectul se rotunjește (Catmull-Clark), cu previzualizare.
// Se aplică pe obiectele selectate (cu subobiectele lor) sau pe fețele conectate de selecție.
Popover {
    id: root

    property var document
    property int levels: 1
    // Rezultatul ultimei previzualizări: { faces, error }.
    property var result: ({})

    function start() {
        root.open()
        root.update()
    }
    function update() {
        if (root.visible)
            root.result = root.document.previewSubdivide(root.levels)
    }

    modal: false
    closePolicy: T.Popup.NoAutoClose
    width: 300
    padding: Theme.space4

    contentItem: Column {
        spacing: Theme.space3
        Text {
            text: "Subdivide"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Slider {
            width: parent.width
            label: "Niveluri"
            from: 1
            to: 3
            stepSize: 1
            value: root.levels
            valueText: Math.round(value)
            onMoved: {
                if (Math.round(value) !== root.levels) {
                    root.levels = Math.round(value)
                    root.update()
                }
            }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            visible: root.result.faces !== undefined
            text: root.result.error ? root.result.error : root.result.faces + " fețe după subdivizare."
            color: root.result.error ? Theme.danger : Theme.textSecondary
            font.pixelSize: Theme.fontSizeSm
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Fiecare nivel împarte fețele în patru și rotunjește forma: un cub devine o pernă, o sferă devine mai fină. "
                + "Pentru muchii drepte păstrate, folosește întâi Bevel pe ele."
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Row {
            anchors.right: parent.right
            spacing: Theme.space2
            Button {
                text: "Anulează"
                onClicked: { root.document.cancelOperation(); root.close() }
            }
            Button {
                variant: "primary"
                text: "Gata"
                onClicked: { root.document.commitOperation(); root.close() }
            }
        }
    }
}
