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
            text: root.mode === 1 ? qsTrId("BevelPanel.bevel") : qsTrId("BevelPanel.chamfer")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Tabs {
            width: parent.width
            model: [qsTrId("BevelPanel.chamfer_plat"), qsTrId("BevelPanel.bevel_rotunjit")]
            currentIndex: root.mode
            onCurrentIndexChanged: root.mode = currentIndex
        }
        NumberField {
            width: parent.width
            label: qsTrId("BevelPanel.distanta")
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
            label: qsTrId("BevelPanel.segmente")
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
            text: qsTrId("BevelPanel.merge_pe_muchii_convexe_dintre_doua_fete_plane_ca")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
        Row {
            anchors.right: parent.right
            spacing: Theme.space2
            Button {
                text: qsTrId("BevelPanel.anuleaza")
                onClicked: { root.document.cancelOperation(); root.close() }
            }
            Button {
                variant: "primary"
                text: qsTrId("BevelPanel.gata")
                onClicked: { root.document.commitOperation(); root.close() }
            }
        }
    }
}
