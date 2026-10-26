import QtQuick
import Aion

// Meniu contextual (click dreapta), în stilul shadcn / Figma: rânduri compacte, scurtături în dreapta,
// separatori, acțiuni distructive în roșu. items: listă de { id, name, shortcut, enabled, destructive }
// sau { separator: true }.
Popover {
    id: root

    property var items: []
    property int currentIndex: -1
    property real rowsWidth: 180

    signal triggered(string id)

    // Deschide meniul la poziția dată (în coordonatele lui `parent`), fără să iasă din fereastră.
    function openAt(px, py) {
        rowsWidth = 180
        currentIndex = -1
        const p = parent
        x = Math.max(Theme.space2, Math.min(px, p.width - implicitWidth - Theme.space2))
        y = Math.max(Theme.space2, Math.min(py, p.height - implicitHeight - Theme.space2))
        open()
    }

    function activate(index) {
        const item = root.items[index]
        if (!item || item.separator || item.enabled === false)
            return
        root.close()
        root.triggered(item.id)
    }

    function step(delta) {
        const n = root.items.length
        for (let k = 1; k <= n; ++k) {
            const i = ((root.currentIndex < 0 ? (delta > 0 ? -1 : 0) : root.currentIndex) + delta * k + n * 2) % n
            const item = root.items[i]
            if (!item.separator && item.enabled !== false) {
                root.currentIndex = i
                return
            }
        }
    }

    padding: Theme.space1
    margins: 0
    focus: true
    implicitWidth: rowsWidth + leftPadding + rightPadding

    contentItem: Column {
        focus: true
        spacing: 0

        Keys.onUpPressed: root.step(-1)
        Keys.onDownPressed: root.step(1)
        Keys.onReturnPressed: root.activate(root.currentIndex)
        Keys.onEnterPressed: root.activate(root.currentIndex)
        Keys.onEscapePressed: root.close()

        Repeater {
            model: root.items

            delegate: Item {
                id: row
                required property var modelData
                required property int index
                readonly property bool separator: modelData.separator === true
                readonly property bool enabledItem: !separator && modelData.enabled !== false
                readonly property bool highlighted: enabledItem && root.currentIndex === index
                readonly property color foreground: !enabledItem ? Theme.textTertiary
                    : modelData.destructive === true ? Theme.danger
                    : Theme.textPrimary

                width: root.availableWidth
                height: separator ? 9 : 30
                implicitWidth: separator ? 0 : Theme.space2 + label.implicitWidth + Theme.space8
                    + shortcutText.implicitWidth + Theme.space2
                Component.onCompleted: root.rowsWidth = Math.max(root.rowsWidth, implicitWidth)

                Rectangle {
                    visible: row.separator
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: -root.leftPadding
                    anchors.rightMargin: -root.rightPadding
                    height: 1
                    color: Theme.borderSubtle
                }
                Rectangle {
                    visible: !row.separator
                    anchors.fill: parent
                    radius: Theme.radiusSm
                    color: row.highlighted ? (row.modelData.destructive === true ? Qt.rgba(0.94, 0.27, 0.27, 0.14)
                                                                                 : Theme.bgHover)
                                           : "transparent"
                }
                Text {
                    id: label
                    visible: !row.separator
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.modelData.name || ""
                    color: row.foreground
                    font.pixelSize: Theme.fontSizeMd
                }
                Text {
                    id: shortcutText
                    visible: !row.separator
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space2
                    anchors.verticalCenter: parent.verticalCenter
                    text: AppInfo.shortcutText(row.modelData.shortcut || "")
                    color: Theme.textTertiary
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
