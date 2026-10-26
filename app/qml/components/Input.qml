import QtQuick
import QtQuick.Templates as T
import Aion

// Câmp de text, cu iconiță opțională în stânga.
T.TextField {
    id: root

    property string iconName: ""

    implicitWidth: 200
    implicitHeight: Theme.controlHeight
    leftPadding: iconName.length > 0 ? Theme.space3 + Theme.iconSizeSm + Theme.space2 : Theme.space3
    rightPadding: Theme.space3
    verticalAlignment: TextInput.AlignVCenter
    color: Theme.textPrimary
    placeholderTextColor: Theme.textTertiary
    selectionColor: Theme.accentSubtle
    selectedTextColor: Theme.textPrimary
    font.pixelSize: Theme.fontSizeMd

    Icon {
        visible: root.iconName.length > 0
        anchors.left: parent.left
        anchors.leftMargin: Theme.space3
        anchors.verticalCenter: parent.verticalCenter
        name: root.iconName
        size: Theme.iconSizeSm
        color: Theme.textTertiary
    }

    Text {
        anchors.fill: parent
        anchors.leftMargin: root.leftPadding
        verticalAlignment: Text.AlignVCenter
        visible: root.text.length === 0 && root.preeditText.length === 0
        text: root.placeholderText
        color: root.placeholderTextColor
        font: root.font
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: Theme.radiusMd
        color: Theme.bgInput
        border.width: root.activeFocus ? 2 : 1
        border.color: root.activeFocus ? Theme.focusRing : root.hovered ? Theme.borderStrong : Theme.borderSubtle

        Behavior on border.color { ColorAnimation { duration: Theme.motionFast } }
    }
}
