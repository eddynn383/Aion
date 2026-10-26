import QtQuick
import QtQuick.Effects
import Forma

// Toolbar plutitor, jos în viewport (ca în Figma).
// Uneltele de desen devin funcționale în Faza 3; Orbit și Pan funcționează de acum.
Item {
    id: root

    property string activeTool: "select"
    readonly property var tools: [
        { id: "select",    icon: "mouse-pointer-2",    name: "Select",    key: "Space" },
        { id: "line",      icon: "pen-line",           name: "Line",      key: "L" },
        { id: "rectangle", icon: "square",             name: "Rectangle", key: "R" },
        { id: "circle",    icon: "circle",             name: "Circle",    key: "C" },
        { id: "pushpull",  icon: "arrow-up-from-line", name: "Push/Pull", key: "P" },
        { id: "move",      icon: "move",               name: "Move",      key: "M" },
        { id: "rotate",    icon: "rotate-cw",          name: "Rotate",    key: "Q" },
        { id: "eraser",    icon: "eraser",             name: "Eraser",    key: "E" },
        { id: "-" },
        { id: "orbit",     icon: "orbit",              name: "Orbit",     key: "O" },
        { id: "pan",       icon: "hand",               name: "Pan",       key: "H" }
    ]

    signal toolSelected(string id)

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
            spacing: 2

            Repeater {
                model: root.tools

                delegate: Loader {
                    required property var modelData
                    anchors.verticalCenter: parent.verticalCenter
                    sourceComponent: modelData.id === "-" ? divider : button

                    Component {
                        id: divider
                        Item {
                            width: Theme.space2 + 1
                            height: 20
                            Separator { anchors.centerIn: parent; height: parent.height }
                        }
                    }
                    Component {
                        id: button
                        IconButton {
                            iconName: modelData.icon
                            tooltip: modelData.name
                            shortcut: modelData.key
                            checked: root.activeTool === modelData.id
                            onClicked: root.toolSelected(modelData.id)
                        }
                    }
                }
            }
        }
    }
}
