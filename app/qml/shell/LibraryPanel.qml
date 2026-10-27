import QtQuick
import Aion

// Biblioteca de obiecte: obiectele dinamice (uși, ferestre, mobilier, scări) și obiectele salvate de utilizator
// (fișiere .aion din dosarul bibliotecii), pe categorii, cu căutare. O plăcuță trasă în viewport pune obiectul acolo
// (pe suprafața de sub cursor sau pe sol); click = în centrul vederii.
Item {
    id: root

    property var document
    // Tragerea: Main desenează „fantoma” și primește drop-ul în viewport.
    signal dragStarted(string id, string label, string icon)
    signal dragMoved(point windowPosition)
    signal dragEnded()
    signal placeRequested(string id)

    readonly property string query: search.text.trim().toLowerCase()
    property string category: "" // filtrul (gol = toate)
    readonly property var icons: ({ "gen:door": "door-open", "gen:window": "app-window", "gen:table": "furniture-table",
                                    "gen:shelf": "library", "gen:cabinet": "archive", "gen:stairs": "stairs",
                                    "gen:tree": "tree-deciduous", "gen:conifer": "tree-pine", "gen:bush": "shrub",
                                    "gen:person": "person-standing" })
    readonly property var items: (root.document ? root.document.libraryItems : [])
        .filter(i => (root.category.length === 0 || i.category === root.category)
                     && (root.query.length === 0 || i.name.toLowerCase().indexOf(root.query) >= 0
                         || i.category.toLowerCase().indexOf(root.query) >= 0))
    readonly property var categories: {
        const names = []
        for (const i of (root.document ? root.document.libraryItems : []))
            if (!names.includes(i.category))
                names.push(i.category)
        return names
    }
    readonly property real gridSpacing: Theme.space2
    readonly property int columns: Math.max(2, Math.floor((width + gridSpacing) / (60 + gridSpacing)))
    readonly property real cellWidth: Math.floor((width - gridSpacing * (columns - 1)) / columns)

    Flickable {
        anchors.fill: parent
        contentHeight: column.height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: column
            width: parent.width
            spacing: Theme.space3

            Input {
                id: search
                width: parent.width
                iconName: "search"
                placeholderText: "Caută (ușă, masă, raft…)"
            }

            // Filtrul pe categorii.
            Flow {
                width: parent.width
                spacing: 4
                Repeater {
                    model: [""].concat(root.categories)
                    delegate: Rectangle {
                        id: chip
                        required property string modelData
                        readonly property bool current: root.category === modelData
                        width: chipText.implicitWidth + Theme.space3 * 2
                        height: 22
                        radius: 11
                        color: current ? Theme.accentSubtle : chipArea.containsMouse ? Theme.bgHover : "transparent"
                        border.color: current ? Theme.accent : Theme.borderSubtle
                        Text {
                            id: chipText
                            anchors.centerIn: parent
                            text: chip.modelData.length > 0 ? chip.modelData : "Toate"
                            color: chip.current ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeXs
                        }
                        MouseArea {
                            id: chipArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: root.category = chip.modelData
                        }
                    }
                }
            }

            Flow {
                width: parent.width
                spacing: root.gridSpacing
                Repeater {
                    model: root.items
                            delegate: Rectangle {
                                id: tile
                                required property var modelData
                                width: root.cellWidth
                                height: root.cellWidth * 0.62 + 20
                                radius: Theme.radiusMd
                                color: tileArea.containsMouse ? Theme.bgHover : "transparent"
                                border.color: tileArea.containsMouse ? Theme.borderStrong : Theme.borderSubtle

                                Item {
                                    id: preview
                                    x: Theme.space1
                                    y: Theme.space1
                                    width: parent.width - Theme.space2
                                    height: root.cellWidth * 0.62 - Theme.space1
                                    Image {
                                        visible: (tile.modelData.thumbnail ?? "").length > 0
                                        anchors.fill: parent
                                        source: tile.modelData.thumbnail ?? ""
                                        fillMode: Image.PreserveAspectCrop
                                    }
                                    Icon {
                                        visible: (tile.modelData.thumbnail ?? "").length === 0
                                        anchors.centerIn: parent
                                        name: root.icons[tile.modelData.id] ?? "box"
                                        size: Math.min(26, parent.height * 0.6)
                                        color: Theme.textSecondary
                                    }
                                    // Obiectele dinamice au parametri (lățime, înălțime…) în Inspector.
                                    Icon {
                                        visible: tile.modelData.dynamic
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        name: "sliders-horizontal"
                                        size: 11
                                        color: Theme.accent
                                    }
                                }
                                Text {
                                    anchors.bottom: parent.bottom
                                    anchors.bottomMargin: 4
                                    width: parent.width
                                    horizontalAlignment: Text.AlignHCenter
                                    elide: Text.ElideRight
                                    leftPadding: 4
                                    rightPadding: 4
                                    text: tile.modelData.name
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontSizeXs
                                }
                                MouseArea {
                                    id: tileArea
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                    onClicked: (mouse) => {
                                        if (mouse.button === Qt.RightButton && tile.modelData.user)
                                            root.document.removeLibraryItem(tile.modelData.id)
                                        else if (mouse.button === Qt.LeftButton)
                                            root.placeRequested(tile.modelData.id)
                                    }
                                }
                                DragHandler {
                                    target: null
                                    onActiveChanged: active
                                        ? root.dragStarted(tile.modelData.id, tile.modelData.name, root.icons[tile.modelData.id] ?? "box")
                                        : root.dragEnded()
                                    onCentroidChanged: if (active) root.dragMoved(tile.mapToItem(null, centroid.position.x, centroid.position.y))
                                }
                            }
                }
            }

            Text {
                visible: root.items.length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: "Nimic găsit."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeSm
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "Obiectele tale: selectează un obiect și apasă + din antet („Salvează în bibliotecă”)."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }
    }
}
