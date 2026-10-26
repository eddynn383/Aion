import QtQuick
import Aion

// Scurtătură de tastatură afișată ca o etichetă mică („⌘K”).
Rectangle {
    id: root

    // Secvența în format Qt („Ctrl+K”); se afișează în forma platformei.
    property string sequence: ""

    visible: sequence.length > 0
    implicitWidth: label.implicitWidth + Theme.space1 * 2 + 2
    implicitHeight: 18
    radius: 4
    color: Theme.bgHover
    border.color: Theme.borderSubtle

    Text {
        id: label
        anchors.centerIn: parent
        text: AppInfo.shortcutText(root.sequence)
        color: Theme.textSecondary
        font.family: Theme.fontMono
        font.pixelSize: Theme.fontSizeXs
    }
}
