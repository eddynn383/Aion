import QtQuick
import QtQuick.Effects
import Aion

// Toolbar plutitor, jos în viewport (ca în Figma). Uneltele sunt grupate; fiecare grup își
// amintește ultima unealtă aleasă. Navigarea (Orbit, Pan, Zoom) stă separat, în dreapta.
// Uneltele de desen devin funcționale în fazele 3 și 5 (`phase`); navigarea funcționează de acum.
Item {
    id: root

    property string activeTool: "select"

    readonly property var groups: [
        { id: "select", tools: [
            { id: "select", icon: "mouse-pointer-2", name: "Select", key: "Space", phase: 3 } ] },
        { id: "lines", tools: [
            { id: "line", icon: "line-segment", name: "Line", key: "L", phase: 3 },
            { id: "arc",  icon: "spline",   name: "Arc",  key: "A", phase: 5 } ] },
        { id: "shapes", tools: [
            { id: "rectangle", icon: "square",   name: "Rectangle", key: "R", phase: 3 },
            { id: "circle",    icon: "circle",   name: "Circle",    key: "C", phase: 5 },
            { id: "polygon",   icon: "pentagon", name: "Polygon",   key: "",  phase: 5 } ] },
        { id: "modify", tools: [
            { id: "pushpull", icon: "arrow-up-from-line", name: "Push/Pull", key: "P", phase: 5 },
            { id: "offset",   icon: "square-dashed",      name: "Offset",    key: "F", phase: 5 },
            { id: "followme", icon: "route",              name: "Follow Me", key: "",  phase: 5 } ] },
        { id: "transform", tools: [
            { id: "move",   icon: "move",      name: "Move",   key: "M", phase: 5 },
            { id: "rotate", icon: "rotate-cw", name: "Rotate", key: "Q", phase: 5 },
            { id: "scale",  icon: "scale-3d",  name: "Scale",  key: "S", phase: 5 } ] },
        { id: "measure", tools: [
            { id: "tape", icon: "ruler", name: "Tape Measure", key: "T", phase: 9 },
            { id: "protractor", icon: "protractor", name: "Protractor", key: "", phase: 9 },
            { id: "dimension", icon: "dimension", name: "Dimension", key: "", phase: 9 },
            { id: "text", icon: "type", name: "Text", key: "", phase: 9 },
            { id: "section", icon: "section", name: "Section Plane", key: "", phase: 9 } ] },
        { id: "erase", tools: [
            { id: "eraser", icon: "eraser", name: "Eraser", key: "E", phase: 3 },
            { id: "paint", icon: "paint-bucket", name: "Paint Bucket", key: "B", phase: 8 } ] }
    ]
    readonly property var navigation: [
        { id: "orbit", icon: "orbit",   name: "Orbit", key: "O" },
        { id: "pan",   icon: "hand",    name: "Pan",   key: "H" },
        { id: "zoom",  icon: "zoom-in", name: "Zoom",  key: "Z" }
    ]
    readonly property var allTools: groups.reduce((all, g) => all.concat(g.tools), []).concat(navigation)

    // Ultima unealtă aleasă din fiecare grup (id grup → id unealtă).
    property var groupSelection: ({})

    signal toolSelected(string id)

    function toolInfo(id) {
        return allTools.find(t => t.id === id)
    }

    function groupOf(toolId) {
        return groups.find(g => g.tools.some(t => t.id === toolId))
    }

    // Unealta activată (din toolbar, scurtătură sau command palette) devine cea afișată în grupul ei.
    onActiveToolChanged: {
        const g = groupOf(activeTool)
        if (g) {
            const next = Object.assign({}, groupSelection)
            next[g.id] = activeTool
            groupSelection = next
        }
    }

    implicitWidth: bar.implicitWidth
    implicitHeight: bar.implicitHeight

    RectangularShadow {
        anchors.fill: bar
        radius: bar.radius
        offset.y: 6
        blur: 24
        spread: -2
        color: Theme.shadow
    }

    Rectangle {
        id: bar
        implicitWidth: row.implicitWidth + Theme.space1 * 2 + 2
        implicitHeight: row.implicitHeight + Theme.space1 * 2 + 2
        radius: Theme.radiusLg
        color: Theme.bgElevated
        border.color: Theme.borderStrong

        Row {
            id: row
            anchors.centerIn: parent
            spacing: Theme.space1

            Repeater {
                model: root.groups
                delegate: ToolGroupButton {
                    required property var modelData
                    anchors.verticalCenter: parent.verticalCenter
                    group: modelData
                    currentToolId: root.groupSelection[modelData.id] || modelData.tools[0].id
                    active: modelData.tools.some(t => t.id === root.activeTool)
                    onToolSelected: (id) => root.toolSelected(id)
                }
            }

            Item {
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.space2 + 1
                height: 24
                Separator { anchors.centerIn: parent; height: parent.height }
            }

            // Navigarea: un cluster separat, ca modurile din dreapta toolbar-ului Figma.
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                implicitWidth: navRow.implicitWidth + 4
                implicitHeight: navRow.implicitHeight + 4
                radius: Theme.radiusMd + 2
                color: Theme.bgHover

                Row {
                    id: navRow
                    anchors.centerIn: parent
                    spacing: 0
                    Repeater {
                        model: root.navigation
                        delegate: IconButton {
                            required property var modelData
                            implicitWidth: Theme.controlHeight
                            implicitHeight: Theme.controlHeight
                            iconName: modelData.icon
                            iconSize: Theme.iconSizeMd
                            tooltip: modelData.name
                            shortcut: modelData.key
                            checked: root.activeTool === modelData.id
                            solidChecked: false
                            onClicked: root.toolSelected(modelData.id)
                        }
                    }
                }
            }
        }
    }
}
