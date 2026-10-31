import QtQuick
import Aion

// O bifă mică (de exemplu pe rândurile unui tabel). `toggled(checked)` la click; starea o ține părintele.
Rectangle {
    id: root

    property bool checked: false
    signal toggled(bool checked)

    implicitWidth: 16
    implicitHeight: 16
    radius: Theme.radiusSm
    color: root.checked ? Theme.accent : "transparent"
    border.color: root.checked ? Theme.accent : area.containsMouse ? Theme.textSecondary : Theme.borderStrong

    Icon {
        anchors.centerIn: parent
        visible: root.checked
        name: "check"
        size: 12
        color: Theme.accentText
    }
    MouseArea {
        id: area
        anchors.fill: parent
        anchors.margins: -4
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.toggled(!root.checked)
    }
}
