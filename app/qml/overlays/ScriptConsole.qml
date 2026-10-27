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
        const r = root.engine.run(editor.text, "Script")
        root.failed = !r.ok
        root.output = r.ok ? (r.output.length > 0 ? r.output : "Gata (un pas de undo: ⌘Z îl anulează).")
                           : "Eroare" + (r.line > 0 ? " pe linia " + r.line : "") + ": " + r.error
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
                text: "Scripturi"
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
                text: "Rulează  ⌘↵"
                onClicked: root.runScript()
            }
            IconButton {
                iconName: "x"
                tooltip: "Închide"
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
                    text: root.output.length > 0 ? root.output : "Rezultatul apare aici (aion.log(…), print(…))."
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
                text: "API (aion.…)"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeSm
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                textFormat: Text.PlainText
                text: "box(x, y, z, l, a, h, nume)\ncylinder(x, y, z, d, h)  cone(…)  sphere(x, y, z, d)\n"
                      + "object(\"door\", { width: 900 }, x, y, z)\nmove(id, dx, dy, dz)\nrotate(id, °, \"z\", [pivot])\n"
                      + "place(id, { x, rz, width… })  remove(id)\nmaterial(id, \"Lemn de stejar\" | \"#c0392b\")\n"
                      + "line([x,y,z], [x,y,z])  face([[…], …])\nobjects()  selection()  select([id…])\n"
                      + "command(\"view.top\")  message(text)  log(…)\n\nmm, în contextul editat."
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
                    text: "Plugin-uri"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.DemiBold
                }
                IconButton {
                    width: 24; height: 24; iconSize: 13
                    iconName: "rotate-ccw"
                    tooltip: "Reîncarcă plugin-urile"
                    onClicked: root.engine.reloadPlugins()
                }
                IconButton {
                    width: 24; height: 24; iconSize: 13
                    iconName: "folder"
                    tooltip: "Deschide dosarul plugin-urilor"
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
                                    root.output = r.ok ? (r.output || "Gata.") : "Eroare: " + r.error
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
                text: "Un plugin = un dosar cu manifest.json (nume, comenzi: { title, function }) și main.js. "
                      + "Comenzile apar și în palette (⌘K). Detalii în docs/scripting.md."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeXs
            }
        }
    }
}
