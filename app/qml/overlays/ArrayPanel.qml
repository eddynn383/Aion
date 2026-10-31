import QtQuick
import QtQuick.Templates as T
import Aion

// Panoul Array: copii ale selecției în linie (pe o axă, cu pas) sau în cerc (în jurul unei axe). Fiecare schimbare
// reface previzualizarea în model; „Gata” păstrează rezultatul ca un singur pas de undo, „Anulează” îl scoate.
Popover {
    id: root

    property var document
    property int mode: 0 // 0 = liniar, 1 = polar
    property int count: 3
    property int axis: 0
    property real stepLength: 1000
    property int polarAxis: 2
    property real angle: 90
    property bool fullCircle: true
    property real radius: 1000
    property bool rotateCopies: true

    function start() {
        const box = root.document.selectionBox()
        if (box.dx === undefined) {
            root.document.previewArray({}) // arată mesajul „selectează…”
            return
        }
        // Pasul implicit: lățimea selecției pe axă plus un spațiu, ca obiectele să stea alături.
        root.stepLength = Math.round(Math.max(box.dx, 1) + 100)
        root.open()
        root.update()
    }
    function update() {
        if (!root.visible)
            return
        const step = [0, 0, 0]
        step[root.axis] = root.stepLength
        root.document.previewArray({
            mode: root.mode === 1 ? "polar" : "linear", count: root.count,
            dx: step[0], dy: step[1], dz: step[2],
            axis: root.polarAxis, angle: root.fullCircle ? 360 / (root.count + 1) : root.angle,
            rotate: root.rotateCopies, radius: root.radius, aroundOrigin: false })
    }
    onModeChanged: update()
    onCountChanged: update()
    onAxisChanged: {
        const box = root.document.selectionBox()
        const sizes = [box.dx, box.dy, box.dz]
        if (sizes[root.axis] !== undefined)
            root.stepLength = Math.round(Math.max(sizes[root.axis], 1) + 100)
        update()
    }
    onStepLengthChanged: update()
    onPolarAxisChanged: update()
    onAngleChanged: update()
    onFullCircleChanged: update()
    onRadiusChanged: update()
    onRotateCopiesChanged: update()

    modal: false
    closePolicy: T.Popup.NoAutoClose
    width: 300
    padding: Theme.space4

    contentItem: Column {
        spacing: Theme.space3
        Text {
            text: qsTrId("ArrayPanel.array")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Tabs {
            width: parent.width
            model: [qsTrId("ArrayPanel.in_linie"), qsTrId("ArrayPanel.in_cerc")]
            currentIndex: root.mode
            onCurrentIndexChanged: root.mode = currentIndex
        }
        NumberField {
            width: parent.width
            label: qsTrId("ArrayPanel.copii")
            from: 1
            to: 500
            value: root.count
            onValueModified: (v) => root.count = Math.round(v)
        }

        // În linie.
        Column {
            visible: root.mode === 0
            width: parent.width
            spacing: Theme.space2
            Tabs {
                width: parent.width
                model: [qsTrId("ArrayPanel.x_rosu"), qsTrId("ArrayPanel.y_verde"), qsTrId("ArrayPanel.z_albastru")]
                currentIndex: root.axis
                onCurrentIndexChanged: root.axis = currentIndex
            }
            NumberField {
                width: parent.width
                label: qsTrId("ArrayPanel.pas")
                unit: "mm"
                step: 50
                value: root.stepLength
                onValueModified: (v) => root.stepLength = v
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTrId("ArrayPanel.lungimea_totala") + Math.round(root.stepLength * root.count) + qsTrId("ArrayPanel.mm_pas_negativ_in_sens_invers")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }

        // În cerc.
        Column {
            visible: root.mode === 1
            width: parent.width
            spacing: Theme.space2
            Tabs {
                width: parent.width
                model: [qsTrId("ArrayPanel.axa_x"), qsTrId("ArrayPanel.axa_y"), qsTrId("ArrayPanel.axa_z")]
                currentIndex: root.polarAxis
                onCurrentIndexChanged: root.polarAxis = currentIndex
            }
            NumberField {
                width: parent.width
                label: qsTrId("ArrayPanel.raza")
                unit: "mm"
                step: 50
                from: 0
                value: root.radius
                onValueModified: (v) => root.radius = v
            }
            Toggle {
                width: parent.width
                text: qsTrId("ArrayPanel.distribuie_pe_tot_cercul")
                checked: root.fullCircle
                onToggled: root.fullCircle = checked
            }
            NumberField {
                visible: !root.fullCircle
                width: parent.width
                label: qsTrId("ArrayPanel.unghi_intre_copii")
                unit: "°"
                decimals: 1
                from: -360
                to: 360
                value: root.angle
                onValueModified: (v) => root.angle = v
            }
            Toggle {
                width: parent.width
                text: qsTrId("ArrayPanel.roteste_copiile")
                checked: root.rotateCopies
                onToggled: root.rotateCopies = checked
            }
        }

        Row {
            anchors.right: parent.right
            spacing: Theme.space2
            Button {
                text: qsTrId("ArrayPanel.anuleaza")
                onClicked: { root.document.cancelOperation(); root.close() }
            }
            Button {
                variant: "primary"
                text: qsTrId("ArrayPanel.gata")
                onClicked: { root.document.commitOperation(); root.close() }
            }
        }
    }
}
