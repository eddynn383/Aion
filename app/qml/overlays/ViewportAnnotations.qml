import QtQuick
import Aion

// Etichetele din viewport: tipul inferenței („Endpoint”, „On Red Axis”), ca o „pastilă” lângă marcaj,
// și cotele live ale segmentelor desenate. Pozițiile vin din Viewport.annotations.
Item {
    id: root

    property var annotations: []

    Repeater {
        model: root.annotations

        delegate: Rectangle {
            id: pill

            required property var modelData
            readonly property bool inference: modelData.kind === "inference"

            // Inferența: deasupra-dreapta marcajului (sub cursor stă Measurements box); cota: centrată.
            x: inference ? modelData.x + 12 : modelData.x - width / 2
            y: inference ? modelData.y - 12 - height : modelData.y - height / 2
            implicitWidth: row.implicitWidth + Theme.space2 * 2
            implicitHeight: 22
            radius: height / 2
            color: Theme.bgElevated
            border.color: Theme.borderStrong
            opacity: 0.96

            Row {
                id: row
                anchors.centerIn: parent
                spacing: Theme.space1 + 2

                Rectangle {
                    visible: pill.inference
                    width: 8
                    height: 8
                    radius: 4
                    anchors.verticalCenter: parent.verticalCenter
                    color: pill.modelData.color ?? Theme.textPrimary
                }
                Icon {
                    visible: pill.inference && pill.modelData.locked === true
                    name: "lock"
                    size: 11
                    color: Theme.textSecondary
                    anchors.verticalCenter: parent.verticalCenter
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: pill.modelData.text
                    color: pill.inference ? Theme.textPrimary : Theme.textSecondary
                    font.family: pill.inference ? Theme.fontUi : Theme.fontMono
                    font.pixelSize: Theme.fontSizeXs
                }
            }
        }
    }
}
