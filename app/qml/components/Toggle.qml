import QtQuick
import QtQuick.Templates as T
import Forma

// Comutator on/off, cu etichetă în stânga.
T.Switch {
    id: root

    implicitWidth: implicitContentWidth + leftPadding + rightPadding
    implicitHeight: Theme.controlHeightSm
    spacing: Theme.space3
    padding: 0
    font.pixelSize: Theme.fontSizeMd

    indicator: Rectangle {
        x: root.width - width
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 16
        radius: 8
        color: root.checked ? Theme.accent : Theme.bgActive
        border.color: root.checked ? "transparent" : Theme.borderSubtle

        Behavior on color { ColorAnimation { duration: Theme.motionFast } }

        Rectangle {
            x: root.checked ? parent.width - width - 2 : 2
            anchors.verticalCenter: parent.verticalCenter
            width: 12
            height: 12
            radius: 6
            color: root.checked ? Theme.accentText : Theme.textSecondary

            Behavior on x { NumberAnimation { duration: Theme.motionFast; easing.type: Easing.OutCubic } }
        }
    }

    contentItem: Text {
        rightPadding: root.indicator.width + root.spacing
        text: root.text
        font: root.font
        color: root.enabled ? Theme.textSecondary : Theme.textTertiary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
