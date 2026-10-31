import QtQuick
import Aion

// Cadrul vederii prin cameră (plan Animare, secțiunea 9): exteriorul formatului estompat (passepartout), marginea,
// eticheta („Camera 1 · 35 mm · 16:9”), comutatorul „Blochează camera pe vedere” și ieșirea. Marginile din stânga /
// dreapta se trag și schimbă formatul (raportul), cu prindere de presetări; scrierea se face la eliberare.
Item {
    id: root

    property var viewport
    property var document

    readonly property rect frame: viewport ? viewport.lookFrame : Qt.rect(0, 0, 0, 0)
    readonly property bool active: viewport && viewport.lookThrough > 0 && frame.width > 0
    readonly property color shade: Qt.rgba(0, 0, 0, 0.55)

    visible: active
    opacity: viewport ? viewport.lookFade : 0

    // Exteriorul cadrului.
    Rectangle { x: 0; y: 0; width: root.width; height: root.frame.y; color: root.shade }
    Rectangle { x: 0; y: root.frame.y + root.frame.height; width: root.width; height: root.height - y; color: root.shade }
    Rectangle { x: 0; y: root.frame.y; width: root.frame.x; height: root.frame.height; color: root.shade }
    Rectangle {
        x: root.frame.x + root.frame.width
        y: root.frame.y
        width: root.width - x
        height: root.frame.height
        color: root.shade
    }
    Rectangle {
        x: root.frame.x
        y: root.frame.y
        width: root.frame.width
        height: root.frame.height
        color: "transparent"
        border.color: Qt.rgba(1, 1, 1, 0.55)
        border.width: 1
    }

    // Eticheta și comenzile, deasupra cadrului.
    Row {
        x: root.frame.x
        y: Math.max(Theme.space1, root.frame.y - height - Theme.space1)
        spacing: Theme.space2
        Rectangle {
            height: 24
            width: labelRow.implicitWidth + Theme.space2 * 2
            radius: Theme.radiusSm
            color: Qt.rgba(0, 0, 0, 0.55)
            Row {
                id: labelRow
                anchors.centerIn: parent
                spacing: Theme.space1 + 2
                Icon { anchors.verticalCenter: parent.verticalCenter; name: "camera"; size: 13; color: "white" }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.viewport ? root.viewport.lookLabel : ""
                    color: "white"
                    font.pixelSize: Theme.fontSizeXs
                }
            }
        }
        Rectangle {
            id: lockButton
            height: 24
            width: lockRow.implicitWidth + Theme.space2 * 2
            radius: Theme.radiusSm
            readonly property bool on: root.viewport && root.viewport.lockCameraToView
            color: on ? Theme.accent : lockArea.containsMouse ? Qt.rgba(0, 0, 0, 0.7) : Qt.rgba(0, 0, 0, 0.55)
            Row {
                id: lockRow
                anchors.centerIn: parent
                spacing: Theme.space1 + 2
                Icon { anchors.verticalCenter: parent.verticalCenter; name: lockButton.on ? "lock" : "lock-open"; size: 12; color: "white" }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("CameraFrame.blocheaza_camera_pe_vedere")
                    color: "white"
                    font.pixelSize: Theme.fontSizeXs
                }
            }
            MouseArea {
                id: lockArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.viewport.lockCameraToView = !root.viewport.lockCameraToView
            }
            Tooltip {
                visible: lockArea.containsMouse
                text: lockButton.on ? qsTrId("CameraFrame.orbit_pan_zoom_muta_camera_un_pas_de_undo_pe_ges")
                                    : qsTrId("CameraFrame.navigarea_iese_din_camera_zbor_inapoi")
            }
        }
        Rectangle {
            height: 24
            width: 24
            radius: Theme.radiusSm
            color: exitArea.containsMouse ? Qt.rgba(0, 0, 0, 0.7) : Qt.rgba(0, 0, 0, 0.55)
            Icon { anchors.centerIn: parent; name: "x"; size: 12; color: "white" }
            MouseArea {
                id: exitArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: root.viewport.lookThrough = 0
            }
            Tooltip { visible: exitArea.containsMouse; text: qsTrId("CameraFrame.iesi_din_camera_esc") }
        }
    }

    // Mânerele formatului: marginile din stânga și dreapta (lățimea), de sus și de jos (înălțimea).
    Repeater {
        model: ["left", "right", "top", "bottom"]
        delegate: MouseArea {
            id: handle
            required property string modelData
            readonly property bool horizontal: modelData === "left" || modelData === "right"
            x: modelData === "left" ? root.frame.x - 4 : modelData === "right" ? root.frame.x + root.frame.width - 4
                                                                                : root.frame.x + root.frame.width * 0.25
            y: modelData === "top" ? root.frame.y - 4 : modelData === "bottom" ? root.frame.y + root.frame.height - 4
                                                                               : root.frame.y + root.frame.height * 0.25
            width: horizontal ? 8 : root.frame.width * 0.5
            height: horizontal ? root.frame.height * 0.5 : 8
            hoverEnabled: true
            cursorShape: horizontal ? Qt.SizeHorCursor : Qt.SizeVerCursor
            property point start
            property size startFrame
            property real aspect: 0
            Rectangle {
                anchors.centerIn: parent
                width: handle.horizontal ? 4 : 28
                height: handle.horizontal ? 28 : 4
                radius: 2
                color: handle.containsMouse || handle.pressed ? Theme.accent : Qt.rgba(1, 1, 1, 0.75)
            }
            // Raportul cerut, prins de presetări când e aproape (2%).
            function snapped(a) {
                for (const p of root.document.cameraFormatPresets()) {
                    const pa = p.width / p.height
                    if (Math.abs(a - pa) / pa < 0.02)
                        return pa
                }
                return a
            }
            onPressed: (mouse) => {
                start = mapToItem(root, mouse.x, mouse.y)
                startFrame = Qt.size(root.frame.width, root.frame.height)
                aspect = startFrame.width / startFrame.height
            }
            onPositionChanged: (mouse) => {
                if (!pressed)
                    return
                // Marginea trasă mărește / micșorează cadrul simetric față de centru.
                const p = mapToItem(root, mouse.x, mouse.y)
                const d = modelData === "left" ? start.x - p.x : modelData === "right" ? p.x - start.x
                        : modelData === "top" ? start.y - p.y : p.y - start.y
                const w = horizontal ? Math.max(20, startFrame.width + d * 2) : startFrame.width
                const h = horizontal ? startFrame.height : Math.max(20, startFrame.height + d * 2)
                aspect = snapped(Math.max(0.2, Math.min(5, w / h)))
                root.viewport.previewLookAspect(aspect)
            }
            onReleased: {
                const cameraId = root.viewport.lookThrough
                root.viewport.previewLookAspect(0)
                const info = root.document.cameraInfo(cameraId)
                if (!info)
                    return
                // Latura lungă rămâne; cealaltă urmează raportul.
                const longSide = Math.max(info.formatWidth, info.formatHeight)
                const w = aspect >= 1 ? longSide : Math.round(longSide * aspect)
                const h = aspect >= 1 ? Math.round(longSide / aspect) : longSide
                root.document.setCameraProperties(cameraId, { formatWidth: w, formatHeight: h })
            }
        }
    }
}
