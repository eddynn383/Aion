import QtQuick
import Aion

// Un rând al timeline-ului: în stânga numele (obiectul cu săgeata ▸ / ▾ și ◆, canalul cu valoarea lui curentă,
// editabilă), în dreapta pista: romburile cheilor și, pe rândul obiectului și al camerei, barele intervalelor animate;
// pe rândul unei căi de parcurgere, bara ei (startul și durata), care se trage.
Item {
    id: root

    required property var timeline
    required property var row
    readonly property var doc: timeline.document
    readonly property bool isObject: row.kind === "object"
    readonly property bool isChannel: row.kind === "channel"

    width: timeline.namesWidth + timeline.lanesWidth
    height: timeline.rowHeight

    // --- Numele ---
    Rectangle {
        id: nameCell
        width: root.timeline.namesWidth
        height: parent.height
        color: root.row.selected && !root.isChannel ? Theme.accentSubtle : "transparent"

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            // Click dreapta: curba de easing pentru tot canalul (sau toate cheile obiectului / camerei).
            onClicked: (mouse) => {
                if (root.row.kind === "path")
                    root.doc.selectObject("path", root.row.path.id)
                else if (mouse.button === Qt.RightButton)
                    root.timeline.editRowEasing(root.row, nameCell)
                else if (root.row.instance > 0)
                    root.doc.selectInstance(root.row.instance)
            }
            onDoubleClicked: if (root.isObject) root.timeline.toggleExpanded(root.row.instance)
        }

        Row {
            anchors.left: parent.left
            anchors.leftMargin: root.isChannel ? 30 : 2
            anchors.verticalCenter: parent.verticalCenter
            spacing: Theme.space1
            IconButton {
                visible: root.isObject
                anchors.verticalCenter: parent.verticalCenter
                width: 18; height: 18; iconSize: 12
                iconName: "chevron-right"
                rotation: root.row.open ? 90 : 0
                tooltip: root.row.open ? qsTrId("TimelineRow.inchide_ascunde_canalele") : qsTrId("TimelineRow.deschide_proprietatile_animate")
                onClicked: root.timeline.toggleExpanded(root.row.instance)
            }
            Item { visible: !root.isObject && !root.isChannel; width: 18; height: 18 }
            Icon {
                visible: !root.isChannel
                anchors.verticalCenter: parent.verticalCenter
                name: root.row.icon ?? "box"
                size: 12
                color: Theme.textTertiary
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: root.timeline.namesWidth - (root.isChannel ? 30 + 96 : 2 + 18 + 12 + 30) - Theme.space1 * 3
                elide: Text.ElideRight
                text: root.row.name
                color: root.isChannel ? Theme.textSecondary : root.row.selected ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontSizeXs
                font.weight: root.isChannel ? Font.Normal : Font.Medium
            }
        }

        // Obiectul: keyframe pe toate canalele lui la timpul curent.
        IconButton {
            visible: root.isObject
            anchors.right: parent.right
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            width: 20; height: 20; iconSize: 11
            iconName: "diamond"
            tooltip: qsTrId("TimelineRow.keyframe_pe_toate_proprietatile_la_timpul_curent")
            onClicked: root.doc.keyInstance(root.row.instance)
        }

        // Canalul: valoarea de la timpul curent; o valoare nouă scrie o cheie.
        NumberField {
            visible: root.isChannel && root.row.property !== "color"
            anchors.right: parent.right
            anchors.rightMargin: Theme.space1
            anchors.verticalCenter: parent.verticalCenter
            width: 92
            height: 20
            compact: true
            labelVisible: false
            unit: root.row.unit === "×" ? "" : (root.row.unit ?? "")
            decimals: root.row.unit === "mm" ? 0 : root.row.unit === "°" ? 1 : 3
            step: root.row.unit === "mm" ? 10 : root.row.unit === "°" ? 1 : 0.05
            value: {
                const v = root.timeline.currentValue(root.row)
                return typeof v === "number" ? v : 0
            }
            onValueModified: (v) => root.doc.setChannelValue(root.row.instance, root.row.property, v)
        }
        Rectangle {
            visible: root.isChannel && root.row.property === "color"
            anchors.right: parent.right
            anchors.rightMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            width: 14
            height: 14
            radius: 3
            color: root.row.property === "color" ? (root.timeline.currentValue(root.row) || "transparent") : "transparent"
            border.color: Theme.borderStrong
        }
    }

    // --- Pista ---
    Item {
        id: lane
        x: root.timeline.namesWidth
        width: root.timeline.lanesWidth
        height: parent.height
        clip: true

        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 1
            anchors.bottomMargin: 1
            radius: Theme.radiusSm
            color: root.isChannel ? "transparent" : root.row.selected ? Theme.accentSubtle : Theme.bgHover
        }
        Rectangle { // linia de bază a canalului
            visible: root.isChannel
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            height: 1
            color: Theme.borderSubtle
        }

        // Intervalele animate (între două momente cu chei).
        Repeater {
            model: root.isChannel ? 0 : Math.max(0, root.row.keys.length - 1)
            delegate: Rectangle {
                required property int index
                readonly property real x0: root.timeline.xAt(root.row.keys[index].time)
                readonly property real x1: root.timeline.xAt(root.row.keys[index + 1].time)
                x: x0
                width: Math.max(0, x1 - x0)
                height: 4
                anchors.verticalCenter: parent.verticalCenter
                radius: 2
                color: root.row.kind === "camera" ? Qt.rgba(Theme.axisY.r, Theme.axisY.g, Theme.axisY.b, 0.35)
                                                  : Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.35)
            }
        }

        // Curba de easing a fiecărui segment dintre două chei (dacă încape); click = editorul ei.
        Repeater {
            model: Math.max(0, root.row.keys.length - 1)
            delegate: Item {
                id: segment
                required property int index
                readonly property var key: root.row.keys[index]
                readonly property real x0: root.timeline.xAt(key.time)
                readonly property real x1: root.timeline.xAt(root.row.keys[index + 1].time)
                // Pe canale, eticheta valorii stă lângă cheie: curba e în a doua jumătate a segmentului.
                readonly property real center: root.isChannel ? x0 + (x1 - x0) * 0.62 : (x0 + x1) / 2
                visible: x1 - x0 > (root.isChannel ? 96 : 44) && key.easing !== null && key.easing !== undefined
                x: center - width / 2
                width: 18
                height: root.height
                Rectangle {
                    anchors.centerIn: parent
                    width: 20
                    height: 16
                    radius: 4
                    color: segmentArea.containsMouse ? Theme.bgActive : (root.isChannel ? Theme.bgPanel : "transparent")
                }
                EasingCurve {
                    anchors.centerIn: parent
                    width: 14
                    height: 12
                    lineWidth: 1.2
                    overshoot: 0.15
                    easing: segment.key.easing ?? null
                    color: segmentArea.containsMouse ? Theme.textPrimary : Theme.textTertiary
                }
                MouseArea {
                    id: segmentArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.timeline.segmentClicked(root.row, segment.index, segment)
                }
                Tooltip { visible: segmentArea.containsMouse; text: qsTrId("TimelineRow.curba_tranzitiei_click_pentru_editor") }
            }
        }

        // Calea: bara de la start la final. Tragerea mută startul, capătul drept schimbă durata (cu snap la cadre; ⇧ fără);
        // la eliberare, un singur pas de undo. Bara trasă după capătul animației o lungește.
        Item {
            id: pathBar
            visible: root.row.kind === "path"
            readonly property var p: root.row.path ?? ({ id: 0, start: 0, duration: 0, anchors: [] })
            property real previewStart: -1
            property real previewDuration: -1
            readonly property real start: previewStart >= 0 ? previewStart : p.start
            readonly property real duration: previewDuration >= 0 ? previewDuration : p.duration
            x: root.timeline.xAt(start)
            width: Math.max(8, root.timeline.xAt(start + duration) - x)
            height: root.height - 6
            anchors.verticalCenter: parent.verticalCenter
            function snapped(t, mouse) {
                return (mouse.modifiers & Qt.ShiftModifier) ? t : Math.round(t * root.timeline.fps) / root.timeline.fps
            }
            Rectangle {
                anchors.fill: parent
                radius: 4
                color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, root.row.selected ? 0.5 : 0.3)
                border.color: root.row.selected ? Theme.accent : "transparent"
            }
            Text {
                anchors.left: parent.left
                anchors.leftMargin: 6
                anchors.right: parent.right
                anchors.rightMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: (pathBar.p.anchors ?? []).length > 0 ? pathBar.p.anchors.join(", ") : qsTrId("TimelineRow.fara_obiecte")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeXs
            }
            MouseArea {
                id: barArea
                anchors.fill: parent
                anchors.rightMargin: endArea.width
                hoverEnabled: true
                cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                property real pressX: 0
                property real pressStart: 0
                property bool moved: false
                onPressed: (mouse) => {
                    pressX = mapToItem(lane, mouse.x, 0).x
                    pressStart = pathBar.p.start
                    moved = false
                    root.doc.selectObject("path", pathBar.p.id)
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const dx = mapToItem(lane, mouse.x, 0).x - pressX
                    moved = moved || Math.abs(dx) > 3
                    if (moved)
                        pathBar.previewStart = Math.max(0, pathBar.snapped(pressStart + dx / root.timeline.pxPerSecond, mouse))
                }
                onReleased: {
                    if (moved && pathBar.previewStart >= 0)
                        root.doc.setPathTiming(pathBar.p.id, pathBar.previewStart, pathBar.p.duration)
                    pathBar.previewStart = -1
                }
                onDoubleClicked: root.timeline.editEasing({ path: pathBar.p.id }, pathBar.p.easing, root.row.name, pathBar)
            }
            MouseArea {
                id: endArea
                anchors.right: parent.right
                width: Math.min(8, parent.width * 0.35) // o bară scurtă rămâne de tras de mijloc
                height: parent.height
                hoverEnabled: true
                cursorShape: Qt.SizeHorCursor
                property real pressX: 0
                property real pressEnd: 0
                onPressed: (mouse) => {
                    pressX = mapToItem(lane, mouse.x, 0).x
                    pressEnd = pathBar.p.start + pathBar.p.duration
                }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const end = pathBar.snapped(pressEnd + (mapToItem(lane, mouse.x, 0).x - pressX) / root.timeline.pxPerSecond, mouse)
                    pathBar.previewDuration = Math.max(1 / root.timeline.fps, end - pathBar.p.start)
                }
                onReleased: {
                    if (pathBar.previewDuration >= 0)
                        root.doc.setPathTiming(pathBar.p.id, pathBar.p.start, pathBar.previewDuration)
                    pathBar.previewDuration = -1
                }
            }
            Tooltip {
                visible: (barArea.containsMouse && !barArea.pressed) || endArea.containsMouse
                text: endArea.containsMouse ? qsTrId("TimelineRow.trage_durata_parcurgerii")
                                            : qsTrId("TimelineRow.trage_startul_capatul_drept_durata_dublu_click_c")
            }
        }

        Repeater {
            model: root.row.keys
            delegate: TimelineKey {
                required property var modelData
                required property int index
                timeline: root.timeline
                row: root.row
                key: modelData
                labelRoom: (index + 1 < root.row.keys.length ? root.timeline.xAt(root.row.keys[index + 1].time)
                                                              : lane.width) - root.timeline.xAt(modelData.time) - 10
            }
        }
    }
}
