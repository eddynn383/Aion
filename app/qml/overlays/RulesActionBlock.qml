import QtQuick
import Aion

// Un bloc de acțiune (puzzle): culoarea categoriei, textul cu parametrii în linie, „ținta”, și — pentru
// „Alternează” — două liste de blocuri înăuntru. Se poate trage (mutare / ștergere pe paletă).
Item {
    id: root

    property var editor // RulesEditor
    property var path: [] // calea acțiunii în regulile editorului
    property var action: ({})

    readonly property var info: editor.blockInfo(action.kind)
    readonly property bool container: action.kind === "alternate"

    width: parent ? parent.width : 300
    height: body.height + 6

    // Corpul blocului, cu „dintele” de jos și „scobitura” de sus (aspect de puzzle).
    Rectangle {
        id: body
        width: parent.width
        height: content.implicitHeight + Theme.space2 * 2
        radius: Theme.radiusMd
        color: root.info.color
        opacity: dragHandler.active ? 0.5 : 1
        Rectangle { x: 18; y: -1; width: 22; height: 5; radius: 2; color: root.editor.canvasColor }
        Rectangle { x: 18; y: parent.height - 1; width: 22; height: 6; radius: 2; color: root.info.color }

        Column {
            id: content
            x: Theme.space3
            y: Theme.space2
            width: parent.width - Theme.space3 * 2
            spacing: Theme.space2

            Flow {
                width: parent.width
                spacing: Theme.space1 + 2

                Text {
                    text: root.info.label
                    color: "white"
                    font.pixelSize: Theme.fontSizeSm
                    font.weight: Font.DemiBold
                    height: 24
                    verticalAlignment: Text.AlignVCenter
                }
                // Ținta (pentru acțiunile asupra unui obiect).
                Loader {
                    active: root.info.target === true
                    sourceComponent: root.editor.pickComponent
                    onLoaded: {
                        item.options = Qt.binding(() => root.editor.targetOptions)
                        item.value = Qt.binding(() => root.action.target ?? 0)
                        item.picked.connect((v) => root.editor.setField(root.path, "target", v))
                    }
                }
                // Parametrii numerici.
                Repeater {
                    model: root.info.fields ?? []
                    delegate: Row {
                        required property var modelData
                        spacing: 3
                        Text {
                            text: modelData.label
                            color: "white"
                            opacity: 0.85
                            font.pixelSize: Theme.fontSizeXs
                            height: 24
                            verticalAlignment: Text.AlignVCenter
                        }
                        Rectangle {
                            width: Math.max(44, valueInput.implicitWidth + 12)
                            height: 24
                            radius: 12
                            color: Qt.rgba(1, 1, 1, 0.92)
                            TextInput {
                                id: valueInput
                                anchors.centerIn: parent
                                text: String(root.action[modelData.field] ?? 0)
                                color: "#111111"
                                font.pixelSize: Theme.fontSizeXs
                                font.family: Theme.fontMono
                                selectByMouse: true
                                validator: DoubleValidator {}
                                onEditingFinished: root.editor.setField(root.path, modelData.field, parseFloat(text) || 0)
                            }
                        }
                        Text {
                            visible: (modelData.unit ?? "").length > 0
                            text: modelData.unit ?? ""
                            color: "white"
                            opacity: 0.85
                            font.pixelSize: Theme.fontSizeXs
                            height: 24
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
                // Alegerea dintr-o listă (material, animație, scenă).
                Loader {
                    active: (root.info.pick ?? "").length > 0
                    sourceComponent: root.editor.pickComponent
                    onLoaded: {
                        const field = root.info.pick
                        item.options = Qt.binding(() => root.editor.optionsFor(field))
                        item.value = Qt.binding(() => root.action[field] ?? 0)
                        item.picked.connect((v) => root.editor.setField(root.path, field, v))
                    }
                }
                // „Pornește animația”: sensul, repetarea și, pentru „de mai multe ori”, numărul de ture.
                Loader {
                    active: root.action.kind === "playAnimation"
                    sourceComponent: root.editor.pickComponent
                    onLoaded: {
                        item.options = root.editor.directionOptions
                        item.value = Qt.binding(() => root.action.direction ?? "forward")
                        item.picked.connect((v) => root.editor.setField(root.path, "direction", v))
                    }
                }
                Loader {
                    active: root.action.kind === "playAnimation"
                    sourceComponent: root.editor.pickComponent
                    onLoaded: {
                        item.options = root.editor.repeatOptions
                        item.value = Qt.binding(() => root.editor.repeatMode(root.action.repeat ?? 1))
                        item.picked.connect((v) => root.editor.setRepeatMode(root.path, v))
                    }
                }
                Row {
                    visible: root.action.kind === "playAnimation" && (root.action.repeat ?? 1) >= 2
                    spacing: 3
                    Rectangle {
                        width: Math.max(36, repeatInput.implicitWidth + 12)
                        height: 24
                        radius: 12
                        color: Qt.rgba(1, 1, 1, 0.92)
                        TextInput {
                            id: repeatInput
                            anchors.centerIn: parent
                            text: String(root.action.repeat ?? 2)
                            color: "#111111"
                            font.pixelSize: Theme.fontSizeXs
                            font.family: Theme.fontMono
                            selectByMouse: true
                            validator: IntValidator { bottom: 1; top: 999 }
                            onEditingFinished: root.editor.setField(root.path, "repeat", Math.max(1, parseInt(text) || 1))
                        }
                    }
                    Text {
                        text: "ori"
                        color: "white"
                        opacity: 0.85
                        font.pixelSize: Theme.fontSizeXs
                        height: 24
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // „Alternează”: două liste.
            Repeater {
                model: root.container ? [{ key: "first", label: qsTrId("RulesActionBlock.intai") }, { key: "second", label: "apoi:" }] : []
                delegate: Column {
                    required property var modelData
                    width: content.width
                    spacing: 2
                    Text {
                        text: modelData.label
                        color: "white"
                        font.pixelSize: Theme.fontSizeXs
                        font.weight: Font.DemiBold
                    }
                    Rectangle {
                        width: parent.width
                        height: innerList.height + Theme.space2
                        radius: Theme.radiusSm
                        color: Qt.rgba(0, 0, 0, 0.18)
                        Loader {
                            id: innerList
                            x: Theme.space1
                            y: Theme.space1
                            width: parent.width - Theme.space2
                            source: Qt.resolvedUrl("RulesActionList.qml")
                            onLoaded: {
                                item.editor = root.editor
                                item.listPath = Qt.binding(() => root.path.concat([modelData.key]))
                                item.actions = Qt.binding(() => root.action[modelData.key] ?? [])
                            }
                        }
                    }
                }
            }
        }
    }

    // Tragerea blocului: îl muți în altă poziție sau îl ștergi pe paletă (fantoma editorului poartă drag-ul).
    DragHandler {
        id: dragHandler
        target: null
        onActiveChanged: active ? root.editor.beginDrag({ type: "move", path: root.path }, root.info.label, root.info.color)
                                : root.editor.endDrag()
        onCentroidChanged: if (active) root.editor.moveDrag(root, centroid.position)
    }
}
