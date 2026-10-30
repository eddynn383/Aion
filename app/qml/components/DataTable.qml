import QtQuick
import Aion

// Tabel simplu, doar pentru citire: antet fix și rânduri care derulează.
// columns: [{ title, width (fracțiune din lățime), align: "left" | "right" }]; rows: listă de liste de texte.
// Opțional, o coloană de bife la început (`checkColumn`): `rowKeys` dă cheia fiecărui rând, `checkedKeys` (obiect
// cheie → true) starea; un click emite `rowToggled(key, checked)`, iar părintele schimbă `checkedKeys`.
// `dimmedRows` (obiect index → true) estompează rândurile date (de exemplu piesele ascunse).
Item {
    id: root

    property var columns: []
    property var rows: []
    property string emptyText: "Nimic de afișat."
    property bool checkColumn: false
    property var rowKeys: []
    property var checkedKeys: ({})
    property var dimmedRows: ({})
    readonly property real checkWidth: root.checkColumn ? 28 : 0

    signal rowToggled(string key, bool checked)

    function columnWidth(index) {
        return (root.width - root.checkWidth) * (root.columns[index].width ?? (1 / root.columns.length))
    }

    Row {
        id: header
        width: parent.width
        height: 28
        Item {
            width: root.checkWidth
            height: header.height
        }
        Repeater {
            model: root.columns
            delegate: Text {
                required property var modelData
                required property int index
                width: root.columnWidth(index)
                height: header.height
                verticalAlignment: Text.AlignVCenter
                horizontalAlignment: modelData.align === "right" ? Text.AlignRight : Text.AlignLeft
                rightPadding: Theme.space2
                leftPadding: index === 0 ? Theme.space2 : 0
                text: modelData.title
                elide: Text.ElideRight
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
                font.weight: Font.Medium
            }
        }
    }
    Separator {
        anchors.top: header.bottom
        width: parent.width
    }

    ListView {
        id: list
        anchors.top: header.bottom
        anchors.topMargin: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        model: root.rows
        delegate: Rectangle {
            id: rowItem
            required property var modelData
            required property int index
            width: ListView.view.width
            height: 26
            color: index % 2 === 1 ? Theme.bgHover : "transparent"
            radius: Theme.radiusSm
            opacity: root.dimmedRows[rowItem.index] ? 0.55 : 1
            Row {
                anchors.fill: parent
                Item {
                    width: root.checkWidth
                    height: rowItem.height
                    visible: root.checkColumn
                    CheckBox {
                        anchors.centerIn: parent
                        readonly property string key: root.rowKeys[rowItem.index] ?? ""
                        checked: root.checkedKeys[key] === true
                        onToggled: (value) => root.rowToggled(key, value)
                    }
                }
                Repeater {
                    model: root.columns.length
                    delegate: Text {
                        required property int index
                        width: root.columnWidth(index)
                        height: rowItem.height
                        verticalAlignment: Text.AlignVCenter
                        horizontalAlignment: root.columns[index].align === "right" ? Text.AlignRight : Text.AlignLeft
                        rightPadding: Theme.space2
                        leftPadding: index === 0 ? Theme.space2 : 0
                        text: rowItem.modelData[index] ?? ""
                        elide: Text.ElideRight
                        color: index === 0 ? Theme.textPrimary : Theme.textSecondary
                        font.pixelSize: Theme.fontSizeSm
                        font.family: root.columns[index].align === "right" ? Theme.fontMono : Theme.fontUi
                    }
                }
            }
        }
    }

    Text {
        visible: root.rows.length === 0
        anchors.centerIn: list
        width: parent.width - Theme.space8
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: root.emptyText
        color: Theme.textTertiary
        font.pixelSize: Theme.fontSizeSm
    }
}
