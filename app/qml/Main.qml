import QtQuick
import Aion
import Aion.Render
import QtQuick.Shapes
import QtQuick.Dialogs
import QtCore

Window {
    id: window

    width: 1440
    height: 900
    minimumWidth: 960
    minimumHeight: 600
    visible: true
    title: doc.fileName + (doc.modified ? " — modificat" : "") + " · Aion"
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
            { id: "file.new", title: "Document nou", category: "Fișier", shortcut: "Ctrl+N", icon: "plus" },
            { id: "file.open", title: "Deschide…", category: "Fișier", shortcut: "Ctrl+O", icon: "search" },
            { id: "file.save", title: "Salvează", category: "Fișier", shortcut: "Ctrl+S", icon: "check" },
            { id: "file.saveAs", title: "Salvează ca…", category: "Fișier", shortcut: "Ctrl+Shift+S", icon: "check" },
            { id: "file.import", title: "Importă (OBJ, glTF, GLB)…", category: "Fișier", shortcut: "", icon: "arrow-down" },
            { id: "file.exportObj", title: "Exportă OBJ…", category: "Fișier", shortcut: "", icon: "arrow-up" },
            { id: "file.exportGlb", title: "Exportă glTF binar (GLB)…", category: "Fișier", shortcut: "", icon: "arrow-up" },
            { id: "file.exportGltf", title: "Exportă glTF…", category: "Fișier", shortcut: "", icon: "arrow-up" },
            { id: "file.exportPng", title: "Exportă imaginea (PNG)…", category: "Fișier", shortcut: "", icon: "monitor" },
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
            { id: "org.makeGroup", title: "Creează grup", category: "Organizare", shortcut: "Ctrl+G", icon: "box" },
            { id: "org.makeComponent", title: "Creează obiect", category: "Organizare", shortcut: "Ctrl+Shift+G", icon: "component" },
            { id: "org.explode", title: "Desface (grup sau obiect)", category: "Organizare", shortcut: "", icon: "box" },
            { id: "org.makeUnique", title: "Fă unic (separă de copii)", category: "Organizare", shortcut: "", icon: "component" },
            { id: "org.enter", title: "Editează grupul sau obiectul selectat", category: "Organizare", shortcut: "", icon: "box" },
            { id: "org.exit", title: "Ieși din grupul sau obiectul editat", category: "Organizare", shortcut: "", icon: "arrow-left" },
            { id: "org.hide", title: "Ascunde selecția", category: "Organizare", shortcut: "", icon: "eye-off" },
            { id: "org.unhideAll", title: "Arată tot", category: "Organizare", shortcut: "", icon: "eye" },
            { id: "org.lock", title: "Blochează selecția", category: "Organizare", shortcut: "", icon: "lock" },
            { id: "org.unlock", title: "Deblochează selecția", category: "Organizare", shortcut: "", icon: "lock" },
            { id: "edit.selectConnected", title: "Selectează ce e conectat", category: "Editare", shortcut: "", icon: "square-dashed-mouse-pointer" },
            { id: "edit.reverseFaces", title: "Inversează fețele selectate", category: "Editare", shortcut: "", icon: "rotate-ccw" },
            { id: "view.zoomSelection", title: "Zoom la selecție", category: "Vedere", shortcut: "", icon: "scan" },
            { id: "edit.intersectModel", title: "Intersectează fețele selectate cu modelul", category: "Editare", shortcut: "", icon: "scan" },
            { id: "edit.intersectSelection", title: "Intersectează fețele selectate între ele", category: "Editare", shortcut: "", icon: "scan" },
            { id: "solid.union", title: "Solide: reuniune", category: "Solide", shortcut: "", icon: "box" },
            { id: "solid.subtract", title: "Solide: scade al doilea din primul", category: "Solide", shortcut: "", icon: "box" },
            { id: "solid.intersect", title: "Solide: intersecție", category: "Solide", shortcut: "", icon: "box" },
            { id: "guides.clear", title: "Șterge ghidajele (" + doc.guideCount + ")", category: "Editare", shortcut: "", icon: "trash-2" },
            { id: "style.shaded", title: "Stil: materiale", category: "Stil", shortcut: "", icon: "palette" },
            { id: "style.clay", title: "Stil: clay (alb mat)", category: "Stil", shortcut: "", icon: "palette" },
            { id: "style.hiddenline", title: "Stil: linii ascunse", category: "Stil", shortcut: "", icon: "palette" },
            { id: "style.wireframe", title: "Stil: wireframe", category: "Stil", shortcut: "", icon: "palette" },
            { id: "style.xray", title: "Stil: X-Ray", category: "Stil", shortcut: "", icon: "palette" },
            { id: "style.shadows", title: viewport.shadows ? "Ascunde umbrele" : "Arată umbrele", category: "Stil", shortcut: "", icon: "sun-medium" },
            { id: "style.profiles", title: viewport.profiles ? "Fără profile" : "Profile (contururi)", category: "Stil", shortcut: "", icon: "palette" },
            { id: "style.fog", title: viewport.fog ? "Fără ceață" : "Ceață", category: "Stil", shortcut: "", icon: "palette" },
            { id: "ui.leftPanel",  title: "Comută panoul din stânga", category: "Interfață", shortcut: "", icon: "panel-left" },
            { id: "ui.rightPanel", title: "Comută panoul din dreapta", category: "Interfață", shortcut: "", icon: "panel-right" },
            { id: "ui.panels",   title: "Mod focus (ascunde panourile)", category: "Interfață", shortcut: "Ctrl+\\", icon: "focus" },
            { id: "ui.theme",    title: "Comută tema (Sistem / Light / Dark)", category: "Interfață", shortcut: "", icon: "sun" },
            { id: "view.snapGrid", title: doc.snapToGrid ? "Oprește snap la grid" : "Snap la grid", category: "Vedere", shortcut: "Alt+G", icon: "grid-3x3" },
            { id: "nav.pivotView", title: "Orbitează în jurul centrului vederii", category: "Vedere", shortcut: "", icon: "orbit" },
            { id: "nav.pivotSelection", title: "Orbitează în jurul selecției", category: "Vedere", shortcut: "", icon: "orbit" },
            { id: "nav.pivotCursor", title: "Orbitează în jurul punctului de sub cursor", category: "Vedere", shortcut: "", icon: "orbit" },
            { id: "app.preferences", title: "Preferințe…", category: "Interfață", shortcut: "Ctrl+,", icon: "settings" },
            { id: "app.about", title: "Despre Aion", category: "Interfață", shortcut: "", icon: "info" },
            { id: "app.tour", title: "Turul uneltelor (cum se folosește fiecare)", category: "Ajutor", shortcut: "", icon: "info" }
        ]
        // Toate uneltele din toolbar, cu scurtăturile lor.
        for (const t of toolbar.allTools)
            list.push({ id: "tool." + t.id, title: t.name, category: "Unelte", shortcut: t.key || "", icon: t.icon })
        // Documentele recente.
        for (let i = 0; i < doc.recentFiles.length; ++i) {
            const path = doc.recentFiles[i]
            list.push({ id: "file.recent:" + i, title: "Deschide recent: " + path.split("/").pop(),
                        category: "Fișier", shortcut: "", icon: "layers" })
        }
        if (AppInfo.debugBuild)
            list.push({ id: "debug.storybook", title: "Storybook: toate componentele", category: "Dezvoltare", shortcut: "", icon: "palette" },
                      { id: "debug.demos", title: "Galeria animațiilor din ghiduri", category: "Dezvoltare", shortcut: "", icon: "palette" },
                      { id: "debug.benchmark", title: "Măsoară fps (rotește camera 4 s)", category: "Dezvoltare", shortcut: "", icon: "orbit" })
        return list
    }

    function runCommand(id) {
        if (id.startsWith("file.recent:")) {
            const path = doc.recentFiles[parseInt(id.slice(12))]
            window.confirmDiscard(() => doc.openFile(path))
            return
        }
        switch (id) {
        case "file.new": window.confirmDiscard(() => doc.newDocument()); break
        case "file.open": window.confirmDiscard(() => fileDialog.ask("open")); break
        case "file.save": window.save(null); break
        case "file.saveAs": fileDialog.ask("saveAs"); break
        case "file.import": fileDialog.ask("import"); break
        case "file.exportObj": fileDialog.ask("exportObj"); break
        case "file.exportGlb": fileDialog.ask("exportGlb"); break
        case "file.exportGltf": fileDialog.ask("exportGltf"); break
        case "file.exportPng": fileDialog.ask("exportPng"); break
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
        case "org.makeGroup": doc.makeGroup(); break
        case "org.makeComponent": doc.makeComponent(""); break
        case "org.explode": doc.explodeSelection(); break
        case "org.makeUnique": doc.makeUniqueSelection(); break
        case "org.enter": doc.enterInstance(0); break
        case "org.exit": doc.exitContext(); break
        case "org.hide": doc.hideSelection(); break
        case "org.unhideAll": doc.unhideAll(); break
        case "org.lock": doc.setSelectionLocked(true); break
        case "org.unlock": doc.setSelectionLocked(false); break
        case "edit.selectConnected": doc.selectConnected(); break
        case "edit.reverseFaces": doc.reverseSelectedFaces(); break
        case "view.zoomSelection": viewport.zoomToSelection(); break
        case "edit.intersectModel": doc.intersectSelection(true); break
        case "edit.intersectSelection": doc.intersectSelection(false); break
        case "solid.union": doc.booleanSelection("union"); break
        case "solid.subtract": doc.booleanSelection("subtract"); break
        case "solid.intersect": doc.booleanSelection("intersect"); break
        case "guides.clear": doc.deleteGuides(); break
        case "style.shaded": viewport.displayStyle = "shaded"; break
        case "style.clay": viewport.displayStyle = "clay"; break
        case "style.hiddenline": viewport.displayStyle = "hiddenline"; break
        case "style.wireframe": viewport.displayStyle = "wireframe"; break
        case "style.xray": viewport.displayStyle = "xray"; break
        case "style.shadows": viewport.shadows = !viewport.shadows; break
        case "style.profiles": viewport.profiles = !viewport.profiles; break
        case "style.fog": viewport.fog = !viewport.fog; break
        case "ui.leftPanel": window.leftPanelVisible = !window.leftPanelVisible; break
        case "ui.rightPanel": window.rightPanelVisible = !window.rightPanelVisible; break
        case "ui.panels": {
            const show = !(window.leftPanelVisible || window.rightPanelVisible)
            window.leftPanelVisible = show
            window.rightPanelVisible = show
            break
        }
        case "ui.theme": Theme.cycleMode(); break
        case "view.snapGrid":
            doc.snapToGrid = !doc.snapToGrid
            toast.show(doc.snapToGrid ? "Snap la grid: pornit (" + (doc.gridStep > 0 ? doc.formatLength(doc.gridStep) : "pas automat") + ")" : "Snap la grid: oprit")
            break
        case "nav.pivotView": viewport.orbitPivot = 0; toast.show("Orbita: în jurul centrului vederii"); break
        case "nav.pivotSelection": viewport.orbitPivot = 1; toast.show("Orbita: în jurul selecției"); break
        case "nav.pivotCursor": viewport.orbitPivot = 2; toast.show("Orbita: în jurul punctului de sub cursor"); break
        case "app.preferences": preferencesDialog.open(); break
        case "app.about": aboutDialog.open(); break
        case "app.tour": toolTour.open(); break
        case "debug.storybook": storybookLoader.active = true; break
        case "debug.demos": demoGalleryLoader.active = true; break
        case "debug.menus": if (menuBarInstantiator.object) menuBarInstantiator.object.dump(); break
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
                } else if (action.startsWith("save:")) {
                    doc.saveFile(action.slice(5), viewport.captureThumbnail())
                } else if (action.startsWith("open:")) {
                    doc.openFile(action.slice(5))
                } else if (action.startsWith("export:")) {
                    doc.exportFile(action.slice(7))
                } else if (action.startsWith("import:")) {
                    doc.importFile(action.slice(7))
                } else if (action.startsWith("png:")) {
                    viewport.saveImage(action.slice(4))
                } else if (action.startsWith("sun:")) {
                    viewport.sunHour = parseFloat(action.slice(4))
                } else if (action.startsWith("style:")) {
                    viewport.displayStyle = action.slice(6)
                } else if (action === "file.newNow") {
                    doc.newDocument()
                } else if (action.startsWith("rclickw:")) {
                    const r = action.slice(8).split(",").map(Number)
                    AppInfo.simulateRightClick(viewport, viewport.projectToScreen(Qt.vector3d(r[0], r[1], r[2])))
                } else if (action.startsWith("movew:")) {
                    const m = action.slice(6).split(",").map(Number)
                    AppInfo.simulateMove(viewport, viewport.projectToScreen(Qt.vector3d(m[0], m[1], m[2])))
                } else if (action.startsWith("wheelw:")) {
                    // „wheelw:x,y,z,trepte”: rotița în punctul 3D dat (trepte pozitive = apropiere).
                    const wh = action.slice(7).split(",").map(Number)
                    AppInfo.simulateWheel(viewport, viewport.projectToScreen(Qt.vector3d(wh[0], wh[1], wh[2])), wh[3] * 120)
                } else if (action.startsWith("moves:")) {
                    // Hover în coordonatele ferestrei (de exemplu pe un buton din toolbar).
                    const ms = action.slice(6).split(",").map(Number)
                    AppInfo.simulateMove(window.contentItem, Qt.point(ms[0], ms[1]))
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
    readonly property var modelingTools: ["select", "line", "arc", "rectangle", "circle", "polygon", "pushpull", "offset",
                                         "move", "rotate", "scale", "eraser", "paint",
                                         "tape", "protractor", "dimension", "text", "section"]
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
                     && !(window.nativeMenu && modelData.shortcut.indexOf("Ctrl+") >= 0)
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

    // --- Meniul nativ, preferințe, fișiere deschise din afară ---

    // Pe macOS, scurtăturile ⌘… sunt ale meniului nativ (altfel ar rula de două ori).
    readonly property bool nativeMenu: Qt.platform.os === "osx"
    readonly property var appWindow: window

    function openPalette() {
        palette.open()
    }

    Instantiator {
        id: menuBarInstantiator
        model: window.nativeMenu ? 1 : 0
        // MenuBar are propria proprietate `window`, care ar umbri id-ul ferestrei; de aceea appWindow.
        delegate: AppMenuBar {
            host: appWindow
            window: appWindow
            document: doc
            onAboutRequested: aboutDialog.open()
            onPreferencesRequested: preferencesDialog.open()
        }
    }

    // Tema și latitudinea se păstrează între sesiuni (unitățile le păstrează Document).
    Settings {
        id: preferences
        category: "ui"
        property int themeMode: 0
        property real latitude: 45
        property int orbitPivot: 0
        property bool zoomToSurface: true
        property real clipStart: 0
        property real clipEnd: 0
        property bool snapToGrid: false
        property real gridStep: 0
    }
    Component.onCompleted: {
        if (AppInfo.themeOverride === 0)
            Theme.mode = preferences.themeMode
        viewport.latitude = preferences.latitude
        viewport.orbitPivot = preferences.orbitPivot
        viewport.zoomToSurface = preferences.zoomToSurface
        viewport.clipStart = preferences.clipStart
        viewport.clipEnd = preferences.clipEnd
        doc.snapToGrid = preferences.snapToGrid
        doc.gridStep = preferences.gridStep
        window.openPendingFiles()
    }
    Connections {
        target: Theme
        function onModeChanged() { preferences.themeMode = Theme.mode }
    }
    Connections {
        target: viewport
        function onAppearanceChanged() { preferences.latitude = viewport.latitude }
        function onNavigationSettingsChanged() {
            preferences.orbitPivot = viewport.orbitPivot
            preferences.zoomToSurface = viewport.zoomToSurface
            preferences.clipStart = viewport.clipStart
            preferences.clipEnd = viewport.clipEnd
        }
    }

    // Fișierele deschise din Finder (dublu-click pe un .aion, „Open With”) sau date în linia de comandă.
    function openPendingFiles() {
        const files = AppInfo.takePendingFiles()
        for (const path of files) {
            if (path.toLowerCase().endsWith(".aion"))
                window.confirmDiscard(() => doc.openFile(path))
            else
                doc.importFile(path)
        }
    }
    Connections {
        target: doc
        function onGridSnapChanged() {
            preferences.snapToGrid = doc.snapToGrid
            preferences.gridStep = doc.gridStep
        }
    }
    Connections {
        target: AppInfo
        function onFileOpenRequested() { window.openPendingFiles() }
    }

    PreferencesDialog {
        id: preferencesDialog
        parent: window.contentItem
        document: doc
        viewport: viewport
    }
    AboutDialog {
        id: aboutDialog
        parent: window.contentItem
    }
    ToolTour {
        id: toolTour
        parent: window.contentItem
        shortcuts: toolbar.allTools.reduce((map, tool) => { map[tool.id] = tool.key || ""; return map }, {})
        onTryTool: (id) => window.selectTool(id)
    }

    // --- Fișiere ---

    // Salvează (în fișierul curent sau, dacă nu are, prin dialog), apoi rulează `then` dacă a reușit.
    function save(then) {
        if (doc.filePath.length === 0) {
            fileDialog.afterSave = then
            fileDialog.ask("saveAs")
            return
        }
        if (doc.saveFile("", viewport.captureThumbnail()) && then)
            then()
    }

    // Înainte de a închide documentul curent: dacă are modificări, întreabă.
    function confirmDiscard(action) {
        if (!doc.modified) {
            action()
            return
        }
        unsavedDialog.action = action
        unsavedDialog.open()
    }

    property bool allowClose: false
    onClosing: (close) => {
        if (doc.modified && !window.allowClose) {
            close.accepted = false
            window.confirmDiscard(() => { window.allowClose = true; window.close() })
        }
    }

    FileDialog {
        id: fileDialog

        property string purpose: ""
        property var afterSave: null

        function ask(what) {
            purpose = what
            const save = ["saveAs", "exportObj", "exportGlb", "exportGltf", "exportPng"].includes(what)
            fileMode = save ? FileDialog.SaveFile : FileDialog.OpenFile
            switch (what) {
            case "open": title = "Deschide"; nameFilters = ["Modele Aion (*.aion)"]; defaultSuffix = "aion"; break
            case "saveAs": title = "Salvează"; nameFilters = ["Modele Aion (*.aion)"]; defaultSuffix = "aion"; break
            case "import": title = "Importă"; nameFilters = ["Modele 3D (*.obj *.gltf *.glb)"]; defaultSuffix = ""; break
            case "exportObj": title = "Exportă OBJ"; nameFilters = ["OBJ (*.obj)"]; defaultSuffix = "obj"; break
            case "exportGlb": title = "Exportă GLB"; nameFilters = ["glTF binar (*.glb)"]; defaultSuffix = "glb"; break
            case "exportGltf": title = "Exportă glTF"; nameFilters = ["glTF (*.gltf)"]; defaultSuffix = "gltf"; break
            case "exportPng": title = "Exportă imaginea"; nameFilters = ["PNG (*.png)"]; defaultSuffix = "png"; break
            }
            if (save)
                selectedFile = "file:///" + doc.fileName + "." + defaultSuffix
            open()
        }

        onAccepted: {
            const path = doc.localPath(selectedFile)
            switch (purpose) {
            case "open": doc.openFile(path); break
            case "saveAs":
                if (doc.saveFile(path, viewport.captureThumbnail()) && afterSave)
                    afterSave()
                break
            case "import": doc.importFile(path); break
            case "exportObj":
            case "exportGlb":
            case "exportGltf":
                if (doc.exportFile(path)) toast.show("Exportat: " + path.split("/").pop())
                break
            case "exportPng":
                if (viewport.saveImage(path)) toast.show("Imagine salvată: " + path.split("/").pop())
                else toast.show("Imaginea nu a putut fi salvată.")
                break
            }
            afterSave = null
        }
        onRejected: afterSave = null
    }

    Connections {
        target: doc
        function onErrorChanged() { toast.show(doc.lastError) }
    }

    TitleBar {
        id: titleBar
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        z: 2
        documentName: doc.fileName
        modified: doc.modified
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
            model: ["Outliner", "Tag-uri", "Materiale", "Scene"]
        }

        Outliner {
            anchors.fill: parent
            visible: leftTabs.currentIndex === 0
            document: doc
        }
        TagsPanel {
            anchors.fill: parent
            visible: leftTabs.currentIndex === 1
            document: doc
        }
        ScenesPanel {
            anchors.fill: parent
            visible: leftTabs.currentIndex === 3
            document: doc
            viewport: viewport
        }
        MaterialsPanel {
            anchors.fill: parent
            visible: leftTabs.currentIndex === 2
            document: doc
            onPaintRequested: window.selectTool("paint")
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
        onContextMenuRequested: (position) => {
            contextMenu.items = doc.contextMenuItems()
            contextMenu.openAt(position.x, position.y)
        }
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
            document: doc
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

        // Contextul de editare: „Model › Casa › Fereastră”, cu ieșire.
        Rectangle {
            visible: doc.contextPath.length > 0
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: Theme.space3
            width: contextRow.implicitWidth + Theme.space2 * 2
            height: 32
            radius: 16
            color: Theme.bgElevated
            border.color: Theme.borderStrong

            Row {
                id: contextRow
                anchors.centerIn: parent
                spacing: Theme.space1

                Repeater {
                    model: ["Model"].concat(doc.contextPath)
                    delegate: Row {
                        required property string modelData
                        required property int index
                        spacing: Theme.space1
                        Icon {
                            visible: parent.index > 0
                            anchors.verticalCenter: parent.verticalCenter
                            name: "chevron-right"
                            size: 12
                            color: Theme.textTertiary
                        }
                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.modelData
                            color: parent.index === doc.contextPath.length ? Theme.textPrimary : Theme.textSecondary
                            font.pixelSize: Theme.fontSizeSm
                            font.weight: parent.index === doc.contextPath.length ? Font.DemiBold : Font.Normal
                        }
                    }
                }
                IconButton {
                    width: 24
                    height: 24
                    anchors.verticalCenter: parent.verticalCenter
                    iconName: "x"
                    iconSize: 12
                    tooltip: "Ieși din grup (Esc)"
                    onClicked: doc.exitContext()
                }
            }
        }

        // O sesiune nesalvată găsită la pornire.
        Rectangle {
            visible: doc.hasRecovery
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: Theme.space3
            width: recoveryRow.implicitWidth + Theme.space3 * 2
            height: 44
            radius: Theme.radiusLg
            color: Theme.bgElevated
            border.color: Theme.borderStrong

            Row {
                id: recoveryRow
                anchors.centerIn: parent
                spacing: Theme.space3
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "info"
                    size: Theme.iconSizeSm
                    color: Theme.accent
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Am găsit o sesiune nesalvată: " + doc.recoveryInfo
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeSm
                }
                Button {
                    small: true
                    variant: "primary"
                    text: "Recuperează"
                    onClicked: doc.recoverAutosave()
                }
                Button {
                    small: true
                    text: "Renunță"
                    onClicked: doc.discardAutosave()
                }
            }
        }

        // Mesaje scurte (export reușit, erori de fișier).
        Rectangle {
            id: toast
            property string message: ""
            function show(text) {
                message = text
                toastTimer.restart()
            }
            visible: opacity > 0
            opacity: toastTimer.running ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.motionNormal } }
            anchors.bottom: toolbar.top
            anchors.bottomMargin: Theme.space3
            anchors.horizontalCenter: parent.horizontalCenter
            width: Math.min(toastText.implicitWidth + Theme.space4 * 2, parent.width - Theme.space6 * 2)
            height: toastText.implicitHeight + Theme.space2 * 2
            radius: Theme.radiusMd
            color: Theme.bgElevated
            border.color: Theme.borderStrong
            Text {
                id: toastText
                anchors.centerIn: parent
                width: parent.width - Theme.space4 * 2
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: toast.message
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
            }
            Timer {
                id: toastTimer
                interval: 4500
            }
        }

        ContextMenu {
            id: contextMenu
            onTriggered: (id) => window.runCommand(id)
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

            EntityInfo {
                width: parent.width
                document: doc
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
                title: "Stil"

                Flow {
                    width: parent.width
                    spacing: Theme.space1 + 2
                    Repeater {
                        model: [
                            { id: "shaded", name: "Materiale" }, { id: "clay", name: "Clay" },
                            { id: "hiddenline", name: "Linii" }, { id: "wireframe", name: "Wireframe" },
                            { id: "xray", name: "X-Ray" }
                        ]
                        delegate: Rectangle {
                            id: styleChip
                            required property var modelData
                            readonly property bool current: viewport.displayStyle === modelData.id
                            width: styleText.implicitWidth + Theme.space3 * 2
                            height: 26
                            radius: 13
                            color: current ? Theme.accentSubtle : styleArea.containsMouse ? Theme.bgHover : "transparent"
                            border.color: current ? Theme.accent : Theme.borderStrong
                            Text {
                                id: styleText
                                anchors.centerIn: parent
                                text: styleChip.modelData.name
                                color: styleChip.current ? Theme.textPrimary : Theme.textSecondary
                                font.pixelSize: Theme.fontSizeXs
                            }
                            MouseArea {
                                id: styleArea
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: viewport.displayStyle = styleChip.modelData.id
                            }
                        }
                    }
                }
                Toggle {
                    width: parent.width
                    text: "Profile (contururi)"
                    checked: viewport.profiles
                    onToggled: viewport.profiles = checked
                }
                Toggle {
                    width: parent.width
                    text: "Ceață"
                    checked: viewport.fog
                    onToggled: viewport.fog = checked
                }
            }

            Section {
                visible: doc.sections.length > 0
                width: parent.width
                title: "Secțiuni"

                Repeater {
                    model: doc.sections
                    delegate: Item {
                        id: sectionRow
                        required property var modelData
                        width: parent.width
                        height: Theme.controlHeight
                        Toggle {
                            anchors.left: parent.left
                            anchors.right: sectionActions.left
                            anchors.verticalCenter: parent.verticalCenter
                            text: sectionRow.modelData.name
                            checked: sectionRow.modelData.active
                            onToggled: doc.setSectionActive(sectionRow.modelData.id, checked)
                        }
                        Row {
                            id: sectionActions
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            IconButton {
                                width: 24; height: 24; iconSize: Theme.iconSizeSm
                                iconName: "arrow-down"; tooltip: "Mută 100 mm înapoi"
                                onClicked: doc.moveSection(sectionRow.modelData.id, -100)
                            }
                            IconButton {
                                width: 24; height: 24; iconSize: Theme.iconSizeSm
                                iconName: "arrow-up"; tooltip: "Mută 100 mm înainte"
                                onClicked: doc.moveSection(sectionRow.modelData.id, 100)
                            }
                            IconButton {
                                width: 24; height: 24; iconSize: Theme.iconSizeSm
                                iconName: "trash-2"; tooltip: "Șterge secțiunea"
                                onClicked: doc.removeSection(sectionRow.modelData.id)
                            }
                        }
                    }
                }
            }

            Section {
                width: parent.width
                title: "Soare și umbre"

                Toggle {
                    width: parent.width
                    text: "Umbre"
                    checked: viewport.shadows
                    onToggled: viewport.shadows = checked
                }
                Slider {
                    visible: viewport.shadows
                    width: parent.width
                    label: "Ora"
                    from: 5
                    to: 21
                    stepSize: 0.25
                    value: viewport.sunHour
                    valueText: Math.floor(value) + ":" + ("0" + Math.round((value % 1) * 60)).slice(-2)
                    onMoved: viewport.sunHour = value
                }
                Slider {
                    visible: viewport.shadows
                    width: parent.width
                    label: "Luna"
                    from: 1
                    to: 12
                    stepSize: 1
                    value: viewport.sunMonth
                    valueText: Qt.locale("ro_RO").monthName(Math.round(value) - 1, Locale.ShortFormat)
                    onMoved: viewport.sunMonth = Math.round(value)
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
        unitText: ["Auto", "mm", "cm", "m", "inci", "picioare"][doc.displayUnit] ?? "mm"
        snapToGrid: doc.snapToGrid
        gridText: doc.gridStep > 0 ? doc.formatLength(doc.gridStep) : "auto"
        onSnapToggled: window.runCommand("view.snapGrid")
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

    Loader {
        id: demoGalleryLoader
        anchors.fill: parent
        z: 100
        active: false
        sourceComponent: DemoGallery {
            onCloseRequested: demoGalleryLoader.active = false
        }
    }

    // „Salvezi modificările?” înainte de a închide documentul.
    Popover {
        id: unsavedDialog
        property var action: null
        modal: true
        anchors.centerIn: parent
        width: 380
        padding: Theme.space6

        contentItem: Column {
            spacing: Theme.space4
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "Salvezi modificările din „" + doc.fileName + "”?"
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeLg
                font.weight: Font.DemiBold
            }
            Text {
                width: parent.width
                wrapMode: Text.Wrap
                text: "Dacă nu le salvezi, se pierd."
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeMd
            }
            Row {
                anchors.right: parent.right
                spacing: Theme.space2
                Button {
                    text: "Anulează"
                    onClicked: unsavedDialog.close()
                }
                Button {
                    text: "Nu salva"
                    onClicked: {
                        const action = unsavedDialog.action
                        unsavedDialog.close()
                        doc.discardAutosave()
                        if (action) action()
                    }
                }
                Button {
                    variant: "primary"
                    text: "Salvează"
                    onClicked: {
                        const action = unsavedDialog.action
                        unsavedDialog.close()
                        window.save(action)
                    }
                }
            }
        }
    }
}
