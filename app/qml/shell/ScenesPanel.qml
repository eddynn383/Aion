import QtQuick
import Aion

// Scenele: poziții de cameră salvate (cu stilul de afișare). Click = tranziție animată spre scenă;
// dublu-click redenumește; butoanele actualizează scena cu vederea curentă sau o șterg.
Item {
    id: root

    property var document
    property var viewport
    // Setările animației (păstrate în Main.qml).
    property real transition: 2
    property real delay: 1
    property bool loop: true
    property int fps: 30

    signal exportVideoRequested()

    function sceneStates() {
        return root.document.scenes.map(s => root.document.sceneCamera(s.id))
    }
    function play() {
        if (root.document.scenes.length === 0)
            return
        root.viewport.playScenes(root.sceneStates(), root.transition, root.delay, root.loop)
    }

    Button {
        id: addButton
        width: parent.width
        text: "Adaugă scena curentă"
        iconName: "plus"
        onClicked: root.document.addScene(root.viewport.cameraState(), root.viewport.displayStyle)
    }

    ListView {
        id: list
        anchors.top: addButton.bottom
        anchors.topMargin: Theme.space3
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: animation.top
        anchors.bottomMargin: Theme.space3
        clip: true
        spacing: 1
        model: root.document ? root.document.scenes : []
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            id: row
            required property var modelData
            property bool editing: false

            width: ListView.view.width
            height: Theme.controlHeight
            radius: Theme.radiusSm
            color: rowArea.containsMouse ? Theme.bgHover : "transparent"

            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    const camera = root.document.sceneCamera(row.modelData.id)
                    root.viewport.animateToCamera(camera)
                    if (camera.style && camera.style.length > 0)
                        root.viewport.displayStyle = camera.style
                }
                onDoubleClicked: row.editing = true
            }

            Text {
                visible: !row.editing
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.right: actions.left
                anchors.verticalCenter: parent.verticalCenter
                text: row.modelData.name
                elide: Text.ElideRight
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
            }
            TextInput {
                visible: row.editing
                anchors.left: parent.left
                anchors.leftMargin: Theme.space2
                anchors.right: actions.left
                anchors.verticalCenter: parent.verticalCenter
                text: row.modelData.name
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
                onVisibleChanged: if (visible) { forceActiveFocus(); selectAll() }
                onEditingFinished: {
                    if (row.editing)
                        root.document.renameScene(row.modelData.id, text)
                    row.editing = false
                }
            }

            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: Theme.space1
                anchors.verticalCenter: parent.verticalCenter
                visible: rowArea.containsMouse || updateButton.hovered || deleteButton.hovered

                IconButton {
                    id: updateButton
                    width: 24
                    height: 24
                    iconSize: Theme.iconSizeSm
                    iconName: "rotate-ccw"
                    tooltip: "Actualizează cu vederea curentă"
                    onClicked: root.document.updateScene(row.modelData.id, root.viewport.cameraState(), root.viewport.displayStyle)
                }
                IconButton {
                    id: deleteButton
                    width: 24
                    height: 24
                    iconSize: Theme.iconSizeSm
                    iconName: "trash-2"
                    tooltip: "Șterge scena"
                    onClicked: root.document.removeScene(row.modelData.id)
                }
            }
        }
    }

    // Animația: redă scenele pe rând (ca în SketchUp) sau le exportă ca video.
    Column {
        id: animation
        visible: list.count > 0
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: visible ? implicitHeight : 0
        spacing: Theme.space2

        Separator { width: parent.width }
        Text {
            text: "ANIMAȚIE"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
            font.weight: Font.DemiBold
            font.letterSpacing: 0.6
        }
        Row {
            width: parent.width
            spacing: Theme.space2
            Button {
                width: (parent.width - Theme.space2) / 2
                variant: "primary"
                text: root.viewport && root.viewport.scenesPlaying ? "Oprește" : "Redă"
                iconName: root.viewport && root.viewport.scenesPlaying ? "x" : "play"
                enabled: root.viewport && !root.viewport.exporting
                onClicked: root.viewport.scenesPlaying ? root.viewport.stopScenes() : root.play()
            }
            Button {
                width: (parent.width - Theme.space2) / 2
                text: root.viewport && root.viewport.exporting ? "Anulează" : "Exportă video…"
                iconName: root.viewport && root.viewport.exporting ? "x" : "arrow-up"
                onClicked: root.viewport.exporting ? root.viewport.cancelExport() : root.exportVideoRequested()
            }
        }
        // Progresul exportului.
        Rectangle {
            visible: root.viewport && root.viewport.exporting
            width: parent.width
            height: 4
            radius: 2
            color: Theme.bgHover
            Rectangle {
                width: parent.width * (root.viewport ? root.viewport.exportProgress : 0)
                height: parent.height
                radius: 2
                color: Theme.accent
            }
        }
        Slider {
            width: parent.width
            label: "Tranziție"
            from: 0
            to: 10
            stepSize: 0.5
            value: root.transition
            valueText: value.toFixed(1) + " s"
            onMoved: root.transition = value
        }
        Slider {
            width: parent.width
            label: "Pauză pe scenă"
            from: 0
            to: 10
            stepSize: 0.5
            value: root.delay
            valueText: value.toFixed(1) + " s"
            onMoved: root.delay = value
        }
        Toggle {
            width: parent.width
            text: "În buclă"
            checked: root.loop
            onToggled: root.loop = checked
        }
        Tabs {
            width: parent.width
            model: ["24 fps", "30 fps", "60 fps"]
            currentIndex: root.fps === 24 ? 0 : root.fps === 60 ? 2 : 1
            onCurrentIndexChanged: root.fps = currentIndex === 0 ? 24 : currentIndex === 2 ? 60 : 30
        }
    }

    Text {
        visible: list.count === 0
        anchors.top: addButton.bottom
        anchors.topMargin: Theme.space3
        width: parent.width
        wrapMode: Text.Wrap
        text: "Nicio scenă. Așază camera cum vrei și adaugă o scenă, ca să revii oricând la acea vedere."
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm
    }
}
