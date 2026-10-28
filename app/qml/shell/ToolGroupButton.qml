import QtQuick
import QtQuick.Templates as T
import Aion

// Un grup de unelte din toolbar (ca în Figma): butonul arată unealta aleasă ultima dată din grup,
// iar săgeata de lângă el deschide meniul cu toate uneltele grupului.
Row {
    id: root

    property var group: ({ tools: [] })
    property string currentToolId: ""
    property bool active: false
    // Scurtătura afișată a unei unelte (personalizabilă în Preferințe).
    property var keyFor: (t) => t.key || ""

    readonly property var currentTool: group.tools.find(t => t.id === currentToolId) || group.tools[0]
    readonly property bool hasMenu: group.tools.length > 1

    signal toolSelected(string id)

    spacing: 0

    IconButton {
        anchors.verticalCenter: parent.verticalCenter
        iconName: root.currentTool.icon
        tooltip: root.currentTool.name
        shortcut: root.keyFor(root.currentTool)
        guideId: root.currentTool.id
        checked: root.active
        onClicked: root.toolSelected(root.currentTool.id)
    }

    T.AbstractButton {
        id: chevron
        anchors.verticalCenter: parent.verticalCenter
        visible: root.hasMenu
        implicitWidth: 18
        implicitHeight: Theme.controlHeight + 4
        hoverEnabled: true
        focusPolicy: Qt.NoFocus

        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: "chevron-down"
                size: 12
                color: chevron.hovered || menu.opened ? Theme.textPrimary : Theme.textTertiary
            }
        }
        background: Rectangle {
            radius: Theme.radiusSm
            color: menu.opened ? Theme.bgActive : chevron.hovered ? Theme.bgHover : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.motionFast } }
        }

        onClicked: menu.opened ? menu.close() : menu.open()

        Menu {
            id: menu
            // Toolbar-ul e jos în fereastră, deci meniul se deschide în sus, aliniat cu grupul.
            x: -root.width + chevron.width
            y: -height - Theme.space3
            // O unealtă cu `section` începe o secțiune nouă (un titlu mic deasupra ei, ca „2D” / „3D”).
            items: root.group.tools.reduce((all, t) => {
                if (t.section)
                    all.push({ id: "", header: t.section, enabled: false })
                all.push({ id: t.id, icon: t.icon, name: t.name, shortcut: root.keyFor(t), checked: t.id === root.currentTool.id })
                return all
            }, [])
            onTriggered: (id) => root.toolSelected(id)
        }
    }
}
