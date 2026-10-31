import QtQuick
import Aion

// O listă de blocuri de acțiune (corpul unei reguli sau o ramură a lui „Alternează”), cu locuri de drop
// înainte de primul bloc și după fiecare bloc.
Column {
    id: root

    property var editor
    property var listPath: [] // calea listei în regulile editorului
    property var actions: []

    spacing: 0

    component Slot: DropArea {
        id: slot
        property int index: 0
        width: root.width
        height: containsDrag ? 26 : 8
        keys: ["aion-block"]
        Rectangle {
            anchors.fill: parent
            anchors.topMargin: 2
            anchors.bottomMargin: 2
            radius: Theme.radiusSm
            visible: slot.containsDrag
            color: Theme.accentSubtle
            border.color: Theme.accent
        }
        onDropped: (drop) => { root.editor.dropAt(root.listPath, slot.index); drop.accept() }
        Behavior on height { NumberAnimation { duration: Theme.motionFast } }
    }

    Slot { index: 0 }
    Repeater {
        model: root.actions
        delegate: Column {
            id: entry
            required property var modelData
            required property int index
            width: root.width
            RulesActionBlock {
                id: block
                width: parent.width
                editor: root.editor
                action: entry.modelData
                path: root.listPath.concat([entry.index])

                // Tot blocul primește drop-ul: jumătatea de sus pune blocul înainte, cea de jos după.
                // (Un „Alternează” are propriile liste înăuntru, care au prioritate fiind deasupra.)
                DropArea {
                    id: blockDrop
                    anchors.fill: parent
                    z: -1
                    keys: ["aion-block"]
                    readonly property bool before: drag.y < height / 2
                    onDropped: (drop) => { root.editor.dropAt(root.listPath, entry.index + (before ? 0 : 1)); drop.accept() }
                    Rectangle {
                        visible: blockDrop.containsDrag
                        width: parent.width
                        height: 3
                        radius: 1.5
                        y: blockDrop.before ? -4 : parent.height - 3
                        color: Theme.accent
                    }
                }
            }
            Slot { index: entry.index + 1 }
        }
    }
    Text {
        visible: root.actions.length === 0
        width: root.width
        text: qsTrId("RulesActionList.trage_aici_blocuri_din_paleta")
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
        horizontalAlignment: Text.AlignHCenter
    }
}
