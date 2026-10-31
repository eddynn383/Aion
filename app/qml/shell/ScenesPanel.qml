import QtQuick
import Aion

// Scenele: stări salvate ale modelului — camera și stilul, pozițiile obiectelor, ce e ascuns, tag-urile ascunse.
// Click = tranziție lină spre scenă (camera și obiectele); dublu-click redenumește; butoanele recapturează starea
// curentă sau șterg scena. „Scenă nouă” vine după listă; durata tranziției e în setările secțiunii (⚙ din antet).
// Animațiile sunt separate (timeline-ul).
Item {
    id: root

    property var document
    property var viewport
    property real transition: 1

    function apply(sceneId) {
        const camera = root.document.sceneCamera(sceneId)
        root.viewport.animateToCamera(camera, root.transition)
        if (camera.style && camera.style.length > 0)
            root.viewport.displayStyle = camera.style
        root.document.applyScene(sceneId, root.transition)
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        spacing: 1
        model: root.document ? root.document.scenes : []
        boundsBehavior: Flickable.StopAtBounds

        // După scene: rândul care adaugă una nouă din starea curentă.
        footer: Column {
            width: list.width
            topPadding: list.count > 0 ? Theme.space1 : 0
            spacing: Theme.space2
            Rectangle {
                width: parent.width
                height: Theme.controlHeight
                radius: Theme.radiusSm
                color: addArea.containsMouse ? Theme.bgHover : "transparent"
                border.color: Theme.borderSubtle
                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space2
                    Icon { anchors.verticalCenter: parent.verticalCenter; name: "plus"; size: 14; color: Theme.textSecondary }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTrId("ScenesPanel.scena_noua_din_vederea_curenta")
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSm
                    }
                }
                MouseArea {
                    id: addArea
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: root.document.addScene(root.viewport.cameraState(), root.viewport.displayStyle)
                }
            }
            Text {
                visible: list.count === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTrId("ScenesPanel.o_scena_retine_camera_si_starea_modelului_poziti")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }

        delegate: Rectangle {
            id: row
            required property var modelData
            property bool editing: false

            width: ListView.view.width
            height: Theme.controlHeightSm
            radius: Theme.radiusSm
            color: rowHover.hovered ? Theme.bgHover : "transparent"

            MouseArea {
                id: rowArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.apply(row.modelData.id)
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

            // Acțiunile (RowAction: locuri fixe, ca în celelalte secțiuni): șterge, apoi actualizează.
            HoverHandler { id: rowHover }
            Row {
                id: actions
                anchors.right: parent.right
                anchors.rightMargin: 2
                anchors.verticalCenter: parent.verticalCenter
                RowAction {
                    shown: rowHover.hovered
                    iconName: "trash-2"
                    tooltip: qsTrId("ScenesPanel.sterge_scena")
                    onClicked: root.document.removeScene(row.modelData.id)
                }
                RowAction {
                    shown: rowHover.hovered
                    iconName: "rotate-ccw"
                    tooltip: qsTrId("ScenesPanel.actualizeaza_cu_starea_curenta_camera_si_obiecte")
                    onClicked: root.document.updateScene(row.modelData.id, root.viewport.cameraState(), root.viewport.displayStyle)
                }
            }
        }
    }

    // Setările secțiunii (deschise din ⚙ din antet).
    function openSettings(anchor) {
        settings.parent = anchor
        settings.open()
    }
    Popover {
        id: settings
        x: (parent ? parent.width : 0) - width
        y: parent ? parent.height + 4 : 0
        width: 240
        padding: Theme.space3
        contentItem: Column {
            spacing: Theme.space2
            Text {
                text: qsTrId("ScenesPanel.setarile_scenelor")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.DemiBold
            }
            Slider {
                width: 214
                label: qsTrId("ScenesPanel.durata_tranzitiei")
                from: 0
                to: 3
                stepSize: 0.1
                value: root.transition
                valueText: value.toFixed(1) + qsTrId("ScenesPanel.s")
                onMoved: root.transition = value
            }
            Text {
                width: 214
                wrapMode: Text.Wrap
                text: qsTrId("ScenesPanel.0_s_salt_direct_la_scena")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }
    }
}
