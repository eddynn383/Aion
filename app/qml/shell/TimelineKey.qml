import QtQuick
import Aion

// O cheie pe timeline: un romb (accent pe obiecte și canale, verde pe cameră, roșu pe culoare) și, pe rândurile de
// canal, valoarea ei alături („2.5 m”, „90°”, un pătrat colorat). Evenimentele merg la timeline (selecție, tragere,
// meniu, editarea valorii).
Item {
    id: root

    required property var timeline
    required property var row
    required property var key
    // Cât loc are eticheta până la cheia următoare (px).
    property real labelRoom: 0

    readonly property var ref: ({ instance: row.instance, property: row.property, time: key.time })
    readonly property bool selected: timeline.isSelected(ref)
    readonly property real dragOffset: selected ? timeline.dragDelta * timeline.pxPerSecond : 0
    readonly property bool isColor: row.property === "color"

    x: timeline.xAt(key.time) - width / 2 + dragOffset
    y: (timeline.rowHeight - height) / 2
    width: 14
    height: 14

    Rectangle {
        anchors.centerIn: parent
        width: root.selected ? 11 : 9
        height: width
        rotation: 45
        radius: 2
        color: root.row.kind === "camera" ? Theme.axisY : root.isColor ? Theme.danger : Theme.accent
        border.color: root.selected ? Theme.textPrimary : (root.row.kind === "channel" ? Theme.bgPanel : "transparent")
        border.width: root.selected ? 2 : 1
    }

    // Valoarea cheii, pe rândurile de canal.
    Row {
        visible: root.row.kind === "channel" && root.labelRoom > 18
        x: root.width + 1
        anchors.verticalCenter: parent.verticalCenter
        spacing: 3
        Rectangle {
            visible: root.isColor
            anchors.verticalCenter: parent.verticalCenter
            width: 10
            height: 10
            radius: 2
            color: root.isColor ? root.key.value : "transparent"
            border.color: Theme.borderStrong
        }
        Text {
            visible: !root.isColor
            width: Math.min(implicitWidth, Math.max(0, root.labelRoom - 6))
            elide: Text.ElideRight
            text: root.key.text ?? ""
            color: root.selected ? Theme.textPrimary : Theme.textSecondary
            font.family: Theme.fontMono
            font.pixelSize: 10
        }
    }

    MouseArea {
        anchors.fill: parent
        anchors.margins: -3
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: Qt.SizeHorCursor
        hoverEnabled: true
        onPressed: (mouse) => root.timeline.keyPressed(root, mouse)
        onPositionChanged: (mouse) => { if (pressed) root.timeline.keyDragged(root, mouse) }
        onReleased: (mouse) => root.timeline.keyReleased(root, mouse)
        onDoubleClicked: (mouse) => root.timeline.keyDoubleClicked(root)
    }
}
