import QtQuick
import Aion
import "../shell/ToolGuides.js" as Guides

// Galerie de depanare: toate animațiile ghidurilor, oprite în câteva momente (AION_STARTUP="debug.demos").
Rectangle {
    id: root
    color: Theme.bgApp
    signal closeRequested()

    property var moments: [0.5, 0.92]

    Flickable {
        anchors.fill: parent
        anchors.margins: Theme.space4
        contentHeight: grid.height
        Grid {
            id: grid
            columns: 8
            spacing: Theme.space2
            Repeater {
                model: Guides.tourOrder.length * root.moments.length
                delegate: Column {
                    required property int index
                    readonly property string tool: Guides.tourOrder[Math.floor(index / root.moments.length)]
                    readonly property real moment: root.moments[index % root.moments.length]
                    Text { text: parent.tool + " @ " + parent.moment; color: Theme.textSecondary; font.pixelSize: 10 }
                    ToolDemo {
                        width: 170
                        height: 96
                        tool: parent.tool
                        playing: false
                        t: parent.moment
                    }
                }
            }
        }
    }
    MouseArea { anchors.fill: parent; onClicked: root.closeRequested(); z: -1 }
}
