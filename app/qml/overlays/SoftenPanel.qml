import QtQuick
import QtQuick.Templates as T
import Aion

// Muchii netede (Soften): muchiile dintre fețe cu unghi mai mic decât pragul devin soft (ascunse, cu umbrire netedă).
// Se aplică pe muchiile selecției sau pe toate muchiile obiectelor selectate, cu previzualizare.
Popover {
    id: root

    property var document
    property real angle: 20
    property bool coplanar: true
    // Rezultatul ultimei previzualizări: { soft, total }.
    property var result: ({})

    function start() {
        root.open()
        root.update()
    }
    function update() {
        if (root.visible)
            root.result = root.document.previewSoften(root.angle, root.coplanar)
    }

    modal: false
    closePolicy: T.Popup.NoAutoClose
    width: 300
    padding: Theme.space4

    contentItem: Column {
        spacing: Theme.space3
        Text {
            text: "Muchii netede"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Slider {
            width: parent.width
            label: "Unghi maxim între fețe"
            from: 0
            to: 180
            stepSize: 1
            value: root.angle
            valueText: Math.round(value) + "°"
            onMoved: { root.angle = value; root.update() }
        }
        Toggle {
            width: parent.width
            text: "Și muchiile dintre fețe coplanare"
            checked: root.coplanar
            onToggled: { root.coplanar = checked; root.update() }
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            visible: root.result.total !== undefined
            text: root.result.total === 0 ? "Selecția nu are muchii între două fețe."
                : root.result.soft > 0 ? root.result.soft + " din " + root.result.total + " muchii sunt netezite."
                : "Nicio muchie nu e sub prag. Muchiile unui cub au 90°: ridică pragul peste 90°."
            color: root.result.soft > 0 ? Theme.textSecondary : Theme.textPrimary
            font.pixelSize: Theme.fontSizeSm
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Sub prag, muchia dispare și umbrirea trece lin peste ea (ca la un cilindru). Nu adaugă fețe: ca obiectul să devină "
                + "mai rotund, folosește Subdivide. 0° = toate muchiile vizibile."
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
