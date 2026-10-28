import QtQuick
import QtQuick.Effects
import Aion

// Toolbar plutitor, jos în viewport (ca în Figma). Uneltele sunt grupate; fiecare grup își amintește ultima unealtă
// aleasă. Primul grup: Select (și Interact, în modul Interacțiune) plus navigarea (Orbit, Pan, Zoom). În dreapta,
// comutatorul modului de lucru. Grupurile și uneltele au `modes` (0 Modelare, 1 Animare, 2 Interacțiune; lipsă =
// toate) și apar doar în modurile lor (docs/plans/moduri-de-lucru.md).
Item {
    id: root

    property string activeTool: "select"
    property int workMode: 0
    signal modeSelected(int mode)
    // Scurtătura unei comenzi după id (pentru comutatorul de mod), din Main.
    property var shortcutFor: (id) => ""

    readonly property var groups: [
        { id: "select", tools: [
            { id: "select", icon: "mouse-pointer-2", name: "Select", key: "Space", phase: 3 },
            { id: "interact", icon: "hand-pointer", name: "Interact", key: "I", phase: 0, modes: [2] },
            { id: "orbit", icon: "orbit", name: "Orbit", key: "O", phase: 0, section: "Navigare" },
            { id: "pan", icon: "hand", name: "Pan", key: "H", phase: 0 },
            { id: "zoom", icon: "zoom-in", name: "Zoom", key: "Z", phase: 0 } ] },
        // Primitive, după dimensiune: 1D (curbe deschise: linie, arc), 2D (contururi închise care fac o față) și 3D
        // (corpuri create direct ca obiecte). Corpurile au Shift + forma din care „cresc” (dreptunghi → cub, cerc →
        // cilindru), sfera ⇧O, conul ⇧V.
        { id: "primitives", modes: [0], tools: [
            { id: "line", icon: "line-segment", name: "Line", key: "L", phase: 3, section: "1D" },
            { id: "arc",  icon: "spline",   name: "Arc",  key: "A", phase: 5 },
            { id: "rectangle", icon: "square",   name: "Rectangle", key: "R", phase: 3, section: "2D" },
            { id: "circle",    icon: "circle",   name: "Circle",    key: "C", phase: 5 },
            { id: "polygon",   icon: "pentagon", name: "Polygon",   key: "N", phase: 5 },
            { id: "cube",     icon: "prim-cube",     name: "Cub",      key: "Shift+R", phase: 13, section: "3D" },
            { id: "sphere",   icon: "prim-sphere",   name: "Sferă",    key: "Shift+O", phase: 13 },
            { id: "cone",     icon: "prim-cone",     name: "Con",      key: "Shift+V", phase: 13 },
            { id: "cylinder", icon: "prim-cylinder", name: "Cilindru", key: "Shift+C", phase: 13 } ] },
        { id: "modify", modes: [0], tools: [
            { id: "pushpull", icon: "arrow-up-from-line", name: "Push/Pull", key: "P", phase: 5 },
            { id: "offset",   icon: "square-dashed",      name: "Offset",    key: "F", phase: 5 },
            { id: "followme", icon: "route",              name: "Follow Me", key: "",  phase: 5 } ] },
        { id: "transform", modes: [0, 1], tools: [
            { id: "move",   icon: "move",      name: "Move",   key: "M", phase: 5 },
            { id: "rotate", icon: "rotate-cw", name: "Rotate", key: "Q", phase: 5 },
            { id: "scale",  icon: "scale-3d",  name: "Scale",  key: "S", phase: 5 } ] },
        { id: "measure", modes: [0], tools: [
            { id: "tape", icon: "ruler", name: "Tape Measure", key: "T", phase: 9 },
            { id: "protractor", icon: "protractor", name: "Protractor", key: "", phase: 9 },
            { id: "dimension", icon: "dimension", name: "Dimension", key: "", phase: 9 },
            { id: "text", icon: "type", name: "Text", key: "", phase: 9 },
            { id: "section", icon: "section", name: "Section Plane", key: "", phase: 9 } ] },
        { id: "erase", modes: [0], tools: [
            { id: "eraser", icon: "eraser", name: "Eraser", key: "E", phase: 3 } ] }
    ]
    // Navigarea e acum în primul grup (păstrat pentru cine o caută după id).
    readonly property var navigation: groups[0].tools.filter(t => ["orbit", "pan", "zoom"].includes(t.id))
    // Unelte fără buton în toolbar (pornesc din alt loc): Paint Bucket — din panoul Materiale sau cu tasta B.
    readonly property var hiddenTools: [
        { id: "paint", icon: "paint-bucket", name: "Paint Bucket", key: "B", phase: 8 },
        { id: "texture", icon: "image", name: "Poziționează textura", key: "", phase: 24 }
    ]
    readonly property var allTools: groups.reduce((all, g) => all.concat(g.tools), []).concat(hiddenTools)
    // Grupurile modului curent, cu uneltele lui.
    function inMode(item) {
        return !item.modes || item.modes.includes(workMode)
    }
    readonly property var visibleGroups: groups.filter(g => inMode(g))
        .map(g => Object.assign({}, g, { tools: g.tools.filter(t => inMode(t)) }))
        .filter(g => g.tools.length > 0)

    // Scurtăturile personalizate (id comandă „tool.<id>” → secvență), din Preferințe.
    property var keyOverrides: ({})
    function keyFor(tool) {
        const custom = keyOverrides["tool." + tool.id]
        return custom !== undefined ? custom : (tool.key || "")
    }

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
                model: root.visibleGroups
                delegate: ToolGroupButton {
                    required property var modelData
                    anchors.verticalCenter: parent.verticalCenter
                    group: modelData
                    keyFor: (t) => root.keyFor(t)
                    // Ultima unealtă aleasă din grup, dacă e în modul curent.
                    currentToolId: modelData.tools.some(t => t.id === root.groupSelection[modelData.id])
                        ? root.groupSelection[modelData.id] : modelData.tools[0].id
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

            ModeSwitch {
                anchors.verticalCenter: parent.verticalCenter
                mode: root.workMode
                shortcutFor: root.shortcutFor
                onModeSelected: (m) => root.modeSelected(m)
            }
        }
    }
}
