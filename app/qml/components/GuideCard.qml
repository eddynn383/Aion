import QtQuick
import Aion
import "../shell/ToolGuides.js" as Guides

// Conținutul unui ghid de unealtă: titlu și scurtătură, animația, ce face și pașii.
// Folosit de tooltip-ul bogat (hover lung) și de turul aplicației.
Column {
    id: root

    property string toolId: ""
    property string shortcut: ""
    property bool playing: true
    readonly property var guide: Guides.guide(toolId)

    spacing: Theme.space3

    Row {
        width: parent.width
        spacing: Theme.space2
        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: root.guide ? root.guide.title : ""
            color: Theme.textPrimary
            font.pixelSize: Theme.fontSizeMd
            font.weight: Font.DemiBold
        }
        Kbd {
            anchors.verticalCenter: parent.verticalCenter
            visible: root.shortcut.length > 0
            sequence: root.shortcut
        }
    }

    Rectangle {
        width: parent.width
        height: demo.implicitHeight * width / demo.implicitWidth
        radius: Theme.radiusMd
        color: "transparent"
        border.color: Theme.borderSubtle
        clip: true
        ToolDemo {
            id: demo
            anchors.fill: parent
            anchors.margins: 1
            tool: root.toolId
            playing: root.playing && root.visible
        }
    }

    Text {
        width: parent.width
        wrapMode: Text.Wrap
        text: root.guide ? AppInfo.platformText(root.guide.summary) : ""
        color: Theme.textPrimary
        font.pixelSize: Theme.fontSizeSm
        lineHeight: 1.25
    }

    Column {
        width: parent.width
        spacing: Theme.space1 + 2
        Repeater {
            model: root.guide ? root.guide.steps : []
            delegate: Row {
                required property string modelData
                required property int index
                width: parent.width
                spacing: Theme.space2
                Rectangle {
                    width: 16
                    height: 16
                    radius: 8
                    color: Theme.accentSubtle
                    Text {
                        anchors.centerIn: parent
                        text: index + 1
                        color: Theme.accent
                        font.pixelSize: 9
                        font.weight: Font.DemiBold
                    }
                }
                Text {
                    width: parent.width - 24
                    wrapMode: Text.Wrap
                    text: AppInfo.platformText(modelData)
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeXs
                    lineHeight: 1.2
                }
            }
        }
    }

    Text {
        visible: root.guide !== null && root.guide.tip.length > 0
        width: parent.width
        wrapMode: Text.Wrap
        text: root.guide ? qsTrId("GuideCard.sfat") + AppInfo.platformText(root.guide.tip) : ""
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeXs
        lineHeight: 1.2
    }
}
