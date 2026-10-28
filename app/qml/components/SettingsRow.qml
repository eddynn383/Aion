import QtQuick
import Aion

// Un rând de setare (Preferințe): titlul și explicația în stânga, controlul în dreapta. `stacked` pune controlul
// sub text, pe toată lățimea (pentru controale late: tab-uri multe, liste). Linia de deasupra apare de la al doilea
// rând al grupului.
Item {
    id: root

    property string title: ""
    property string description: ""
    property bool stacked: false
    property bool compact: false // rânduri mai joase, pentru liste lungi (scurtături)
    readonly property real padding: compact ? Theme.space1 + 2 : Theme.space3
    default property alias control: slot.data
    readonly property real controlWidth: stacked ? width - Theme.space4 * 2 : slot.childrenRect.width

    width: parent ? parent.width : 0
    implicitHeight: Math.max(texts.implicitHeight, stacked ? 0 : slot.childrenRect.height)
        + (stacked ? slot.childrenRect.height + Theme.space3 : 0) + padding * 2
    height: implicitHeight

    Rectangle {
        visible: root.y > 0
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.space4
        height: 1
        color: Theme.borderSubtle
    }

    Column {
        id: texts
        anchors.left: parent.left
        anchors.leftMargin: Theme.space4
        anchors.right: root.stacked ? parent.right : slot.left
        anchors.rightMargin: Theme.space4
        y: root.stacked ? root.padding : (root.height - implicitHeight) / 2
        spacing: 2
        Text {
            width: parent.width
            text: root.title
            wrapMode: Text.Wrap
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
        }
        Text {
            width: parent.width
            visible: root.description.length > 0
            text: root.description
            wrapMode: Text.Wrap
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }

    Item {
        id: slot
        width: root.stacked ? root.controlWidth : childrenRect.width
        height: childrenRect.height
        anchors.right: parent.right
        anchors.rightMargin: Theme.space4
        y: root.stacked ? texts.y + texts.implicitHeight + Theme.space3 : (root.height - height) / 2
    }
}
