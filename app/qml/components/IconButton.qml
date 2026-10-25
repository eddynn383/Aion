import QtQuick
import Forma

// Buton pătrat pentru toolbar. Până la integrarea iconițelor Lucide (Faza 1)
// afișează un glif text.
Item {
    id: root

    property string glyph: ""
    property string tooltip: ""
    property string shortcut: ""
    property bool checked: false

    signal clicked()

    implicitWidth: Theme.controlHeight + 4
    implicitHeight: Theme.controlHeight + 4

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: root.checked ? Theme.accent
             : tap.pressed ? Theme.bgActive
             : hover.hovered ? Theme.bgHover
             : "transparent"

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }
    }

    Text {
        anchors.centerIn: parent
        text: root.glyph
        color: root.checked ? Theme.accentText : Theme.textPrimary
        font.pixelSize: Theme.fontSizeLg
    }

    HoverHandler { id: hover; cursorShape: Qt.PointingHandCursor }
    TapHandler { id: tap; onTapped: root.clicked() }

    // Tooltip simplu de tip „pill”, deasupra butonului.
    Rectangle {
        visible: hover.hovered && root.tooltip.length > 0
        anchors.bottom: parent.top
        anchors.bottomMargin: Theme.space2
        anchors.horizontalCenter: parent.horizontalCenter
        width: tipRow.implicitWidth + Theme.space3 * 2
        height: 26
        radius: Theme.radiusSm
        color: Theme.bgElevated
        border.color: Theme.borderSubtle

        Row {
            id: tipRow
            anchors.centerIn: parent
            spacing: Theme.space2

            Text {
                text: root.tooltip
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
            }
            Text {
                visible: root.shortcut.length > 0
                text: root.shortcut
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeSm
                font.family: Theme.fontMono
            }
        }
    }
}
