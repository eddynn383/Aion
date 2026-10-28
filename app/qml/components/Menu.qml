import QtQuick
import Aion

// Meniu derulant în stilul Figma: bifă pentru elementul curent, iconiță, nume și scurtătură.
// items: listă de { id, icon, name, shortcut, checked, enabled }; { header: "Titlu" } e titlul unei secțiuni.
Popover {
    id: root

    property var items: []
    property int currentIndex: -1
    property real rowsWidth: 0

    signal triggered(string id)

    function activate(index) {
        const item = root.items[index]
        if (!item || item.enabled === false)
            return
        root.close()
        root.triggered(item.id)
    }

    function step(delta) {
        const n = root.items.length
        for (let k = 1; k <= n; ++k) {
            const i = ((root.currentIndex < 0 ? (delta > 0 ? -1 : 0) : root.currentIndex) + delta * k + n * 2) % n
            if (root.items[i].enabled !== false) {
                root.currentIndex = i
                return
            }
        }
    }

    padding: Theme.space1 + 2
    focus: true
    implicitWidth: rowsWidth + leftPadding + rightPadding

    onAboutToShow: currentIndex = root.items.findIndex(i => i.checked)

    contentItem: Column {
        focus: true
        spacing: 2

        Keys.onUpPressed: root.step(-1)
        Keys.onDownPressed: root.step(1)
        Keys.onReturnPressed: root.activate(root.currentIndex)
        Keys.onEnterPressed: root.activate(root.currentIndex)
        Keys.onEscapePressed: root.close()

        Repeater {
            model: root.items

            delegate: Rectangle {
                id: row
                required property var modelData
                required property int index
                readonly property bool enabledItem: modelData.enabled !== false
                readonly property bool highlighted: enabledItem && root.currentIndex === index
                readonly property color foreground: highlighted ? Theme.accentText
                    : enabledItem ? Theme.textPrimary : Theme.textTertiary

                readonly property bool isHeader: modelData.header !== undefined
                width: root.availableWidth
                height: isHeader ? 24 : 32
                radius: Theme.radiusMd
                color: highlighted ? Theme.accent : "transparent"

                implicitWidth: Theme.space2 + 16 + Theme.space2 + 16 + Theme.space3 + label.implicitWidth
                    + Theme.space8 + shortcutText.implicitWidth + Theme.space3
                Component.onCompleted: root.rowsWidth = Math.max(root.rowsWidth, implicitWidth)

                Text {
                    visible: row.isHeader
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 3
                    text: row.modelData.header ?? ""
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
                    font.weight: Font.DemiBold
                    font.letterSpacing: 0.6
                }
                Icon {
                    id: check
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    size: 16
                    name: "check"
                    color: row.foreground
                    visible: row.modelData.checked === true
                }
                Icon {
                    id: icon
                    anchors.left: check.right
                    anchors.leftMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    size: 16
                    name: row.modelData.icon || ""
                    visible: !row.isHeader
                    color: row.foreground
                }
                Text {
                    id: label
                    anchors.left: icon.right
                    anchors.leftMargin: Theme.space3
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.name ?? ""
                    color: row.foreground
                    font.pixelSize: Theme.fontSizeMd
                }
                Text {
                    id: shortcutText
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space3
                    anchors.verticalCenter: parent.verticalCenter
                    text: AppInfo.shortcutText(row.modelData.shortcut || "")
                    color: row.highlighted ? Theme.accentText : Theme.textTertiary
                    font.pixelSize: Theme.fontSizeSm
                }

                HoverHandler {
                    enabled: row.enabledItem
                    onHoveredChanged: if (hovered) root.currentIndex = row.index
                }
                TapHandler {
                    enabled: row.enabledItem
                    onTapped: root.activate(row.index)
                }
            }
        }
    }
}
