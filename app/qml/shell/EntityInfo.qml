import QtQuick
import Aion

// Entity Info: proprietățile selecției. Pentru un grup sau o componentă: nume, tag, dimensiuni, blocare și
// comenzile de organizare; pentru fețe aria, pentru muchii lungimea.
Column {
    id: root

    property var document
    readonly property var info: document ? document.entityInfo : ({ kind: "none" })
    readonly property bool isInstance: info.kind === "instance"

    spacing: Theme.space3

    Text {
        width: parent.width
        wrapMode: Text.Wrap
        text: root.info.kind === "none" ? "Nimic selectat" : root.info.title
        color: root.info.kind === "none" ? Theme.textTertiary : Theme.textPrimary
        font.pixelSize: Theme.fontSizeMd
        font.weight: root.info.kind === "none" ? Font.Normal : Font.DemiBold
    }

    Input {
        visible: root.isInstance
        width: parent.width
        placeholderText: root.info.title === "Grup" ? "Numele grupului" : "Numele componentei"
        text: root.isInstance ? root.info.name : ""
        onEditingFinished: if (root.isInstance && text !== root.info.name) root.document.renameInstance(root.info.instanceId, text)
    }

    Repeater {
        model: root.info.details || []
        delegate: Item {
            required property var modelData
            width: root.width
            height: Theme.controlHeightSm

            Text {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: parent.modelData.label
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
            }
            Text {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: parent.modelData.value
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
                font.family: Theme.fontMono
            }
        }
    }

    // Tag-ul: „pastile” cu tag-urile modelului.
    Column {
        visible: root.isInstance
        width: parent.width
        spacing: Theme.space2

        Text {
            text: "Tag"
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeSm
        }
        Flow {
            width: parent.width
            spacing: Theme.space1 + 2

            Repeater {
                model: [{ id: 0, name: "Fără tag", color: "transparent" }].concat(root.document ? root.document.tags : [])
                delegate: Rectangle {
                    id: chip
                    required property var modelData
                    readonly property bool current: root.isInstance && root.info.tagId === modelData.id
                    width: chipRow.implicitWidth + Theme.space3 * 2
                    height: 24
                    radius: 12
                    color: current ? Theme.accentSubtle : chipArea.containsMouse ? Theme.bgHover : "transparent"
                    border.color: current ? Theme.accent : Theme.borderStrong

                    Row {
                        id: chipRow
                        anchors.centerIn: parent
                        spacing: Theme.space1 + 2
                        Rectangle {
                            visible: chip.modelData.id !== 0
                            anchors.verticalCenter: parent.verticalCenter
                            width: 8
                            height: 8
                            radius: 4
                            color: chip.modelData.color
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: chip.modelData.name
                            color: chip.current ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeXs
                        }
                    }
                    MouseArea {
                        id: chipArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: root.document.assignTag(chip.modelData.id)
                    }
                }
            }
        }
    }

    Toggle {
        visible: root.isInstance
        width: parent.width
        text: "Blocat"
        checked: root.isInstance && root.info.locked === true
        onToggled: root.document.setSelectionLocked(checked)
    }

    Flow {
        visible: root.document && root.document.hasInstanceSelected
        width: parent.width
        spacing: Theme.space2

        Button {
            small: true
            text: "Editează"
            visible: root.isInstance && root.info.locked !== true
            onClicked: root.document.enterInstance(0)
        }
        Button {
            small: true
            text: "Explode"
            onClicked: root.document.explodeSelection()
        }
        Button {
            small: true
            text: "Make Unique"
            visible: root.isInstance && root.info.title === "Componentă"
            onClicked: root.document.makeUniqueSelection()
        }
    }
}
