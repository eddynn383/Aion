import QtQuick
import Forma

// Bara de sus: numele documentului, câmpul pentru command palette și tema.
Rectangle {
    id: root

    property string documentName: "Fără titlu"

    signal commandPaletteRequested()

    height: Theme.titleBarHeight
    color: Theme.bgPanel

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space4
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2

        Text {
            text: "Forma"
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Text {
            text: "/"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeMd
        }
        Text {
            text: root.documentName
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeMd
        }
    }

    // Câmp de căutare (command palette vine în Faza 1).
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(360, root.width * 0.4)
        height: 28
        radius: Theme.radiusMd
        color: searchHover.hovered ? Theme.bgActive : Theme.bgHover
        border.color: Theme.borderSubtle

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: Theme.space3
            anchors.verticalCenter: parent.verticalCenter
            text: "Caută comenzi…"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeMd
        }
        Text {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space3
            anchors.verticalCenter: parent.verticalCenter
            text: Qt.platform.os === "osx" ? "⌘K" : "Ctrl K"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeSm
            font.family: Theme.fontMono
        }

        HoverHandler { id: searchHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.commandPaletteRequested() }
    }

    IconButton {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        implicitWidth: 32
        implicitHeight: 32
        glyph: Theme.dark ? "☾" : "☀"
        onClicked: Theme.cycleMode()
    }

    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.borderSubtle
    }
}
