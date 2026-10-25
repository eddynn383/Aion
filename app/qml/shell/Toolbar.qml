import QtQuick
import Forma

// Toolbar plutitor, jos în viewport (ca în Figma).
// Uneltele sunt doar vizuale până în Faza 3.
Rectangle {
    id: root

    property string activeTool: "select"

    implicitWidth: tools.implicitWidth + Theme.space2 * 2
    implicitHeight: tools.implicitHeight + Theme.space2 * 2
    radius: Theme.radiusLg
    color: Theme.bgElevated
    border.color: Theme.borderSubtle

    // Umbră simplă (MultiEffect va fi folosit în Faza 1).
    Rectangle {
        z: -1
        anchors.fill: parent
        anchors.topMargin: 4
        anchors.leftMargin: -1
        anchors.rightMargin: -1
        anchors.bottomMargin: -6
        radius: parent.radius + 2
        color: Theme.shadow
        opacity: 0.6
    }

    Row {
        id: tools
        anchors.centerIn: parent
        spacing: Theme.space1

        Repeater {
            model: [
                { id: "select",    glyph: "↖", name: "Select",    key: "Space" },
                { id: "line",      glyph: "╱", name: "Line",      key: "L" },
                { id: "rectangle", glyph: "▭", name: "Rectangle", key: "R" },
                { id: "circle",    glyph: "◯", name: "Circle",    key: "C" },
                { id: "pushpull",  glyph: "⇅", name: "Push/Pull", key: "P" },
                { id: "move",      glyph: "✥", name: "Move",      key: "M" },
                { id: "rotate",    glyph: "⟳", name: "Rotate",    key: "Q" },
                { id: "eraser",    glyph: "⌫", name: "Eraser",    key: "E" }
            ]

            delegate: IconButton {
                required property var modelData
                glyph: modelData.glyph
                tooltip: modelData.name
                shortcut: modelData.key
                checked: root.activeTool === modelData.id
                onClicked: root.activeTool = modelData.id
            }
        }
    }
}
