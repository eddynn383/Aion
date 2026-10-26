import QtQuick
import QtQuick.Templates as T
import Aion

// Buton pătrat cu iconiță, pentru toolbar și bare. Tooltip-ul arată numele și scurtătura.
T.AbstractButton {
    id: root

    property string iconName: ""
    property string tooltip: ""
    property string shortcut: ""
    property int iconSize: Theme.iconSizeLg
    // Starea „activ” folosește accentul plin (unealta curentă) sau doar o tentă discretă.
    property bool solidChecked: true
    // Ghidul animat afișat la hover lung (id-ul uneltei din ToolGuides.js); gol = doar tooltip simplu.
    property string guideId: ""

    implicitWidth: Theme.controlHeight + 4
    implicitHeight: Theme.controlHeight + 4
    focusPolicy: Qt.NoFocus
    hoverEnabled: true

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: root.iconName
            size: root.iconSize
            color: root.checked && root.solidChecked ? Theme.accentText
                : root.checked ? Theme.accent
                : root.hovered ? Theme.textPrimary : Theme.textSecondary
        }
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: root.checked ? (root.solidChecked ? Theme.accent : Theme.accentSubtle)
             : root.down ? Theme.bgActive
             : root.hovered ? Theme.bgHover
             : "transparent"

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }

    Tooltip {
        visible: root.hovered && root.tooltip.length > 0 && !guideLoader.active
        text: root.tooltip
        shortcut: root.shortcut
    }

    // Hover lung: tooltip-ul simplu se transformă în ghidul animat.
    Timer {
        interval: 1300
        running: root.hovered && root.guideId.length > 0 && UserPrefs.hoverGuides && !root.down
        onTriggered: guideLoader.active = true
    }
    onHoveredChanged: if (!hovered) guideLoader.active = false
    onPressed: guideLoader.active = false
    Loader {
        id: guideLoader
        active: false
        sourceComponent: RichTooltip {
            parent: root
            toolId: root.guideId
            shortcut: root.shortcut
            Component.onCompleted: open()
            onClosed: guideLoader.active = false
        }
    }
}
