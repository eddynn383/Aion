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
            text: qsTrId("AboutDialog.aion")
            color: Theme.textPrimary
            font.pixelSize: 22
            font.weight: Font.DemiBold
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTrId("AboutDialog.aion_modeleaza_ce_ramane")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeMd
        }
        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTrId("AboutDialog.versiunea") + AppInfo.version + " · " + AppInfo.graphicsApi + (AppInfo.debugBuild ? qsTrId("AboutDialog.debug") : "")
            color: Theme.textTertiary
            font.family: Theme.fontMono
            font.pixelSize: Theme.fontSizeXs
        }
        Item { width: 1; height: Theme.space2 }
        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
            text: qsTrId("AboutDialog.construit_cu_qt_glm_earcut_manifold_tinygltf_min")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeXs
            lineHeight: 1.3
        }
        Button {
            anchors.horizontalCenter: parent.horizontalCenter
            text: qsTrId("AboutDialog.inchide")
            onClicked: root.close()
        }
    }
}
