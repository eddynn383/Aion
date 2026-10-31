import QtQuick
import Aion

// Interacțiunile din model: fiecare obiect care are reguli, cu regulile pe scurt („Click → Mută, Ascunde”).
// Click pe un obiect îl selectează și deschide editorul cu blocuri; „+” din antet adaugă reguli obiectului selectat.
Item {
    id: root

    property var document
    signal editRequested(double instanceId, string name)

    readonly property var triggerLabels: ({ click: qsTrId("InteractionsPanel.click"), rightClick: qsTrId("InteractionsPanel.click_dreapta"), doubleClick: qsTrId("InteractionsPanel.dublu_click"),
        hoverEnter: qsTrId("InteractionsPanel.cursorul_intra"), hoverLeave: qsTrId("InteractionsPanel.cursorul_iese"), longPress: qsTrId("InteractionsPanel.apasare_lunga"), drag: qsTrId("InteractionsPanel.tragere"),
        key: "Tasta" })
    readonly property var actionLabels: ({ moveBy: qsTrId("InteractionsPanel.muta"), moveTo: qsTrId("InteractionsPanel.muta_la"), rotateBy: qsTrId("InteractionsPanel.roteste"), scaleTo: qsTrId("InteractionsPanel.scaleaza"),
        show: qsTrId("InteractionsPanel.arata"), hide: qsTrId("InteractionsPanel.ascunde"), toggleVisible: qsTrId("InteractionsPanel.comuta_vizibilitatea"), setMaterial: qsTrId("InteractionsPanel.schimba_materialul"),
        playAnimation: qsTrId("InteractionsPanel.porneste_animatia"), goToScene: qsTrId("InteractionsPanel.mergi_la_scena"), wait: qsTrId("InteractionsPanel.asteapta"), alternate: qsTrId("InteractionsPanel.alterneaza") })
    readonly property var shapeIcons: ({ box: "prim-cube", cylinder: "prim-cylinder", cone: "prim-cone", sphere: "prim-sphere" })

    function summary(rule) {
        const trigger = (root.triggerLabels[rule.trigger] ?? rule.trigger) + (rule.trigger === "key" && rule.key ? " " + rule.key : "")
        const actions = rule.actions.length > 0 ? rule.actions.map(a => root.actionLabels[a] ?? a).join(", ") : qsTrId("InteractionsPanel.nicio_actiune")
        return trigger + " → " + actions
    }

    ListView {
        id: list
        anchors.fill: parent
        clip: true
        spacing: 2
        model: root.document ? root.document.interactions : []
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            id: item
            required property var modelData
            width: ListView.view.width
            height: content.implicitHeight + Theme.space2
            radius: Theme.radiusSm
            color: itemHover.hovered ? Theme.bgHover : "transparent"

            // Acțiunile (RowAction: locuri fixe, ca în celelalte secțiuni): șterge regulile, deschide editorul.
            HoverHandler { id: itemHover }
            Row {
                id: itemActions
                anchors.right: parent.right
                anchors.rightMargin: 2
                y: Theme.space1 - 1
                RowAction {
                    shown: itemHover.hovered
                    iconName: "trash-2"
                    tooltip: qsTrId("InteractionsPanel.sterge_interactiunile_obiectului")
                    onClicked: root.document.setRules(item.modelData.id, [])
                }
                RowAction {
                    shown: itemHover.hovered
                    iconName: "pencil"
                    tooltip: qsTrId("InteractionsPanel.editeaza_cu_blocuri")
                    onClicked: {
                        root.document.selectInstance(item.modelData.id)
                        root.editRequested(item.modelData.id, item.modelData.name)
                    }
                }
            }

            MouseArea {
                id: itemArea
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    root.document.selectInstance(item.modelData.id)
                    root.editRequested(item.modelData.id, item.modelData.name)
                }
            }
            Column {
                id: content
                x: Theme.space2
                y: Theme.space1
                width: parent.width - Theme.space2 * 2 - itemActions.width
                spacing: 2
                Row {
                    spacing: Theme.space2
                    Icon {
                        anchors.verticalCenter: parent.verticalCenter
                        name: root.shapeIcons[item.modelData.shape] ?? "shapes"
                        size: Theme.iconSizeSm
                        color: Theme.textSecondary
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: item.modelData.name
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSm
                        font.weight: Font.Medium
                    }
                }
                Repeater {
                    model: item.modelData.rules
                    delegate: Text {
                        required property var modelData
                        leftPadding: Theme.iconSizeSm + Theme.space2
                        width: content.width
                        elide: Text.ElideRight
                        text: root.summary(modelData)
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSizeXs
                    }
                }
            }
        }

        footer: Text {
            width: list.width
            topPadding: list.count > 0 ? Theme.space2 : 0
            wrapMode: Text.Wrap
            text: list.count > 0 ? qsTrId("InteractionsPanel.f5_ruleaza_interactiunile_modul_interactiu")
                                 : qsTrId("InteractionsPanel.nicio_interactiune_selecteaza_un_obiect_si")
            color: Theme.textTertiary
            font.pixelSize: Theme.fontSizeXs
        }
    }
}
