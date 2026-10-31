import QtQuick
import Aion

// Materialul unei părți a selecției, ca „pastilă” (cercul culorii + numele). Click: alegi alt material din model sau
// din bibliotecă, sau „Fără material”. `material`: { id, name, color } (id 0 = implicit, −1 = mixt).
Rectangle {
    id: root

    property var document
    property var material: ({ id: 0, name: qsTrId("MaterialSwatch.implicit") })
    property string label: ""

    signal picked(var material)

    readonly property bool hasColor: (material.id ?? 0) > 0

    implicitWidth: 200
    implicitHeight: Theme.controlHeight
    radius: Theme.radiusMd
    color: area.containsMouse || picker.opened ? Theme.bgHover : Theme.bgInput
    border.color: picker.opened ? Theme.accent : "transparent"

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Theme.space2
        anchors.right: chevron.left
        anchors.verticalCenter: parent.verticalCenter
        spacing: Theme.space2

        Text {
            visible: root.label.length > 0
            anchors.verticalCenter: parent.verticalCenter
            text: root.label
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeSm
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 18
            height: 18
            radius: 9
            color: root.hasColor ? root.material.color : "transparent"
            border.color: Theme.borderStrong
            // Implicit: o diagonală (fără material).
            Rectangle {
                visible: !root.hasColor
                anchors.centerIn: parent
                width: 18
                height: 1.5
                rotation: -45
                color: Theme.textTertiary
            }
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.material.name ?? qsTrId("MaterialSwatch.implicit")
            elide: Text.ElideRight
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeSm
        }
    }
    Icon {
        id: chevron
        anchors.right: parent.right
        anchors.rightMargin: Theme.space2
        anchors.verticalCenter: parent.verticalCenter
        name: "chevron-down"
        size: 12
        color: Theme.textTertiary
    }
    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        onClicked: picker.opened ? picker.close() : picker.open()
    }

    Popover {
        id: picker
        y: root.height + Theme.space1
        x: root.width - width
        width: 264
        padding: Theme.space3

        component Swatch: Rectangle {
            id: sw
            property color swatchColor: "transparent"
            property bool none: false
            property string name: ""
            signal chosen()
            width: 28
            height: 28
            radius: 14
            color: none ? "transparent" : swatchColor
            border.color: swArea.containsMouse ? Theme.accent : Theme.borderStrong
            border.width: swArea.containsMouse ? 2 : 1
            Rectangle {
                visible: sw.none
                anchors.centerIn: parent
                width: 26
                height: 1.5
                rotation: -45
                color: Theme.textTertiary
            }
            MouseArea {
                id: swArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: sw.chosen()
            }
            Tooltip {
                visible: swArea.containsMouse
                text: sw.name
            }
        }

        contentItem: Column {
            spacing: Theme.space2
            Text {
                text: qsTrId("MaterialSwatch.in_model")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
            Flow {
                width: 240
                spacing: Theme.space1 + 2
                Swatch {
                    none: true
                    name: qsTrId("MaterialSwatch.fara_material")
                    onChosen: { root.picked({}); picker.close() }
                }
                Repeater {
                    model: root.document ? root.document.modelMaterials : []
                    delegate: Swatch {
                        required property var modelData
                        swatchColor: modelData.color
                        name: modelData.name
                        onChosen: { root.picked({ id: modelData.id }); picker.close() }
                    }
                }
            }
            Text {
                text: qsTrId("MaterialSwatch.biblioteca")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
            Flow {
                width: 240
                spacing: Theme.space1 + 2
                Repeater {
                    model: root.document ? root.document.libraryMaterials : []
                    delegate: Swatch {
                        required property var modelData
                        required property int index
                        swatchColor: modelData.color
                        name: modelData.name
                        onChosen: { root.picked({ library: index }); picker.close() }
                    }
                }
            }
        }
    }
}
