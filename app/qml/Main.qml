import QtQuick
import Aion
import Aion.Render

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
            { id: "tool.orbit",  title: "Orbit",  category: "Unelte", shortcut: "O", icon: "orbit" },
            { id: "tool.pan",    title: "Pan",    category: "Unelte", shortcut: "H", icon: "hand" },
            { id: "tool.zoom",   title: "Zoom",   category: "Unelte", shortcut: "Z", icon: "zoom-in" },
            { id: "tool.select", title: "Select", category: "Unelte", shortcut: "Space", icon: "mouse-pointer-2" },
            { id: "ui.leftPanel",  title: "Comută panoul din stânga", category: "Interfață", shortcut: "", icon: "panel-left" },
            { id: "ui.rightPanel", title: "Comută panoul din dreapta", category: "Interfață", shortcut: "", icon: "panel-right" },
            { id: "ui.panels",   title: "Mod focus (ascunde panourile)", category: "Interfață", shortcut: "Ctrl+\\", icon: "focus" },
            { id: "ui.theme",    title: "Comută tema (Sistem / Light / Dark)", category: "Interfață", shortcut: "", icon: "sun" }
        ]
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
        case "tool.orbit": window.activeTool = "orbit"; break
        case "tool.pan": window.activeTool = "pan"; break
        case "tool.zoom": window.activeTool = "zoom"; break
        case "tool.select": window.activeTool = "select"; break
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
        default: console.warn("Comandă necunoscută:", id)
        }
    }

    // Comenzile din AION_STARTUP; „palette:text” deschide command palette cu textul dat.
    Timer {
        running: AppInfo.startupActions.length > 0
        interval: 300
        onTriggered: {
            for (const action of AppInfo.startupActions) {
                if (action.startsWith("palette:"))
                    palette.openWithQuery(action.slice(8))
                else
                    window.runCommand(action)
            }
        }
    }

    function selectTool(id) {
        window.activeTool = id
        if (id !== "select" && !navigationTools.includes(id))
            statusBar.message = "Unealta „" + id + "” vine în Faza 3"
        else
            statusBar.message = "Gata"
    }

    // Scurtăturile comenzilor, generate din aceeași listă ca și command palette.
    Instantiator {
        model: window.commands
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.shortcut
            enabled: modelData.shortcut.length > 0 && !palette.opened
            onActivated: window.runCommand(modelData.id)
        }
    }
    Shortcut {
        sequence: "Ctrl+K"
        onActivated: palette.opened ? palette.close() : palette.open()
    }
    // Uneltele de desen (doar vizual, până în Faza 3).
    Instantiator {
        model: toolbar.tools.filter(t => t.key && t.key.length === 1 && !navigationTools.includes(t.id))
        delegate: Shortcut {
            required property var modelData
            sequence: modelData.key
            enabled: !palette.opened
            onActivated: window.selectTool(modelData.id)
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
                text: leftTabs.currentIndex === 0 ? "Modelul e gol" : "Niciun tag"
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
        faceFrontColor: Theme.faceFront
        faceBackColor: Theme.faceBack
        reducedMotion: Theme.reduceMotion
        navigationTool: window.navigationTools.includes(window.activeTool) ? window.activeTool : ""

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
                text: "Nimic selectat"
                color: Theme.textTertiary
                font.pixelSize: Theme.fontSizeMd
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
                Toggle {
                    width: parent.width
                    text: "Cutie demo"
                    checked: viewport.demoVisible
                    onToggled: viewport.demoVisible = checked
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
