import QtQuick
import QtQuick.Templates as T
import Aion

// Command palette (⌘K): caută fuzzy printre comenzi și le execută cu Enter.
Popover {
    id: root

    property var commands: []

    signal commandTriggered(string id)

    function trigger(row) {
        const id = search.commandIdAt(row)
        if (id.length === 0)
            return
        root.close()
        root.commandTriggered(id)
    }

    function openWithQuery(text) {
        open()
        field.text = text
    }

    modal: true
    focus: true
    padding: 0
    width: Math.min(600, parent ? parent.width - Theme.space8 * 2 : 600)
    height: Math.min(contentColumn.implicitHeight, parent ? parent.height * 0.6 : 480)
    x: parent ? (parent.width - width) / 2 : 0
    y: parent ? parent.height * 0.14 : 0

    onAboutToShow: {
        field.text = ""
        list.currentIndex = 0
    }
    onOpened: field.forceActiveFocus()

    CommandSearch {
        id: search
        commands: root.commands
        query: field.text
        highlightColor: Theme.accent
        onQueryChanged: list.currentIndex = 0
    }

    contentItem: Column {
        id: contentColumn

        T.TextField {
            id: field
            width: parent.width
            height: 48
            leftPadding: Theme.space4 + Theme.iconSizeMd + Theme.space3
            rightPadding: Theme.space4
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            selectionColor: Theme.accentSubtle
            selectedTextColor: Theme.textPrimary
            font.pixelSize: Theme.fontSizeLg
            background: null

            Icon {
                anchors.left: parent.left
                anchors.leftMargin: Theme.space4
                anchors.verticalCenter: parent.verticalCenter
                name: "search"
                color: Theme.textTertiary
            }
            Text {
                anchors.fill: parent
                anchors.leftMargin: field.leftPadding
                verticalAlignment: Text.AlignVCenter
                visible: field.text.length === 0
                text: "Caută o comandă…"
                color: Theme.textTertiary
                font: field.font
            }

            Keys.onDownPressed: list.currentIndex = Math.min(list.currentIndex + 1, list.count - 1)
            Keys.onUpPressed: list.currentIndex = Math.max(list.currentIndex - 1, 0)
            Keys.onReturnPressed: root.trigger(list.currentIndex)
            Keys.onEnterPressed: root.trigger(list.currentIndex)
            Keys.onEscapePressed: root.close()
        }

        Separator { width: parent.width }

        ListView {
            id: list
            width: parent.width
            height: Math.min(contentHeight, 8 * 40) + Theme.space2 * 2
            clip: true
            topMargin: Theme.space2
            bottomMargin: Theme.space2
            model: search
            currentIndex: 0
            boundsBehavior: Flickable.StopAtBounds
            highlightMoveDuration: 0
            visible: count > 0

            delegate: Item {
                id: row
                required property int index
                required property string titleHtml
                required property string category
                required property string shortcut
                required property string icon

                width: list.width
                height: 40

                Rectangle {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.space2
                    anchors.rightMargin: Theme.space2
                    radius: Theme.radiusMd
                    color: list.currentIndex === row.index ? Theme.bgActive : "transparent"
                }

                Icon {
                    id: rowIcon
                    anchors.left: parent.left
                    anchors.leftMargin: Theme.space4
                    anchors.verticalCenter: parent.verticalCenter
                    name: row.icon
                    color: list.currentIndex === row.index ? Theme.textPrimary : Theme.textSecondary
                }
                Text {
                    anchors.left: rowIcon.right
                    anchors.leftMargin: Theme.space3
                    anchors.right: meta.left
                    anchors.rightMargin: Theme.space3
                    anchors.verticalCenter: parent.verticalCenter
                    text: row.titleHtml
                    textFormat: Text.StyledText
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeMd
                    elide: Text.ElideRight
                }
                Row {
                    id: meta
                    anchors.right: parent.right
                    anchors.rightMargin: Theme.space4
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space2
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: row.category
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeSm
                    }
                    Kbd {
                        anchors.verticalCenter: parent.verticalCenter
                        sequence: row.shortcut
                    }
                }

                HoverHandler {
                    onHoveredChanged: if (hovered) list.currentIndex = row.index
                }
                TapHandler { onTapped: root.trigger(row.index) }
            }
        }

        Text {
            visible: list.count === 0
            width: parent.width
            height: 72
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: "Nicio comandă pentru „" + field.text + "”"
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeMd
        }

        Separator { width: parent.width }

        Row {
            height: 34
            leftPadding: Theme.space4
            spacing: Theme.space4
            Repeater {
                model: [["↑↓", "navighează"], ["↵", "execută"], ["esc", "închide"]]
                delegate: Row {
                    required property var modelData
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: Theme.space1 + 2
                    Text {
                        text: modelData[0]
                        color: Theme.textSecondary
                        font.family: Theme.fontMono
                        font.pixelSize: Theme.fontSizeXs
                    }
                    Text {
                        text: modelData[1]
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
            }
        }
    }
}
