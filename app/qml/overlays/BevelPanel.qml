import QtQuick
import QtQuick.Templates as T
import Aion

// Chamfer / Bevel: muchiile selectate (sau laturile feței selectate) se teșesc plat ori se rotunjesc,
// cu previzualizare. Distanța e retragerea pe fiecare față vecină.
Popover {
    id: root

    property var document
    property int mode: 0 // 0 = chamfer, 1 = bevel
    property real distance: 50
    property int segments: 6

    function start(bevel) {
        root.mode = bevel ? 1 : 0
        root.open()
        root.update()
    }
    function update() {
        if (root.visible)
            root.document.previewBevel(root.distance, root.mode === 1 ? root.segments : 1)
    }
    onModeChanged: update()
    onDistanceChanged: update()
    onSegmentsChanged: update()

    modal: false
    closePolicy: T.Popup.NoAutoClose
    width: 300
    padding: Theme.space4

    contentItem: Column {
        spacing: Theme.space3
        Text {
            text: root.mode === 1 ? "Bevel" : "Chamfer"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Tabs {
            width: parent.width
            model: ["Chamfer (plat)", "Bevel (rotunjit)"]
            currentIndex: root.mode
            onCurrentIndexChanged: root.mode = currentIndex
        }
        NumberField {
            width: parent.width
            label: "Distanță"
            unit: "mm"
            from: 0.1
            to: 100000
            decimals: 1
            value: root.distance
            onValueModified: (v) => root.distance = v
        }
        Slider {
            width: parent.width
            visible: root.mode === 1
            label: "Segmente"
            from: 2
            to: 24
            stepSize: 1
            value: root.segments
            valueText: Math.round(value)
            onMoved: root.segments = Math.round(value)
        }
        Text {
            width: parent.width
            wrapMode: Text.Wrap
            text: "Merge pe muchii convexe dintre două fețe plane (ca la o cutie). Pe o față se teșesc toate laturile ei."
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
