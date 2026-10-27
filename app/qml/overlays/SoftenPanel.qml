import QtQuick
import QtQuick.Templates as T
import Aion

// Soften / Smooth: muchiile dintre fețe cu unghi mai mic decât pragul devin soft (ascunse, cu umbrire netedă).
// Se aplică pe muchiile selecției sau pe toate muchiile obiectelor selectate, cu previzualizare.
Popover {
    id: root

    property var document
    property real angle: 20
    property bool coplanar: true

    function start() {
        root.open()
        root.update()
    }
    function update() {
        if (root.visible)
            root.document.previewSoften(root.angle, root.coplanar)
    }

    modal: false
    closePolicy: T.Popup.NoAutoClose
    width: 300
    padding: Theme.space4

    contentItem: Column {
        spacing: Theme.space3
        Text {
            text: "Soften / Smooth"
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
            text: "Sub prag, muchia dispare și umbrirea trece lin peste ea (ca la un cilindru). 0° = toate muchiile vizibile."
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
