import QtQuick
import Aion

// Consola de scripturi: editor JavaScript (API-ul `aion`, vezi docs/scripting.md), exemple, rezultatul ultimei rulări
// și plugin-urile instalate. ⌘↵ rulează; o rulare e un singur pas de undo, iar o eroare anulează tot ce a făcut.
Popover {
    id: root

    property var engine
    property string output: ""
    property bool failed: false

    function runScript() {
        const r = root.engine.run(editor.text, qsTrId("ScriptConsole.script"))
        root.failed = !r.ok
        root.output = r.ok ? (r.output.length > 0 ? r.output : qsTrId("ScriptConsole.gata_un_pas_de_undo_z_il_anuleaza"))
                           : qsTrId("ScriptConsole.eroare") + (r.line > 0 ? qsTrId("ScriptConsole.pe_linia") + r.line : "") + ": " + r.error
                             + (r.output.length > 0 ? "\n" + r.output : "")
    }

    modal: false
    anchors.centerIn: parent
    width: Math.min(980, (parent ? parent.width : 980) - 80)
    height: Math.min(640, (parent ? parent.height : 640) - 80)
    padding: Theme.space4

    contentItem: Item {
        // Antetul: titlu, exemple, rulare.
        Row {
            id: header
            width: parent.width
            spacing: Theme.space2
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTrId("ScriptConsole.scripturi")
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
            }
            Item { width: Theme.space4; height: 1 }
            Repeater {
                model: root.engine ? root.engine.examples : []
                delegate: Button {
                    required property var modelData
                    anchors.verticalCenter: parent.verticalCenter
                    variant: "ghost"
                    text: modelData.name
                    onClicked: editor.text = modelData.code
                }
            }
        }
        Row {
            anchors.right: parent.right
            anchors.verticalCenter: header.verticalCenter
            spacing: Theme.space2
            Button {
                variant: "primary"
                iconName: "play"
                text: qsTrId("ScriptConsole.ruleaza")
                onClicked: root.runScript()
            }
            IconButton {
                iconName: "x"
                tooltip: qsTrId("ScriptConsole.inchide")
                onClicked: root.close()
            }
        }

        // Editorul.
        Rectangle {
            id: editorFrame
            anchors.top: header.bottom
            anchors.topMargin: Theme.space3
            anchors.left: parent.left
            anchors.right: sidebar.left
            anchors.rightMargin: Theme.space3
            anchors.bottom: outputFrame.top
            anchors.bottomMargin: Theme.space3
            radius: Theme.radiusMd
            color: Theme.bgInput
            border.color: editor.activeFocus ? Theme.focusRing : Theme.borderSubtle
            Flickable {
                id: flick
                anchors.fill: parent
                anchors.margins: Theme.space3
                contentWidth: editor.contentWidth
                contentHeight: editor.contentHeight
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                TextEdit {
                    id: editor
                    width: flick.width
                    text: root.engine && root.engine.examples.length > 0 ? root.engine.examples[0].code : ""
                    color: Theme.textPrimary
                    selectionColor: Theme.accentSubtle
                    selectedTextColor: Theme.textPrimary
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeSm
                    wrapMode: TextEdit.NoWrap
                    tabStopDistance: 28
                    selectByMouse: true
                    Keys.onPressed: (event) => {
                        if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter)
                                && (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier))) {
                            root.runScript()
                            event.accepted = true
                        }
                    }
                }
            }
        }

        // Rezultatul.
        Rectangle {
            id: outputFrame
            anchors.left: parent.left
            anchors.right: sidebar.left
            anchors.rightMargin: Theme.space3
            anchors.bottom: parent.bottom
            height: 110
            radius: Theme.radiusMd
            color: root.failed ? Qt.rgba(0.9, 0.2, 0.2, 0.10) : Theme.bgHover
            Flickable {
                anchors.fill: parent
                anchors.margins: Theme.space3
                contentHeight: outputText.implicitHeight
                clip: true
                Text {
                    id: outputText
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: root.output.length > 0 ? root.output : qsTrId("ScriptConsole.rezultatul_apare_aici_aion_log_print")
                    color: root.failed ? Theme.danger : root.output.length > 0 ? Theme.textPrimary : Theme.textTertiary
                    font.family: Theme.fontMono
                    font.pixelSize: Theme.fontSizeXs
                }
            }
        }

        // API-ul pe scurt și plugin-urile.
        Column {
            id: sidebar
            anchors.top: header.bottom
            anchors.topMargin: Theme.space3
            anchors.right: parent.right
            width: 260
            spacing: Theme.space2
            Text {
                text: qsTrId("ScriptConsole.api_aion")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: qsTrId("ScriptConsole.box_x_y_z_l_a_h_nume_ncylinder_x_y_z_d_h_cone_")
                color: Theme.textTertiary
                font.family: Theme.fontMono
                font.pixelSize: 11
                lineHeight: 1.15
            }
            Separator { width: parent.width }
            Row {
                width: parent.width
                Text {
                    width: parent.width - 48
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTrId("ScriptConsole.plugin_uri")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.DemiBold
                }
                IconButton {
                    width: 24; height: 24; iconSize: 13
                    iconName: "rotate-ccw"
                    tooltip: qsTrId("ScriptConsole.reincarca_plugin_urile")
                    onClicked: root.engine.reloadPlugins()
                }
                IconButton {
                    width: 24; height: 24; iconSize: 13
                    iconName: "folder"
                    tooltip: qsTrId("ScriptConsole.deschide_dosarul_plugin_urilor")
                    onClicked: Qt.openUrlExternally("file://" + root.engine.pluginFolder())
                }
            }
            Repeater {
                model: root.engine ? root.engine.plugins : []
                delegate: Column {
                    id: plugin
                    required property var modelData
                    width: sidebar.width
                    spacing: 2
                    Text {
                        text: plugin.modelData.name + (plugin.modelData.version ? " " + plugin.modelData.version : "")
                        color: Theme.textPrimary
                        font.pixelSize: Theme.fontSizeSm
                    }
                    Repeater {
                        model: plugin.modelData.commands
                        delegate: Text {
                            required property var modelData
                            text: "▸ " + modelData.title
                            color: Theme.accent
                            font.pixelSize: Theme.fontSizeXs
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    const r = root.engine.runPluginCommand(modelData.id)
                                    root.failed = !r.ok
                                    root.output = r.ok ? (r.output || qsTrId("ScriptConsole.gata")) : qsTrId("ScriptConsole.eroare_2") + r.error
                                }
                            }
                        }
                    }
                }
            }
            Text {
                visible: !root.engine || root.engine.plugins.length === 0
                width: parent.width
                wrapMode: Text.Wrap
                text: qsTrId("ScriptConsole.un_plugin_un_dosar_cu_manifest_json_nume_comen")
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }
    }
}
