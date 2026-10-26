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
        placeholderText: root.info.title === "Grup" ? "Numele grupului" : "Numele obiectului"
        text: root.isInstance ? root.info.name : ""
        onEditingFinished: if (root.isInstance && text !== root.info.name) root.document.renameInstance(root.info.instanceId, text)
    }

    // Detaliile: eticheta în stânga și valoarea în dreapta; dacă nu încap pe un rând, valoarea trece dedesubt.
    // Pozițiile sunt calculate direct (nu prin ancore schimbate din mers, care pot rămâne agățate).
    Repeater {
        model: root.info.details || []
        delegate: Item {
            id: detailRow
            required property var modelData
            readonly property real gap: Theme.space3
            readonly property bool stacked: label.implicitWidth + valueMetrics.advanceWidth + gap > width

            width: root.width
            height: stacked ? label.implicitHeight + Theme.space1 + value.implicitHeight
                            : Math.max(Theme.controlHeightSm, label.implicitHeight, value.implicitHeight)

            TextMetrics {
                id: valueMetrics
                font: value.font
                text: detailRow.modelData.value
            }
            Text {
                id: label
                x: 0
                y: detailRow.stacked ? 0 : (detailRow.height - implicitHeight) / 2
                text: detailRow.modelData.label
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
            }
            Text {
                id: value
                width: detailRow.stacked ? detailRow.width : Math.ceil(valueMetrics.advanceWidth) + 1
                x: detailRow.stacked ? 0 : detailRow.width - width
                y: detailRow.stacked ? label.implicitHeight + Theme.space1 : (detailRow.height - implicitHeight) / 2
                horizontalAlignment: detailRow.stacked ? Text.AlignLeft : Text.AlignRight
                wrapMode: Text.Wrap
                text: detailRow.modelData.value
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
            text: "Desface"
            onClicked: root.document.explodeSelection()
        }
        Button {
            small: true
            text: "Fă unic"
            visible: root.isInstance && root.info.title === "Obiect" && root.info.copies > 1
            onClicked: root.document.makeUniqueSelection()
        }
    }
}
