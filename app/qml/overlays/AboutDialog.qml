import QtQuick
import Aion

// „Despre Aion”: versiune, backend grafic, licențe.
Popover {
    id: root

    modal: true
    anchors.centerIn: parent
    width: 360
    padding: Theme.space8

    contentItem: Column {
        spacing: Theme.space3

        Image {
            anchors.horizontalCenter: parent.horizontalCenter
            source: "qrc:/aion/branding/aion-icon.svg"
            sourceSize: Qt.size(96, 96)
            width: 96
            height: 96
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Aion"
            color: Theme.textPrimary
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Aion — modelează ce rămâne."
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeMd
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Versiunea " + AppInfo.version + " · " + AppInfo.graphicsApi + (AppInfo.debugBuild ? " · debug" : "")
            color: Theme.textTertiary
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeXs
        }
        Item { width: 1; height: Theme.space2 }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: "Construit cu Qt, glm, earcut, Manifold, tinygltf, miniz, nlohmann/json.\n"
                  + "Fonturi: Inter și Geist Mono (OFL). Iconițe: Lucide (ISC)."
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeXs
            lineHeight: 1.3
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: "Închide"
            onClicked: root.close()
        }
    }
}
