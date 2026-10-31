import QtQuick
import Aion

// Bara de sus, integrată în fereastră (pe macOS, sub butoanele native).
// Se poate trage pentru a muta fereastra; dublu-click o maximizează.
Rectangle {
    id: root

    property string documentName: qsTrId("TitleBar.fara_titlu")
    // Documentul are modificări nesalvate: un punct după nume (ca în Figma).
    property bool modified: false
    property bool leftPanelVisible: true
    property bool rightPanelVisible: true

    signal commandPaletteRequested()
    signal toggleLeftPanel()
    signal toggleRightPanel()

    height: Theme.titleBarHeight
    color: Theme.bgPanel

    // Mutarea ferestrei și maximizarea; elementele interactive de deasupra au prioritate.
    DragHandler {
        target: null
        grabPermissions: PointerHandler.CanTakeOverFromAnything
        onActiveChanged: if (active) root.Window.window.startSystemMove()
    }
    TapHandler {
        onDoubleTapped: {
            const w = root.Window.window
            if (w.visibility === Window.Maximized)
                w.showNormal()
            else
                w.showMaximized()
        }
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.trafficLightsInset + Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space1

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            implicitWidth: 28
            implicitHeight: 28
            iconName: "panel-left"
            iconSize: Theme.iconSizeMd
            tooltip: qsTrId("TitleBar.panoul_din_stanga")
            checked: root.leftPanelVisible
            solidChecked: false
            onClicked: root.toggleLeftPanel()
        }

        Item { width: Theme.space2; height: 1 }

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTrId("TitleBar.aion")
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            leftPadding: Theme.space1
            rightPadding: Theme.space1
            text: "/"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeMd
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.documentName
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeMd
        }
        Rectangle {
            visible: root.modified
            anchors.verticalCenter: parent.verticalCenter
            width: 6
            height: 6
            radius: 3
            color: Theme.textTertiary
        }
    }

    // Câmpul care deschide command palette.
    Rectangle {
        id: search
        anchors.centerIn: parent
        width: Math.min(380, root.width * 0.34)
        height: 30
        radius: Theme.radiusMd
        color: searchHover.hovered ? Theme.bgActive : Theme.bgHover
        border.color: Theme.borderSubtle

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }

        Icon {
            id: searchIcon
            anchors.left: parent.left
            anchors.leftMargin: Theme.space3
            anchors.verticalCenter: parent.verticalCenter
            name: "search"
            size: Theme.iconSizeSm
            color: Theme.textTertiary
        }
        Text {
            anchors.left: searchIcon.right
            anchors.leftMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            text: qsTrId("TitleBar.cauta_comenzi")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeMd
        }
        Kbd {
            anchors.right: parent.right
            anchors.rightMargin: Theme.space2
            anchors.verticalCenter: parent.verticalCenter
            sequence: "Ctrl+K"
        }

        HoverHandler { id: searchHover; cursorShape: Qt.PointingHandCursor }
        TapHandler { onTapped: root.commandPaletteRequested() }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space1

        IconButton {
            implicitWidth: 28
            implicitHeight: 28
            iconName: Theme.mode === 0 ? "monitor" : Theme.dark ? "moon" : "sun"
            iconSize: Theme.iconSizeMd
            tooltip: qsTrId("TitleBar.tema") + Theme.modeLabel
            onClicked: Theme.cycleMode()
        }
        IconButton {
            implicitWidth: 28
            implicitHeight: 28
            iconName: "panel-right"
            iconSize: Theme.iconSizeMd
            tooltip: qsTrId("TitleBar.panoul_din_dreapta")
            checked: root.rightPanelVisible
            solidChecked: false
            onClicked: root.toggleRightPanel()
        }
    }

    Separator {
        anchors.bottom: parent.bottom
        width: parent.width
    }
}
