import QtQuick
import Aion

// Comutatorul modurilor de lucru, în dreapta toolbar-ului (ca Design / Prototype / Dev Mode în Figma):
// Modelare, Animare, Interacțiune. Modul activ e plin, în culoarea de accent.
Rectangle {
    id: root

    property int mode: 0
    // Scurtătura afișată a unei comenzi („mode.model” …), personalizabilă în Preferințe.
    property var shortcutFor: (id) => ""
    signal modeSelected(int mode)

    readonly property var modes: [
        { id: "mode.model", icon: "box", name: qsTrId("ModeSwitch.modelare") },
        { id: "mode.animate", icon: "film", name: qsTrId("ModeSwitch.animare") },
        { id: "mode.interact", icon: "hand-pointer", name: qsTrId("ModeSwitch.interactiune") }
    ]

    implicitWidth: modeRow.implicitWidth + 4
    implicitHeight: modeRow.implicitHeight + 4
    radius: Theme.radiusMd + 2
    color: Theme.bgHover

    Row {
        id: modeRow
        anchors.centerIn: parent
        spacing: 0
        Repeater {
            model: root.modes
            delegate: IconButton {
                required property var modelData
                required property int index
                implicitWidth: Theme.controlHeight
                implicitHeight: Theme.controlHeight
                iconName: modelData.icon
                iconSize: Theme.iconSizeMd
                tooltip: qsTrId("ModeSwitch.modul") + modelData.name
                shortcut: root.shortcutFor(modelData.id)
                checked: root.mode === index
                onClicked: root.modeSelected(index)
            }
        }
    }
}
