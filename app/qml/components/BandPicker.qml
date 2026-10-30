import QtQuick
import Aion

// Cantul unei plăci (pasul 7): dreptunghiul plăcii cu cele 4 laturi — L1 jos (latura lungă din față), L2 sus, W1 în
// stânga, W2 în dreapta. Clic pe o latură → tipul și decorul ei dedesubt; cursorul peste o latură o evidențiază pe
// placa din viewport. Presetările pun toate laturile deodată.
Column {
    id: root

    property var document
    property var board: null // entityInfo.board
    property int side: 0
    readonly property var bands: board ? board.bands : []
    readonly property var current: bands.length === 4 ? bands[side] : null
    readonly property var userPresets: {
        try { return JSON.parse(UserPrefs.bandPresets) } catch (e) { return [] }
    }

    spacing: Theme.space2

    component Chip: Rectangle {
        id: chip
        property string label: ""
        property bool current: false
        property color swatch: "transparent"
        signal clicked()
        width: row.implicitWidth + Theme.space3 * 2
        height: 24
        radius: 12
        color: current ? Theme.accentSubtle : chipArea.containsMouse ? Theme.bgHover : "transparent"
        border.color: current ? Theme.accent : Theme.borderStrong
        Row {
            id: row
            anchors.centerIn: parent
            spacing: 6
            Rectangle {
                visible: chip.swatch.a > 0
                anchors.verticalCenter: parent.verticalCenter
                width: 10
                height: 10
                radius: 5
                color: chip.swatch
                border.color: Theme.borderStrong
            }
            Text {
                text: chip.label
                color: chip.current ? Theme.textPrimary : Theme.textSecondary
                font.pixelSize: Theme.fontSizeXs
            }
        }
        MouseArea {
            id: chipArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: chip.clicked()
        }
    }

    // Placa văzută de sus, cu laturile ei.
    Item {
        width: root.width
        height: 92
        Rectangle {
            id: plate
            anchors.centerIn: parent
            width: Math.min(parent.width - 56, 200)
            height: 48
            color: Theme.bgHover
            border.color: Theme.borderStrong
        }
        Repeater {
            model: 4
            delegate: Item {
                id: edge
                required property int index
                readonly property var band: root.bands.length === 4 ? root.bands[index] : null
                readonly property bool horizontal: index < 2
                readonly property bool chosen: root.side === index
                x: index === 2 ? plate.x - 14 : index === 3 ? plate.x + plate.width + 2 : plate.x
                y: index === 0 ? plate.y + plate.height + 2 : index === 1 ? plate.y - 14 : plate.y
                width: horizontal ? plate.width : 12
                height: horizontal ? 12 : plate.height
                Rectangle {
                    anchors.centerIn: parent
                    width: edge.horizontal ? parent.width : (edge.band && edge.band.bandKey ? 6 : 2)
                    height: edge.horizontal ? (edge.band && edge.band.bandKey ? 6 : 2) : parent.height
                    radius: 2
                    color: edge.chosen || edgeArea.containsMouse ? Theme.accent
                         : edge.band && edge.band.bandKey ? Theme.textSecondary : Theme.borderStrong
                }
                Text {
                    text: edge.band ? edge.band.name : ""
                    color: edge.chosen ? Theme.accent : Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                    x: edge.horizontal ? (edge.width - width) / 2 : (edge.index === 2 ? -width - 4 : edge.width + 4)
                    y: edge.horizontal ? (edge.index === 0 ? edge.height + 2 : -height - 2) : (edge.height - height) / 2
                }
                MouseArea {
                    id: edgeArea
                    anchors.fill: parent
                    anchors.margins: -4
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onContainsMouseChanged: if (root.document) root.document.hoveredBandSide = containsMouse ? edge.index : -1
                    onClicked: root.side = edge.index
                }
            }
        }
    }

    Text {
        width: root.width
        wrapMode: Text.Wrap
        text: root.current
              ? root.current.name + ": " + (root.current.bandKey ? root.current.label + " · "
                    + (root.current.decor ? root.current.decorName : "decor ca placa") : "fără cant")
              : ""
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeXs
    }
    Flow {
        width: root.width
        spacing: 4
        Chip {
            label: "Fără"
            current: root.current !== null && !root.current.bandKey
            onClicked: root.document.setBand(root.side, "", "")
        }
        Repeater {
            model: root.board ? root.board.bandTypes : []
            delegate: Chip {
                required property var modelData
                label: modelData.label
                current: root.current !== null && root.current.bandKey === modelData.key
                onClicked: root.document.setBand(root.side, modelData.key, root.current ? root.current.decor : "")
            }
        }
    }
    Flow {
        width: root.width
        spacing: 4
        visible: root.current !== null && root.current.bandKey !== ""
        Chip {
            label: "Ca placa"
            current: root.current !== null && !root.current.decor
            onClicked: root.document.setBand(root.side, root.current.bandKey, "")
        }
        Repeater {
            model: root.board ? root.board.bandDecors : []
            delegate: Chip {
                required property var modelData
                label: modelData.name
                swatch: modelData.color
                current: root.current !== null && root.current.decor === modelData.key
                onClicked: root.document.setBand(root.side, root.current.bandKey, modelData.key)
            }
        }
    }
    Text {
        text: "Presetări"
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
    }
    Flow {
        width: root.width
        spacing: 4
        Repeater {
            model: root.document ? root.document.bandPresets().concat(root.userPresets) : []
            delegate: Chip {
                required property var modelData
                label: modelData.name
                onClicked: root.document.setBands(modelData.sides)
            }
        }
        Chip {
            label: "+ Salvează ca presetare"
            onClicked: {
                const list = root.userPresets.slice()
                list.push({ name: "Presetarea mea " + (list.length + 1),
                            sides: root.bands.map(b => ({ bandKey: b.bandKey, decor: b.decor })) })
                UserPrefs.bandPresets = JSON.stringify(list)
            }
        }
    }
}
