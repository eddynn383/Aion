import QtQuick
import QtQuick.Templates as T
import Aion

// Slider cu etichetă în stânga și valoarea (formatată) în dreapta.
T.Slider {
    id: root

    property string label: ""
    property string valueText: Math.round(value).toString()

    implicitWidth: 240
    implicitHeight: 44
    topPadding: 22
    focusPolicy: Qt.NoFocus

    Text {
        anchors.left: parent.left
        anchors.top: parent.top
        text: root.label
        color: Theme.textSecondary
        font.pixelSize: Theme.fontSizeSm
    }
    Text {
        anchors.right: parent.right
        anchors.top: parent.top
        text: root.valueText
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeSm
        font.family: Theme.fontMono
    }

    background: Rectangle {
        x: root.leftPadding
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: root.availableWidth
        height: 4
        radius: 2
        color: Theme.bgHover
        Rectangle {
            width: root.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }

    handle: Rectangle {
        x: root.leftPadding + root.visualPosition * (root.availableWidth - width)
        y: root.topPadding + root.availableHeight / 2 - height / 2
        width: 14
        height: 14
        radius: 7
        color: Theme.textPrimary
        border.color: Theme.accent
        border.width: root.pressed ? 3 : 2
    }
}
