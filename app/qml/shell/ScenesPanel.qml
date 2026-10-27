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
                        text: "Scenă nouă din vederea curentă"
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
                text: "O scenă reține camera și starea modelului (pozițiile obiectelor, ce e ascuns), ca să revii "
                      + "oricând la ea — de exemplu „Ușile închise” și „Ușile deschise”."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }

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
                    tooltip: "Actualizează cu starea curentă (camera și obiectele)"
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
                text: "Setările scenelor"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.DemiBold
            }
            Slider {
                width: 214
                label: "Durata tranziției"
                from: 0
                to: 3
                stepSize: 0.1
                value: root.transition
                valueText: value.toFixed(1) + " s"
                onMoved: root.transition = value
            }
            Text {
                width: 214
                wrapMode: Text.Wrap
                text: "0 s = salt direct la scenă."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }
    }
}
