import QtQuick
import Aion
import Aion.Render
import QtQuick.Shapes

Window {
    id: window

    width: 1440
    height: 900
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: "Aion"
    color: Theme.bgApp
    // Conținutul continuă sub bara de titlu (macOS: butoanele native rămân, fără fundal propriu).
    flags: Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint

    property bool leftPanelVisible: true
    property bool rightPanelVisible: true
    property string activeTool: "select"
    readonly property var navigationTools: ["orbit", "pan", "zoom"]

    // Toate comenzile aplicației; apar în command palette și au scurtături.
    readonly property var commands: {
        const list = [
            { id: "view.iso",    title: "Vedere izometrică", category: "Vedere", shortcut: "Ctrl+0", icon: "box" },
            { id: "view.top",    title: "Vedere de sus",     category: "Vedere", shortcut: "Ctrl+1", icon: "arrow-down" },
            { id: "view.front",  title: "Vedere din față",   category: "Vedere", shortcut: "Ctrl+2", icon: "arrow-up" },
            { id: "view.right",  title: "Vedere din dreapta", category: "Vedere", shortcut: "Ctrl+3", icon: "arrow-left" },
            { id: "view.back",   title: "Vedere din spate",  category: "Vedere", shortcut: "Ctrl+4", icon: "arrow-down" },
            { id: "view.left",   title: "Vedere din stânga", category: "Vedere", shortcut: "Ctrl+5", icon: "arrow-right" },
            { id: "view.bottom", title: "Vedere de jos",     category: "Vedere", shortcut: "Ctrl+6", icon: "arrow-up" },
            { id: "view.extents", title: "Zoom Extents",     category: "Vedere", shortcut: "Shift+Z", icon: "scan" },
            { id: "view.projection", title: viewport.orthographic ? "Proiecție în perspectivă" : "Proiecție paralelă (ortografică)",
              category: "Vedere", shortcut: "", icon: "focus" },
            { id: "view.grid",   title: viewport.gridVisible ? "Ascunde grid-ul" : "Arată grid-ul", category: "Vedere", shortcut: "", icon: "grid-3x3" },
            { id: "view.demo",   title: viewport.demoVisible ? "Ascunde cutia demo" : "Arată cutia demo", category: "Vedere", shortcut: "", icon: "box" },
            { id: "edit.undo", title: doc.canUndo ? "Undo " + doc.undoName : "Undo", category: "Editare",
              shortcut: "Ctrl+Z", icon: "undo-2" },
            { id: "edit.redo", title: doc.canRedo ? "Redo " + doc.redoName : "Redo", category: "Editare",
              shortcut: "Ctrl+Shift+Z", icon: "redo-2" },
            { id: "edit.delete", title: "Șterge selecția", category: "Editare", shortcut: "Delete", icon: "trash-2" },
            { id: "edit.selectAll", title: "Selectează tot", category: "Editare", shortcut: "Ctrl+A",
              icon: "square-dashed-mouse-pointer" },
            { id: "edit.selectNone", title: "Deselectează tot", category: "Editare", shortcut: "Ctrl+Shift+A", icon: "x" },
            { id: "ui.leftPanel",  title: "Comută panoul din stânga", category: "Interfață", shortcut: "", icon: "panel-left" },
            { id: "ui.rightPanel", title: "Comută panoul din dreapta", category: "Interfață", shortcut: "", icon: "panel-right" },
            { id: "ui.panels",   title: "Mod focus (ascunde panourile)", category: "Interfață", shortcut: "Ctrl+\\", icon: "focus" },
            { id: "ui.theme",    title: "Comută tema (Sistem / Light / Dark)", category: "Interfață", shortcut: "", icon: "sun" }
        ]
        // Toate uneltele din toolbar, cu scurtăturile lor.
        for (const t of toolbar.allTools)
            list.push({ id: "tool." + t.id, title: t.name, category: "Unelte", shortcut: t.key || "", icon: t.icon })
        if (AppInfo.debugBuild)
            list.push({ id: "debug.storybook", title: "Storybook: toate componentele", category: "Dezvoltare", shortcut: "", icon: "palette" },
                      { id: "debug.benchmark", title: "Măsoară fps (rotește camera 4 s)", category: "Dezvoltare", shortcut: "", icon: "orbit" })
        return list
    }

    function runCommand(id) {
        switch (id) {
        case "view.iso": viewport.setStandardView(Viewport.Iso); break
        case "view.top": viewport.setStandardView(Viewport.Top); break
        case "view.front": viewport.setStandardView(Viewport.Front); break
        case "view.right": viewport.setStandardView(Viewport.Right); break
        case "view.back": viewport.setStandardView(Viewport.Back); break
        case "view.left": viewport.setStandardView(Viewport.Left); break
        case "view.bottom": viewport.setStandardView(Viewport.Bottom); break
        case "view.extents": viewport.zoomExtents(); break
        case "view.projection": viewport.toggleProjection(); break
        case "view.grid": viewport.gridVisible = !viewport.gridVisible; break
        case "view.demo": viewport.demoVisible = !viewport.demoVisible; break
        case "edit.undo": doc.undo(); break
        case "edit.redo": doc.redo(); break
        case "edit.delete": doc.deleteSelection(); break
        case "edit.selectAll": doc.selectAll(); break
        case "edit.selectNone": doc.clearSelection(); break
        case "ui.leftPanel": window.leftPanelVisible = !window.leftPanelVisible; break
        case "ui.rightPanel": window.rightPanelVisible = !window.rightPanelVisible; break
        case "ui.panels": {
            const show = !(window.leftPanelVisible || window.rightPanelVisible)
            window.leftPanelVisible = show
            window.rightPanelVisible = show
            break
        }
        case "ui.theme": Theme.cycleMode(); break
        case "debug.storybook": storybookLoader.active = true; break
        case "debug.benchmark": viewport.startTurntable(4); break
        default:
            if (id.startsWith("tool."))
                window.selectTool(id.slice(5))
            else
                console.warn("Comandă necunoscută:", id)
        }
    }

    // Comenzile din AION_STARTUP, rulate pe rând: „palette:text” deschide command palette,
    // „wait:ms” așteaptă, „clickw:x,y,z” dă click în punctul 3D dat, „movew:x,y,z” mută doar cursorul,
    // „type:text” tastează în Measurements box, „key:escape|enter|left|right|up|down” apasă tasta.
    property int startupIndex: 0
    Timer {
        id: startupTimer
        running: AppInfo.startupActions.length > 0
        interval: 300
        onTriggered: {
            const actions = AppInfo.startupActions
            while (window.startupIndex < actions.length) {
                const action = actions[window.startupIndex++]
                if (action.startsWith("wait:")) {
                    startupTimer.interval = parseInt(action.slice(5))
                    startupTimer.start()
                    return
                }
                if (action.startsWith("palette:")) {
                    palette.openWithQuery(action.slice(8))
                } else if (action.startsWith("clickw:")) {
                    const c = action.slice(7).split(",").map(Number)
                    AppInfo.simulateClick(viewport, viewport.projectToScreen(Qt.vector3d(c[0], c[1], c[2])))
                } else if (action.startsWith("movew:")) {
                    const m = action.slice(6).split(",").map(Number)
                    AppInfo.simulateMove(viewport, viewport.projectToScreen(Qt.vector3d(m[0], m[1], m[2])))
                } else if (action.startsWith("type:")) {
                    window.vcbText = action.slice(5)
                } else if (action === "key:escape") {
                    window.escapePressed()
                } else if (action === "key:enter") {
                    window.enterPressed()
                } else if (["key:left", "key:right", "key:up", "key:down"].includes(action)) {
                    doc.arrowKey(action.slice(4))
                } else {
                    window.runCommand(action)
                }
            }
        }
    }

    // Uneltele de modelare implementate până acum (celelalte apar în toolbar, dar vin în fazele următoare).
    readonly property var modelingTools: ["select", "line", "rectangle", "pushpull", "move", "rotate", "scale", "eraser"]
    property string pendingToolMessage: ""

    function selectTool(id) {
        window.activeTool = id
        const tool = toolbar.toolInfo(id)
        const ready = modelingTools.includes(id) || navigationTools.includes(id)
        pendingToolMessage = tool && !ready ? "Unealta „" + tool.name + "” vine în Faza " + tool.phase : ""
    }

    Document {
        id: doc
        activeTool: window.modelingTools.includes(window.activeTool) ? window.activeTool : ""
        onActiveToolChanged: window.vcbText = ""
    }

    // Measurements box: ce a tastat utilizatorul (gol = arată valoarea curentă a uneltei).
    property string vcbText: ""

    // Tastele care ajung în Measurements box: cifre, separatori și (după prima cifră) unitățile.
    function vcbKey(event) {
        if (!doc.hasMeasurement || palette.opened)
            return
        if (event.modifiers & (Qt.ControlModifier | Qt.MetaModifier))
            return
        if (event.key === Qt.Key_Backspace) {
            if (vcbText.length > 0) {
                vcbText = vcbText.slice(0, -1)
                event.accepted = true
            }
            return
        }
        const t = event.text
        if (t.length !== 1)
            return
        // „x3” / „/3” (array după copiere) încep cu x, * sau /.
        const allowed = vcbText.length > 0 ? /[0-9.,;'"+\-*\/ a-zA-Z]/ : /[0-9.,;'"+\-*\/xX]/
        if (allowed.test(t)) {
            vcbText += t
            event.accepted = true
        }
    }

    function escapePressed() {
        if (vcbText.length > 0)
            vcbText = ""
        else
            doc.escape()
    }

    function enterPressed() {
        if (vcbText.length === 0) {
            doc.enter()
        } else if (doc.applyMeasurement(vcbText)) {
            vcbText = ""
        } else {
            measurementsBox.flashInvalid()
        }
    }

    // Scurtăturile comenzilor, generate din aceeași listă ca și command palette.
    Instantiator {
        model: window.commands
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.shortcut
            // Cât timp se tastează în Measurements box, literele sunt unități („m”, „cm”), nu unelte.
            enabled: modelData.shortcut.length > 0 && !palette.opened
                     && !(window.vcbText.length > 0 && modelData.shortcut.length === 1)
            onActivated: window.runCommand(modelData.id)
        }
    }
    Shortcut {
        sequence: "Ctrl+K"
        onActivated: palette.opened ? palette.close() : palette.open()
    }
    // Tastele care nu sunt comenzi: Esc (anulează / deselectează), Enter (termină sau aplică valoarea tastată),
    // Backspace (șterge selecția, pe Mac; în timpul tastării, ultimul caracter).
    Shortcut {
        sequence: "Escape"
        enabled: !palette.opened
        onActivated: window.escapePressed()
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: !palette.opened
        onActivated: window.enterPressed()
    }
    Shortcut {
        sequence: "Backspace"
        enabled: !palette.opened && window.vcbText.length === 0
        onActivated: doc.deleteSelection()
    }
    // Săgețile blochează inferența pe o axă: → roșie, ← verde, ↑ albastră, ↓ paralel cu muchia atinsă.
    Instantiator {
        model: ["left", "right", "up", "down"]
        delegate: Shortcut {
            required property string modelData
            sequence: modelData.charAt(0).toUpperCase() + modelData.slice(1)
            enabled: doc.hasMeasurement && !palette.opened
            onActivated: doc.arrowKey(modelData)
        }
    }

    TitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 2
        leftPanelVisible: window.leftPanelVisible
        rightPanelVisible: window.rightPanelVisible
        onCommandPaletteRequested: palette.open()
        onToggleLeftPanel: window.leftPanelVisible = !window.leftPanelVisible
        onToggleRightPanel: window.rightPanelVisible = !window.rightPanelVisible
    }

    Panel {
        id: leftPanel
        anchors.top: titleBar.bottom
        anchors.bottom: statusBar.top
        anchors.left: parent.left
        width: window.leftPanelVisible ? Theme.sidePanelWidth : 0
        visible: width > 0
        clip: true

        Behavior on width { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }

        header: Tabs {
            id: leftTabs
            anchors.fill: parent
            model: ["Outliner", "Tag-uri"]
        }

        Column {
            width: parent.width
            spacing: Theme.space2
            Text {
                text: leftTabs.currentIndex === 0 ? (doc.faceCount + doc.edgeCount > 0 ? "Geometrie liberă" : "Modelul e gol") : "Niciun tag"
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMd
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: leftTabs.currentIndex === 0
                      ? "Grupurile și componentele vor apărea aici (Faza 6)."
                      : "Tag-urile (straturile) vor apărea aici (Faza 6)."
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeSm
            }
        }
    }

    Separator {
        anchors.top: leftPanel.top
        anchors.bottom: leftPanel.bottom
        anchors.left: leftPanel.right
        visible: leftPanel.visible
    }

    Viewport {
        id: viewport
        onFramesPerSecondChanged: if (AppInfo.startupActions.length > 0) console.info("fps:", framesPerSecond)
        anchors.top: titleBar.bottom
        anchors.bottom: statusBar.top
        anchors.left: leftPanel.right
        anchors.right: rightPanel.left

        backgroundTop: Theme.viewportTop
        backgroundBottom: Theme.viewportBottom
        gridMinorColor: Theme.gridMinor
        gridMajorColor: Theme.gridMajor
        axisXColor: Theme.axisX
        axisYColor: Theme.axisY
        axisZColor: Theme.axisZ
        edgeColor: Theme.edge
        freeEdgeColor: Theme.edgeFree
        faceFrontColor: Theme.faceFront
        faceBackColor: Theme.faceBack
        accentColor: Theme.accent
        previewColor: Theme.textPrimary
        eraseColor: Theme.danger
        inferenceCyanColor: Theme.inferenceCyan
        inferenceMagentaColor: Theme.inferenceMagenta
        document: doc
        focus: true
        Keys.onPressed: (event) => window.vcbKey(event)
        reducedMotion: Theme.reduceMotion
        navigationTool: window.navigationTools.includes(window.activeTool) ? window.activeTool : ""

        // Dreptunghiul de selecție: linie continuă = window (stânga → dreapta), punctată = crossing.
        Shape {
            visible: viewport.selectionRect.width > 0 || viewport.selectionRect.height > 0
            x: viewport.selectionRect.x
            y: viewport.selectionRect.y
            width: viewport.selectionRect.width
            height: viewport.selectionRect.height
            ShapePath {
                strokeColor: Theme.accent
                strokeWidth: 1
                fillColor: Theme.accentSubtle
                strokeStyle: viewport.selectionCrossing ? ShapePath.DashLine : ShapePath.SolidLine
                dashPattern: [4, 3]
                startX: 0; startY: 0
                PathLine { x: viewport.selectionRect.width; y: 0 }
                PathLine { x: viewport.selectionRect.width; y: viewport.selectionRect.height }
                PathLine { x: 0; y: viewport.selectionRect.height }
                PathLine { x: 0; y: 0 }
            }
        }

        ViewportAnnotations {
            anchors.fill: parent
            annotations: viewport.annotations
        }

        MeasurementsBox {
            id: measurementsBox
            readonly property real offset: 18
            visible: doc.hasMeasurement && (window.vcbText.length > 0
                     || (viewport.pointerInside && doc.measurementValue.length > 0))
            // Lângă cursor, dar întotdeauna în interiorul viewport-ului.
            x: Math.min(viewport.pointerPosition.x + offset, viewport.width - width - Theme.space2)
            y: Math.min(viewport.pointerPosition.y + offset, viewport.height - height - Theme.space2)
            label: doc.measurementLabel
            value: doc.measurementValue
            typed: window.vcbText
        }

        NavigationGizmo {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.margins: Theme.space2
            cameraRight: viewport.cameraRight
            cameraUp: viewport.cameraUp
            cameraForward: viewport.cameraForward
            onViewRequested: (view) => viewport.setStandardView(Viewport[view])
        }

        Toolbar {
            id: toolbar
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Theme.space6
            activeTool: window.activeTool
            onToolSelected: (id) => window.selectTool(id)
        }
    }

    Panel {
        id: rightPanel
        anchors.top: titleBar.bottom
        anchors.bottom: statusBar.top
        anchors.right: parent.right
        width: window.rightPanelVisible ? Theme.sidePanelWidth : 0
        visible: width > 0
        clip: true
        title: "Inspector"

        Behavior on width { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }

        Column {
            width: parent.width
            spacing: Theme.space6

            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: doc.selectionSummary.length > 0 ? doc.selectionSummary : "Nimic selectat"
                color: doc.selectionSummary.length > 0 ? Theme.textPrimary : Theme.textTertiary
                font.pixelSize: Theme.fontSizeMd
            }

            Section {
                width: parent.width
                title: "Model"
                Text {
                    text: (doc.faceCount === 1 ? "1 față" : doc.faceCount + " fețe") + " · "
                          + (doc.edgeCount === 1 ? "1 muchie" : doc.edgeCount + " muchii")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSm
                    font.family: Theme.fontMono
                }
            }

            Section {
                width: parent.width
                title: "Vizualizare"

                Tabs {
                    width: parent.width
                    model: ["Perspectivă", "Paralelă"]
                    currentIndex: viewport.orthographic ? 1 : 0
                    onCurrentIndexChanged: viewport.orthographic = currentIndex === 1
                }
                Toggle {
                    width: parent.width
                    text: "Grid"
                    checked: viewport.gridVisible
                    onToggled: viewport.gridVisible = checked
                }
            }

            Section {
                width: parent.width
                title: "Cameră"

                NumberField {
                    width: parent.width
                    label: "Dist."
                    unit: "mm"
                    readOnly: true
                    value: viewport.cameraDistance
                }
                Button {
                    width: parent.width
                    text: "Zoom Extents"
                    iconName: "scan"
                    onClicked: viewport.zoomExtents()
                }
            }
        }
    }

    Separator {
        anchors.top: rightPanel.top
        anchors.bottom: rightPanel.bottom
        anchors.right: rightPanel.left
        visible: rightPanel.visible
    }

    StatusBar {
        id: statusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        framesPerSecond: viewport.framesPerSecond
        message: window.pendingToolMessage.length > 0 ? window.pendingToolMessage
               : doc.toolStatus.length > 0 ? doc.toolStatus : "Gata"
        selectionText: doc.selectionSummary
        orthographic: viewport.orthographic
    }

    CommandPalette {
        id: palette
        parent: window.contentItem
        commands: window.commands
        onCommandTriggered: (id) => window.runCommand(id)
    }

    Loader {
        id: storybookLoader
        anchors.fill: parent
        z: 100
        active: AppInfo.storybookOnStart
        sourceComponent: Storybook {
            onCloseRequested: storybookLoader.active = false
        }
    }
}
