import QtQuick
import QtQuick.Templates as T
import Forma

// Tab-uri compacte în stil „segmented control”. model: listă de titluri.
T.TabBar {
    id: root

    property var model: []

    implicitWidth: 80 * Math.max(1, model.length)
    implicitHeight: Theme.controlHeightSm
    padding: 2
    spacing: 2

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.bgHover
    }

    contentItem: Row {
        spacing: root.spacing
        Repeater { model: root.contentModel }
    }

    Repeater {
        model: root.model

        T.TabButton {
            id: tab
            required property string modelData
            required property int index

            // TabBar își așază copiii după dimensiunile implicite (cu 0, i-ar centra greșit).
            implicitWidth: (root.availableWidth - root.spacing * (root.count - 1)) / Math.max(1, root.count)
            implicitHeight: root.availableHeight
            text: modelData
            font.pixelSize: Theme.fontSizeSm
            font.weight: Font.Medium
            hoverEnabled: true

            contentItem: Text {
                text: tab.text
                font: tab.font
                color: tab.checked ? Theme.textPrimary : tab.hovered ? Theme.textPrimary : Theme.textSecondary
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }

            background: Rectangle {
                radius: Theme.radiusSm
                color: tab.checked ? Theme.bgElevated : "transparent"
                border.color: tab.checked ? Theme.borderSubtle : "transparent"

                Behavior on color { ColorAnimation { duration: Theme.motionFast } }
            }
        }
    }
}
