import QtQuick
import Aion
import "../shell/ToolGuides.js" as Guides

// Turul aplicației: uneltele pe rând, fiecare cu animația și pașii ei. Săgețile ← → navighează.
Popover {
    id: root

    // Scurtăturile uneltelor (id → tastă), din Toolbar.
    property var shortcuts: ({})
    signal tryTool(string id)

    property int index: 0
    readonly property var order: Guides.tourOrder
    readonly property string toolId: order[index]

    modal: true
    anchors.centerIn: parent
    width: 400
    padding: Theme.space6

    onAboutToShow: index = 0

    contentItem: Column {
        spacing: Theme.space4
        focus: true
        Keys.onLeftPressed: root.index = Math.max(0, root.index - 1)
        Keys.onRightPressed: root.index = Math.min(root.order.length - 1, root.index + 1)

        Row {
            width: parent.width
            Text {
                width: parent.width - closeButton.width
                anchors.verticalCenter: parent.verticalCenter
                text: "Turul uneltelor · " + (root.index + 1) + " din " + root.order.length
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.Medium
            }
            IconButton {
                id: closeButton
                iconName: "x"
                tooltip: "Închide"
                onClicked: root.close()
            }
        }

        GuideCard {
            width: parent.width
            toolId: root.toolId
            shortcut: root.shortcuts[root.toolId] || ""
            playing: root.opened
        }

        // Progresul: un punct pentru fiecare unealtă (click = salt).
        Flow {
            width: parent.width
            spacing: 5
            Repeater {
                model: root.order.length
                delegate: Rectangle {
                    required property int index
                    width: index === root.index ? 16 : 6
                    height: 6
                    radius: 3
                    color: index === root.index ? Theme.accent : Theme.borderStrong
                    Behavior on width { NumberAnimation { duration: Theme.motionFast } }
                    MouseArea {
                        anchors.fill: parent
                        anchors.margins: -3
                        onClicked: root.index = parent.index
                    }
                }
            }
        }

        Item {
            width: parent.width
            height: Theme.controlHeight
            Button {
                anchors.left: parent.left
                text: "Încearcă"
                iconName: "mouse-pointer-2"
                onClicked: { root.tryTool(root.toolId); root.close() }
            }
            Row {
                anchors.right: parent.right
                spacing: Theme.space2
                Button {
                    text: "Înapoi"
                    enabled: root.index > 0
                    onClicked: root.index -= 1
                }
                Button {
                    variant: "primary"
                    text: root.index === root.order.length - 1 ? "Gata" : "Următorul"
                    onClicked: root.index === root.order.length - 1 ? root.close() : root.index += 1
                }
            }
        }
    }
}
