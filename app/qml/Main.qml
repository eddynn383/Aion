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
    // Pe Windows și Linux rămâne bara de titlu a sistemului (TitleBar-ul nostru stă sub ea).
    flags: Qt.platform.os === "osx" ? Qt.Window | Qt.ExpandedClientAreaHint | Qt.NoTitleBarBackgroundHint : Qt.Window

    property bool leftPanelVisible: true
    // Timeline-ul ține de modul Animare: acolo e mereu vizibil și nu se închide (docs/plans/moduri-de-lucru.md).
    readonly property bool timelineVisible: doc.workMode === 1
    property bool rightPanelVisible: true
    property string activeTool: "select"
    // Uneltele tratate de viewport ca navigare. Interact pornește rularea interacțiunilor (doc.interactionMode),
    // care primește clicurile direct.
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
            { id: "scene.add", title: "Adaugă scena curentă", category: "Vedere", shortcut: "", icon: "plus" },
            { id: "file.exportVideo", title: "Exportă animația (video MP4)…", category: "Fișier", shortcut: "", icon: "film" },
            { id: "object.camera", title: "Adaugă o cameră (din vederea curentă)", category: "Obiecte", shortcut: "", icon: "camera" },
            { id: "object.light.point", title: "Adaugă o lumină punctuală (bec)", category: "Obiecte", shortcut: "", icon: "lightbulb" },
            { id: "object.light.spot", title: "Adaugă un spot", category: "Obiecte", shortcut: "", icon: "light-spot" },
            { id: "object.light.directional", title: "Adaugă o lumină direcțională", category: "Obiecte", shortcut: "", icon: "sun" },
            { id: "object.light.area", title: "Adaugă un panou luminos", category: "Obiecte", shortcut: "", icon: "light-area" },
            { id: "camera.look", title: "Privește prin camera selectată", category: "Vizualizare", shortcut: "", icon: "video" },
            { id: "edit.mirrorX", title: "Oglindește pe X (roșu)", category: "Modelare", shortcut: "", icon: "flip-horizontal" },
            { id: "edit.mirrorY", title: "Oglindește pe Y (verde)", category: "Modelare", shortcut: "", icon: "flip-horizontal" },
            { id: "edit.mirrorZ", title: "Oglindește pe Z (albastru)", category: "Modelare", shortcut: "", icon: "flip-vertical" },
            { id: "edit.array", title: "Array: copii în linie sau în cerc…", category: "Modelare", shortcut: "", icon: "copy" },
            { id: "edit.chamfer", title: "Chamfer (teșește plat muchiile sau fața)…", category: "Modelare", shortcut: "", icon: "square" },
            { id: "edit.bevel", title: "Bevel (rotunjește muchiile sau fața)…", category: "Modelare", shortcut: "", icon: "circle" },
            { id: "edit.soften", title: "Soften / Smooth (muchii netede după unghi)…", category: "Modelare", shortcut: "", icon: "circle" },
            { id: "library.save", title: "Salvează obiectul selectat în bibliotecă", category: "Obiecte", shortcut: "", icon: "book-open" },
            { id: "view.environment:simplu", title: "Mediu: simplu", category: "Vizualizare", shortcut: "", icon: "sun" },
            { id: "view.environment:studio", title: "Mediu: studio", category: "Vizualizare", shortcut: "", icon: "sun" },
            { id: "view.environment:exterior", title: "Mediu: exterior", category: "Vizualizare", shortcut: "", icon: "sun" },
            { id: "view.environment:innorat", title: "Mediu: înnorat", category: "Vizualizare", shortcut: "", icon: "sun" },
            { id: "view.ambientOcclusion", title: "Umbrire ambientală (SSAO)", category: "Vizualizare", shortcut: "", icon: "circle" },
            { id: "view.progressiveAA", title: "Antialiasing progresiv (cât camera stă pe loc)", category: "Vizualizare", shortcut: "", icon: "circle" },
            { id: "material.new", title: "Material nou (personalizat)…", category: "Materiale", shortcut: "", icon: "palette" },
            { id: "mode.model", title: "Modul Modelare", category: "Moduri", shortcut: "Alt+1", icon: "box" },
            { id: "mode.animate", title: "Modul Animare", category: "Moduri", shortcut: "Alt+2", icon: "film" },
            { id: "mode.interact", title: "Modul Interacțiune", category: "Moduri", shortcut: "Alt+3", icon: "hand-pointer" },
            { id: "mode.interaction", title: "Interact: folosește interacțiunile (comută cu Select)", category: "Vizualizare", shortcut: "F5", icon: "play" },
            // Animare: tastatura timeline-ului (doar în modul Animare; Space nu mai alege Select acolo).
            { id: "anim.play", title: "Redă / pauză", category: "Animare", shortcut: "Space", icon: "play" },
            { id: "anim.prevFrame", title: "Cadrul anterior", category: "Animare", shortcut: "Left", icon: "arrow-left" },
            { id: "anim.nextFrame", title: "Cadrul următor", category: "Animare", shortcut: "Right", icon: "arrow-right" },
            { id: "anim.back10", title: "10 cadre înapoi", category: "Animare", shortcut: "Shift+Left", icon: "arrow-left" },
            { id: "anim.forward10", title: "10 cadre înainte", category: "Animare", shortcut: "Shift+Right", icon: "arrow-right" },
            { id: "anim.prevKey", title: "Cheia anterioară", category: "Animare", shortcut: "Alt+Left", icon: "diamond" },
            { id: "anim.nextKey", title: "Cheia următoare", category: "Animare", shortcut: "Alt+Right", icon: "diamond" },
            { id: "anim.start", title: "La începutul animației", category: "Animare", shortcut: "Home", icon: "skip-back" },
            { id: "anim.end", title: "La sfârșitul animației", category: "Animare", shortcut: "End", icon: "skip-back" },
            { id: "anim.key", title: "Keyframe pe selecție (toate canalele)", category: "Animare", shortcut: "K", icon: "diamond" },
            { id: "rules.edit", title: "Interacțiuni cu blocuri pentru obiectul selectat…", category: "Organizare", shortcut: "", icon: "hand-pointer" },
            { id: "file.quantities", title: "Raport de cantități (arii, volume, debitare mobilier)…", category: "Fișier", shortcut: "", icon: "ruler" },
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
            { id: "org.makeGroup", title: "Creează obiect (din formele selectate)", category: "Organizare", shortcut: "Ctrl+G", icon: "box" },
            { id: "org.group", title: "Grupează obiectele", category: "Organizare", shortcut: "Ctrl+Alt+G", icon: "folder" },
            { id: "org.duplicateLinked", title: "Duplicare legată (copie editată împreună)", category: "Organizare", shortcut: "Alt+D", icon: "component" },
            { id: "org.explode", title: "Desface (grup sau obiect)", category: "Organizare", shortcut: "", icon: "box" },
            { id: "org.makeUnique", title: "Fă unic (separă de copii)", category: "Organizare", shortcut: "", icon: "component" },
            { id: "org.enter", title: "Editează grupul sau obiectul selectat", category: "Organizare", shortcut: "", icon: "box" },
            { id: "org.exit", title: "Ieși din grupul sau obiectul editat", category: "Organizare", shortcut: "", icon: "arrow-left" },
            { id: "org.hide", title: "Ascunde selecția", category: "Organizare", shortcut: "", icon: "eye-off" },
            { id: "org.unhideAll", title: "Arată tot", category: "Organizare", shortcut: "", icon: "eye" },
            { id: "org.lock", title: "Blochează selecția", category: "Organizare", shortcut: "", icon: "lock" },
            { id: "org.unlock", title: "Deblochează selecția", category: "Organizare", shortcut: "", icon: "lock" },
            { id: "edit.createFaces", title: "Creează fețe din muchiile selectate", category: "Modelare", shortcut: "", icon: "square" },
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
            list.push({ id: "tool." + t.id, title: t.name, category: "Unelte", shortcut: t.key || "", alias: t.alias || "", icon: t.icon })
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
        list.push({ id: "script.console", title: "Consola de scripturi (JavaScript)…", category: "Scripturi", shortcut: "Ctrl+Alt+J", icon: "code" })
        for (const p of scripts.plugins)
            for (const c of p.commands)
                list.push({ id: "plugin:" + c.id, title: p.name + ": " + c.title, category: "Plugin-uri", shortcut: "", icon: "code" })
        for (const g of doc.generators)
            list.push({ id: "object.parametric:" + g.id, title: "Obiect dinamic: " + g.name, category: "Obiecte", shortcut: "", icon: "sliders-horizontal" })
        // Scurtăturile alese în Preferințe înlocuiesc pe cele implicite ("" = fără scurtătură).
        const custom = window.shortcutOverrides
        return list.map(c => Object.assign({}, c, {
            defaultShortcut: c.shortcut,
            shortcut: custom[c.id] !== undefined ? custom[c.id] : c.shortcut
        }))
    }

    // Cine primește fiecare tastă în modul curent: o comandă permisă în mod; la aceeași tastă, comanda modului (anim.*)
    // o bate pe cea comună (Space = redare în Animare, Select în rest). Așa nu există scurtături ambigue.
    readonly property var shortcutOwners: {
        const owners = {}
        const mode = doc.workMode
        for (const c of commands) {
            if (!doc.commandAllowed(c.id))
                continue
            for (const seq of [c.shortcut, c.alias || ""]) {
                if (seq.length === 0)
                    continue
                const current = owners[seq]
                if (current === undefined || (!current.startsWith("anim.") && c.id.startsWith("anim.")))
                    owners[seq] = c.id
            }
        }
        return owners
    }
    // Săgețile simple sunt ale uneltei cât are o operație în curs (sau se tastează o valoare); altfel ale comenzii
    // care le are în acest mod (în Animare: cadrele).
    readonly property bool toolKeepsArrows: doc.toolOwnsArrows || vcbText.length > 0
    function plainArrow(seq) {
        return seq === "Left" || seq === "Right" || seq === "Up" || seq === "Down"
    }
    function activeSequences(c) {
        return [c.shortcut, c.alias || ""].filter(s => s.length > 0 && shortcutOwners[s] === c.id
                                                  && !(plainArrow(s) && toolKeepsArrows))
    }

    // Scurtăturile personalizate: id comandă → secvență (păstrate în preferințe).
    readonly property var shortcutOverrides: {
        try { return JSON.parse(preferences.customShortcuts) } catch (e) { return {} }
    }
    // O scurtătură nouă pentru comandă (null = revine la cea implicită). O combinație deja folosită trece la ea:
    // comanda care o avea rămâne fără scurtătură (întoarce titlul ei, pentru mesaj).
    function setShortcut(id, sequence) {
        const next = Object.assign({}, shortcutOverrides)
        let taken = ""
        if (sequence) {
            for (const c of commands) {
                if (c.id !== id && c.shortcut === sequence) {
                    next[c.id] = ""
                    taken = c.title
                }
            }
        }
        const command = commands.find(c => c.id === id)
        if (sequence === null || (command && command.defaultShortcut === sequence))
            delete next[id]
        else
            next[id] = sequence
        preferences.customShortcuts = JSON.stringify(next)
        return taken
    }
    function resetShortcuts() {
        preferences.customShortcuts = "{}"
    }

    function runCommand(id) {
        if (id.startsWith("plugin:")) {
            const r = scripts.runPluginCommand(id.slice(7))
            if (!r.ok) toast.show("Plugin: " + r.error)
            return
        }
        if (id.startsWith("view.environment:")) {
            preferences.environment = id.slice(17)
            return
        }
        if (id === "script.console") {
            scriptConsole.open()
            return
        }
        if (id.startsWith("object.parametric:")) {
            const c = viewport.cameraState()
            doc.createParametric(id.slice(18), Qt.vector3d(c.tx, c.ty, 0))
            return
        }
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
        case "file.quantities": quantitiesDialog.open(); break
        case "object.camera": doc.addCamera(viewport.cameraState(), 0); break
        case "object.light.point":
        case "object.light.spot":
        case "object.light.directional":
        case "object.light.area": {
            const c = viewport.cameraState()
            doc.addLight(id.slice(13), Qt.vector3d(c.tx, c.ty, c.tz))
            break
        }
        case "camera.look":
            if (doc.selectedObject.kind === "camera") viewport.animateToCamera(doc.cameraView(doc.selectedObject.id), 0.8)
            else toast.show("Selectează o cameră (în Outliner, la Camere și lumini).")
            break
        case "mode.interaction":
            // F5: Select ↔ Interact; din alt mod, trece în Interacțiune cu Interact activ.
            if (doc.workMode !== 2) {
                window.setWorkMode(2)
                window.selectTool("interact")
            } else {
                window.selectTool(window.activeTool === "interact" ? "select" : "interact")
            }
            break
        case "anim.play": doc.togglePlay(); break
        case "anim.prevFrame": doc.stepFrames(-1); break
        case "anim.nextFrame": doc.stepFrames(1); break
        case "anim.back10": doc.stepFrames(-10); break
        case "anim.forward10": doc.stepFrames(10); break
        case "anim.prevKey": doc.jumpToKey(-1); break
        case "anim.nextKey": doc.jumpToKey(1); break
        case "anim.start": doc.jumpToEdge(false); break
        case "anim.end": doc.jumpToEdge(true); break
        case "anim.key": doc.keySelection(); break
        case "mode.model": window.setWorkMode(0); break
        case "mode.animate": window.setWorkMode(1); break
        case "mode.interact": window.setWorkMode(2); break
        case "rules.edit": window.openRulesEditor(); break
        case "file.exportVideo":
            if (doc.activeAnimation > 0) fileDialog.ask("exportVideo")
            else toast.show("Creează întâi o animație în timeline.")
            break
        case "scene.add": doc.addScene(viewport.cameraState(), viewport.displayStyle); break
        case "debug.materials": preferences.leftMaterials = true; break
        case "debug.leftAll":
            preferences.leftOutliner = preferences.leftTags = preferences.leftMaterials = true
            preferences.leftScenes = preferences.leftInteractions = true
            break
        case "edit.mirrorX": doc.mirrorSelection(0); break
        case "edit.mirrorY": doc.mirrorSelection(1); break
        case "edit.mirrorZ": doc.mirrorSelection(2); break
        case "edit.array": arrayPanel.start(); break
        case "edit.soften": softenPanel.start(); break
        case "edit.chamfer": bevelPanel.start(false); break
        case "edit.bevel": bevelPanel.start(true); break
        case "library.save": window.saveToLibrary(); break
        case "debug.objectsOnly":
            preferences.leftLibrary = preferences.leftTags = preferences.leftMaterials = false
            preferences.leftScenes = preferences.leftInteractions = false
            preferences.leftOutliner = true
            break
        case "debug.materialsOnly":
            preferences.leftOutliner = preferences.leftLibrary = preferences.leftTags = false
            preferences.leftScenes = preferences.leftInteractions = false
            preferences.leftMaterials = true
            break
        case "view.ambientOcclusion": preferences.ambientOcclusion = !preferences.ambientOcclusion; break
        case "view.progressiveAA": preferences.progressiveAA = !preferences.progressiveAA; break
        case "material.new": preferences.leftMaterials = true; materialsPanel.newMaterial(); break
        case "debug.cutlist": quantitiesDialog.tab = 1; quantitiesDialog.open(); break
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
        case "org.group": doc.groupSelection(); break
        case "org.duplicateLinked": doc.duplicateLinked(); break
        case "org.explode": doc.explodeSelection(); break
        case "org.makeUnique": doc.makeUniqueSelection(); break
        case "org.enter": doc.enterInstance(0); break
        case "org.exit": doc.exitContext(); break
        case "org.hide": doc.hideSelection(); break
        case "org.unhideAll": doc.unhideAll(); break
        case "org.lock": doc.setSelectionLocked(true); break
        case "org.unlock": doc.setSelectionLocked(false); break
        case "edit.selectConnected": doc.selectConnected(); break
        case "edit.createFaces": doc.createFacesFromSelection(); break
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
        case "app.preferences": preferencesPage.active = true; break
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
                } else if (action.startsWith("anim:")) {
                    // „anim:new”, „anim:key”, „anim:color”, „anim:camera”, „anim:time,1.5”, „anim:play”.
                    const an = action.slice(5).split(",")
                    if (an[0] === "new") { window.setWorkMode(1); doc.createAnimation() }
                    else if (an[0] === "key") doc.keySelection()
                    else if (an[0] === "color") doc.keySelectionColor()
                    else if (an[0] === "camera") doc.keyCamera(viewport.cameraState())
                    else if (an[0] === "time") doc.timelineTime = parseFloat(an[1])
                    else if (an[0] === "play") doc.play()
                } else if (action.startsWith("scene:")) {
                    // „scene:0” aplică scena cu indexul dat (camera și obiectele).
                    const sc = doc.scenes[parseInt(action.slice(6))]
                    viewport.animateToCamera(doc.sceneCamera(sc.id), 1.0)
                    doc.applyScene(sc.id, 1.0)
                } else if (action.startsWith("tag:")) {
                    // „tag:add,Pereți”, „tag:assign,0” (indexul tag-ului), „tag:hide,0”, „tag:select,0”.
                    const tg = action.slice(4).split(",")
                    if (tg[0] === "add") doc.addTag(tg[1])
                    else if (tg[0] === "assign") doc.assignTag(doc.tags[parseInt(tg[1])].id)
                    else if (tg[0] === "hide") doc.setTagVisible(doc.tags[parseInt(tg[1])].id, false)
                    else if (tg[0] === "select") doc.selectByTag(doc.tags[parseInt(tg[1])].id)
                } else if (action.startsWith("themecolor:")) {
                    // „themecolor:accent=#EC4899” (gol după „=” resetează).
                    const tc = action.slice(11).split("=")
                    Theme.setOverride(tc[0], tc[1] || "")
                } else if (action.startsWith("mode:")) {
                    // „mode:model|animate|interact”: modul de lucru.
                    window.setWorkMode(["model", "animate", "interact"].indexOf(action.slice(5)))
                } else if (action.startsWith("prefs:")) {
                    preferencesPage.active = true
                    preferencesPage.item.category = parseInt(action.slice(6))
                } else if (action.startsWith("material:")) {
                    // „material:back,3” = materialul 3 din bibliotecă pe versoul selecției.
                    const mt = action.slice(9).split(",")
                    doc.assignMaterial({ library: parseInt(mt[1]) }, mt[0])
                } else if (action.startsWith("placement:")) {
                    // „placement:dz=600,rotation=30” pe instanța selectată.
                    const pl = {}
                    for (const kv of action.slice(10).split(",")) {
                        const parts = kv.split("=")
                        pl[parts[0]] = parseFloat(parts[1])
                    }
                    doc.setPlacement(pl)
                } else if (action.startsWith("behavior:")) {
                    // „behavior:rotate,0,90” pe instanța selectată.
                    const bh = action.slice(9).split(",")
                    doc.setBehavior({ kind: bh[0], anchor: parseInt(bh[1]), amount: parseFloat(bh[2]), duration: 0.6 })
                } else if (action.startsWith("rule:")) {
                    // „rule:click,moveBy,0,0,500” adaugă o regulă instanței selectate.
                    const ru = action.slice(5).split(",")
                    const target = doc.entityInfo.instanceId
                    const rules = doc.rulesOf(target)
                    rules.push({ trigger: ru[0], key: "", actions: [{ kind: ru[1], target: 0, x: parseFloat(ru[2] || 0),
                        y: parseFloat(ru[3] || 0), z: parseFloat(ru[4] || 0), duration: 0.4, material: 0, animation: 0,
                        scene: 0, first: [], second: [] }] })
                    doc.setRules(target, rules)
                } else if (action.startsWith("exportvideo:")) {
                    window.exportVideo(action.slice(12))
                } else if (action.startsWith("object:")) {
                    // „object:intensity=3,color=#FF0000” pe camera / lumina selectată (valori numerice sau text).
                    const props = {}
                    for (const kv of action.slice(7).split(",")) {
                        const parts = kv.split("=")
                        props[parts[0]] = isNaN(parseFloat(parts[1])) ? parts[1] : parseFloat(parts[1])
                    }
                    doc.setObjectProperties(props)
                } else if (action.startsWith("clicks:")) {
                    // Click în coordonatele ferestrei (butoane, câmpuri).
                    const cs = action.slice(7).split(",").map(Number)
                    AppInfo.simulateClick(window.contentItem, Qt.point(cs[0], cs[1]))
                } else if (action.startsWith("keys:")) {
                    // „keys:45,enter”: taste ca de la tastatură (cu scurtăturile aplicației), apoi o tastă finală.
                    const kk = action.slice(5).split(",")
                    AppInfo.simulateKeys(kk[0], kk[1] ?? "")
                } else if (action === "debug.geometry") {
                    console.info("geometry:", JSON.stringify(doc.entityInfo.geometry), "undo:", doc.undoName)
                } else if (action === "debug.anim") {
                    console.info("anim: time", doc.timelineTime.toFixed(3), "playing", doc.playing, "tool", window.activeTool,
                                 "undo:", doc.undoName, "keys:", JSON.stringify((doc.timeline.tracks ?? []).map(
                                     t => t.channels.map(c => c.property + "=" + c.keys.map(k => k.time + ":" + k.text).join("|")))))
                } else if (action === "debug.rz") {
                    console.info("rz:", JSON.stringify(doc.entityInfo.placement), "undo:", doc.undoName)
                } else if (action.startsWith("shape:")) {
                    // „shape:segments=12”: mărimea formei selectate (ca din Inspector).
                    const kv = action.slice(6).split("=")
                    doc.setShapeValue(kv[0], parseFloat(kv[1]))
                } else if (action.startsWith("draglight:")) {
                    // „draglight:dx,dy,shift”: trage marcajul primei lumini cu (dx, dy) pixeli; afișează poziția.
                    const dl = action.slice(10).split(",").map(Number)
                    doc.selectObject("light", doc.lights[0].id)
                    const p = doc.selectedObject.position
                    const screen = viewport.mapToItem(window.contentItem, viewport.projectToScreen(Qt.vector3d(p[0], p[1], p[2])))
                    const before = JSON.stringify(p) // înainte de tragere (p e o referință vie)
                    AppInfo.simulateDrag(window.contentItem, screen, Qt.point(screen.x + dl[0], screen.y + dl[1]))
                    console.info("light:", before, "→", JSON.stringify(doc.selectedObject.position), "undo:", doc.undoName)
                } else if (action.startsWith("droppreview:")) {
                    // „droppreview:gen:table,700,450”: cutia de previzualizare a obiectului la poziția din viewport.
                    const dp = action.slice(12).split(",")
                    const size = doc.libraryItemSize(dp[0])
                    viewport.setDropPreview(viewport.dropPointAt(Qt.point(parseFloat(dp[1]), parseFloat(dp[2]))),
                                            Qt.vector3d(size.dx, size.dy, size.dz))
                } else if (action.startsWith("scriptjs:")) {
                    // Cod JavaScript direct (pentru teste): „scriptjs:aion.box(0,0,0,100,100,100)”.
                    const r = scripts.run(action.slice(9), "Script")
                    console.info("scriptjs:", r.ok, r.output, r.error)
                } else if (action.startsWith("script:")) {
                    // „script:0” rulează exemplul 0 din consolă.
                    const r = scripts.run(scripts.examples[parseInt(action.slice(7))].code, "Script")
                    console.info("script:", r.ok, r.output, r.error)
                } else if (action.startsWith("drags:")) {
                    // Tragere în coordonatele ferestrei: „drags:x1,y1,x2,y2”.
                    const dg = action.slice(6).split(",").map(Number)
                    AppInfo.simulateDrag(window.contentItem, Qt.point(dg[0], dg[1]), Qt.point(dg[2], dg[3]))
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
    readonly property var modelingTools: ["select", "interact", "line", "arc", "rectangle", "circle", "polygon", "pushpull", "offset",
                                         "followme", "move", "rotate", "scale", "eraser", "paint", "texture",
                                         "tape", "protractor", "dimension", "text", "section", "cube", "sphere", "cone", "cylinder"]
    property string pendingToolMessage: ""

    // Modul de lucru (0 Modelare, 1 Animare, 2 Interacțiune): Document oprește ce era în curs, unealta revine la
    // Select, iar preferința se păstrează pentru repornire.
    function setWorkMode(mode) {
        if (mode < 0 || mode > 2)
            return
        doc.workMode = mode
        preferences.workMode = mode
        selectTool("select")
    }

    function selectTool(id) {
        // O unealtă din alt mod de lucru nu pornește (de exemplu Line în Animare).
        if (!doc.commandAllowed("tool." + id)) {
            toast.show("Unealta nu e disponibilă în modul " + ["Modelare", "Animare", "Interacțiune"][doc.workMode] + ".")
            id = "select"
        }
        window.activeTool = id
        // Interact folosește interacțiunile obiectelor (rulează pe o copie a modelului); orice altă unealtă revine la
        // editare, iar starea rulării se aruncă.
        doc.interactionMode = id === "interact"
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

    function saveToLibrary() {
        const info = doc.entityInfo
        if (info.kind !== "instance") {
            toast.show("Selectează un obiect ca să-l salvezi în bibliotecă.")
            return
        }
        if (doc.saveSelectionToLibrary(info.name, viewport.captureThumbnail()))
            toast.show("„" + info.name + "” e acum în bibliotecă.")
    }

    // Fantoma unui element tras din bibliotecă; lăsată peste viewport, pune obiectul acolo.
    Rectangle {
        id: libraryGhost
        property string itemId: ""
        property string label: ""
        property string icon: "box"
        property point position: Qt.point(-1000, -1000)
        function finish() {
            Drag.drop()
            itemId = ""
            position = Qt.point(-1000, -1000)
        }
        parent: window.contentItem
        z: 1000
        visible: itemId.length > 0
        x: position.x - width / 2
        y: position.y - height / 2
        width: ghostRow.implicitWidth + Theme.space3 * 2
        height: 32
        radius: 16
        color: Theme.bgElevated
        border.color: Theme.accent
        Drag.active: itemId.length > 0
        Drag.keys: ["aion-library"]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2
        Row {
            id: ghostRow
            anchors.centerIn: parent
            spacing: Theme.space2
            Icon { anchors.verticalCenter: parent.verticalCenter; name: libraryGhost.icon; size: 14; color: Theme.accent }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: libraryGhost.label
                color: Theme.textPrimary
                font.pixelSize: Theme.fontSizeSm
            }
        }
    }

    function openRulesEditor() {
        const info = doc.entityInfo
        if (info.kind !== "instance") {
            toast.show("Selectează un grup sau o componentă ca să-i adaugi interacțiuni.")
            return
        }
        rulesEditor.openFor(info.instanceId, info.name)
    }

    function escapePressed() {
        if (arrayPanel.visible || softenPanel.visible || bevelPanel.visible) {
            doc.cancelOperation()
            arrayPanel.close()
            softenPanel.close()
            bevelPanel.close()
        } else if (doc.interactionMode)
            window.selectTool("select")
        else if (vcbText.length > 0)
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
            sequences: window.activeSequences(modelData)
            // Cât timp se tastează în Measurements box, literele sunt unități („m”, „cm”), nu unelte.
            enabled: sequences.length > 0 && !palette.opened && (doc.workMode, doc.commandAllowed(modelData.id))
                     && !(window.vcbText.length > 0 && modelData.shortcut.indexOf("Ctrl+") < 0 && modelData.shortcut.indexOf("Alt+") < 0)
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
    // Cât se scrie într-un câmp de text (Inspector, nume), Enter și Esc sunt ale câmpului.
    readonly property bool typingInField: activeFocusItem !== null && activeFocusItem.cursorPosition !== undefined
    Shortcut {
        sequence: "Escape"
        enabled: !palette.opened && !window.typingInField
        onActivated: window.escapePressed()
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: !palette.opened && !window.typingInField
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
            // În Animare, ← → sunt ale timeline-ului, dacă unealta nu are o operație în curs.
            enabled: doc.hasMeasurement && !palette.opened
                     && (window.toolKeepsArrows || window.shortcutOwners[sequence] === undefined)
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
            onPreferencesRequested: preferencesPage.active = true
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
        property real animTransition: 2
        property real animDelay: 1
        property bool animLoop: true
        property int animFps: 30
        property real sceneTransition: 1
        property bool leftOutliner: true
        property bool leftMaterials: true
        property bool leftScenes: false
        property bool leftTags: false
        property bool leftLibrary: true
        property bool leftInteractions: false
        // Înălțimile secțiunilor din stânga (JSON: cheie → px), schimbate trăgând de marginea lor.
        property string leftSectionHeights: "{}"
        property string customShortcuts: "{}"
        property string environment: "simplu"
        property bool ambientOcclusion: true
        property bool progressiveAA: true
        property int msaa: 4
        property int shadowQuality: 2048
        property real gridStep: 0
        property int workMode: 0
    }
    Component.onCompleted: {
        // Ultimul mod de lucru; rulările automate (scripturi de test) pornesc mereu în Modelare.
        doc.workMode = AppInfo.startupActions.length > 0 ? 0 : preferences.workMode
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

    // Preferințele: o pagină peste toată fereastra.
    Loader {
        id: preferencesPage
        anchors.fill: parent
        z: 150
        active: false
        sourceComponent: PreferencesPage {
            document: doc
            viewport: viewport
            settings: preferences
            commands: window.commands
            onShortcutChanged: (id, sequence) => {
                const taken = window.setShortcut(id, sequence)
                shortcutNotice = taken ? "Combinația era folosită de „" + taken + "”, care a rămas fără scurtătură." : ""
            }
            onShortcutsReset: window.resetShortcuts()
            onCloseRequested: preferencesPage.active = false
        }
    }
    AboutDialog {
        id: aboutDialog
        parent: window.contentItem
    }
    // Drop din bibliotecă: obiectul se pune pe suprafața (sau solul) de sub cursor.
    DropArea {
        parent: viewport
        anchors.fill: parent
        keys: ["aion-library"]
        // Un material tras: Paint Bucket e activ cât timp e deasupra (fața de sub cursor se evidențiază).
        property string toolBeforeMaterial: ""
        readonly property bool material: libraryGhost.itemId.startsWith("mat:")
        onEntered: if (material) { toolBeforeMaterial = window.activeTool; window.selectTool("paint") }
        // Cutia obiectului urmează cursorul (pe suprafața sau solul de sub el).
        onPositionChanged: (drag) => {
            if (material) {
                viewport.toolHoverAt(Qt.point(drag.x, drag.y))
                return
            }
            const size = doc.libraryItemSize(libraryGhost.itemId)
            if (size.dx === undefined)
                return
            viewport.setDropPreview(viewport.dropPointAt(Qt.point(drag.x, drag.y)), Qt.vector3d(size.dx, size.dy, size.dz))
        }
        onExited: {
            viewport.clearDropPreview()
            if (material && toolBeforeMaterial.length > 0) window.selectTool(toolBeforeMaterial)
        }
        onDropped: (drop) => {
            viewport.clearDropPreview()
            if (material) {
                // „mat:model:ID” / „mat:lib:INDEX”: îl alegem ca material curent și vopsim fața (sau obiectul) de sub cursor.
                const parts = libraryGhost.itemId.split(":")
                if (parts[1] === "model") doc.selectModelMaterial(parseFloat(parts[2]))
                else doc.selectLibraryMaterial(parseInt(parts[2]))
                viewport.toolClickAt(Qt.point(drop.x, drop.y))
                if (toolBeforeMaterial.length > 0) window.selectTool(toolBeforeMaterial)
                toolBeforeMaterial = ""
            } else {
                doc.placeLibraryItem(libraryGhost.itemId, viewport.dropPointAt(Qt.point(drop.x, drop.y)))
            }
            drop.accept()
        }
    }
    ArrayPanel {
        id: arrayPanel
        parent: viewport
        x: Theme.space3
        y: 56
        document: doc
    }
    SoftenPanel {
        id: softenPanel
        parent: viewport
        x: Theme.space3
        y: 56
        document: doc
    }
    BevelPanel {
        id: bevelPanel
        parent: viewport
        x: Theme.space3
        y: 56
        document: doc
    }
    ScriptEngine {
        id: scripts
        document: doc
        onMessageRequested: (text) => toast.show(text)
        onCommandRequested: (id) => window.runCommand(id)
    }
    ScriptConsole {
        id: scriptConsole
        parent: window.contentItem
        engine: scripts
    }
    RulesEditor {
        id: rulesEditor
        parent: window.contentItem
        document: doc
    }
    QuantitiesDialog {
        id: quantitiesDialog
        parent: window.contentItem
        document: doc
        onExportRequested: (kind, options) => {
            fileDialog.reportKind = kind
            fileDialog.reportOptions = options
            fileDialog.ask("exportReport")
        }
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
        if (WebFiles.available) {
            if (window.webSave("aion") && then)
                then()
            return
        }
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
        property string reportKind: "general"
        property var reportOptions: ({})
        // Imaginea aleasă pentru o textură (editorul de material).
        property var textureCallback: null

        function ask(what) {
            // În browser: selectorul de fișiere al browserului și download-uri.
            if (WebFiles.available) {
                switch (what) {
                case "open": WebFiles.pick("open", "Modele Aion (*.aion)"); break
                case "import": WebFiles.pick("import", "Modele 3D (*.obj *.gltf *.glb)"); break
                case "saveAs":
                    if (window.webSave("aion") && afterSave) afterSave()
                    afterSave = null
                    break
                case "exportObj": window.webSave("obj"); break
                case "exportGlb": window.webSave("glb"); break
                case "exportGltf": window.webSave("gltf"); break
                case "exportPng": window.webSave("png"); break
                case "exportReport": {
                    const name = doc.fileName + (reportKind === "cutlist" ? " - debitare" : " - cantitati") + ".csv"
                    const path = WebFiles.temporaryPath(name)
                    if (doc.exportReport(path, reportKind, reportOptions)) WebFiles.download(path, name)
                    break
                }
                case "exportVideo": toast.show("Exportul video nu e disponibil în browser (folosește aplicația desktop)."); break
                case "textureImage": WebFiles.pick("texture", "Imagini (*.png *.jpg *.jpeg)"); break
                }
                return
            }
            purpose = what
            const save = ["saveAs", "exportObj", "exportGlb", "exportGltf", "exportPng", "exportReport", "exportVideo"].includes(what)
            fileMode = save ? FileDialog.SaveFile : FileDialog.OpenFile
            switch (what) {
            case "open": title = "Deschide"; nameFilters = ["Modele Aion (*.aion)"]; defaultSuffix = "aion"; break
            case "saveAs": title = "Salvează"; nameFilters = ["Modele Aion (*.aion)"]; defaultSuffix = "aion"; break
            case "import": title = "Importă"; nameFilters = ["Modele 3D (*.obj *.gltf *.glb)"]; defaultSuffix = ""; break
            case "textureImage": title = "Alege imaginea texturii"; nameFilters = ["Imagini (*.png *.jpg *.jpeg)"]; defaultSuffix = ""; break
            case "exportObj": title = "Exportă OBJ"; nameFilters = ["OBJ (*.obj)"]; defaultSuffix = "obj"; break
            case "exportGlb": title = "Exportă GLB"; nameFilters = ["glTF binar (*.glb)"]; defaultSuffix = "glb"; break
            case "exportGltf": title = "Exportă glTF"; nameFilters = ["glTF (*.gltf)"]; defaultSuffix = "gltf"; break
            case "exportPng": title = "Exportă imaginea"; nameFilters = ["PNG (*.png)"]; defaultSuffix = "png"; break
            case "exportReport": title = "Exportă raportul"; nameFilters = ["CSV (*.csv)"]; defaultSuffix = "csv"; break
            case "exportVideo": title = "Exportă animația"; nameFilters = ["Video MP4 (*.mp4)"]; defaultSuffix = "mp4"; break
            }
            if (save)
                selectedFile = "file:///" + doc.fileName
                    + (what === "exportReport" ? (reportKind === "cutlist" ? " - debitare" : " - cantitati") : "")
                    + "." + defaultSuffix
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
            case "exportReport":
                if (doc.exportReport(path, reportKind, reportOptions)) toast.show("Raport salvat: " + path.split("/").pop())
                break
            case "exportVideo": window.exportVideo(path); break
            case "textureImage": if (textureCallback) textureCallback(path); break
            }
            afterSave = null
        }
        onRejected: afterSave = null
    }

    Connections {
        target: doc
        function onErrorChanged() { toast.show(doc.lastError) }
    }

    // Web: salvează în memorie, apoi descarcă (kind: aion, obj, glb, gltf, png).
    function webSave(kind) {
        const name = doc.fileName + "." + kind
        const path = WebFiles.temporaryPath(name)
        let ok = false
        if (kind === "aion")
            ok = doc.saveFile(path, viewport.captureThumbnail())
        else if (kind === "png")
            ok = viewport.saveImage(path)
        else
            ok = doc.exportFile(path)
        if (ok) {
            WebFiles.download(path, name)
            if (kind === "obj")
                WebFiles.download(path.replace(/\.obj$/, ".mtl"), doc.fileName + ".mtl")
        }
        return ok
    }
    Connections {
        target: WebFiles
        function onFilePicked(purpose, path) {
            if (purpose === "texture") {
                if (fileDialog.textureCallback) fileDialog.textureCallback(path)
            } else if (purpose === "open")
                window.confirmDiscard(() => doc.openFile(path))
            else
                doc.importFile(path)
        }
    }

    // Animația scenelor → cadre PNG (viewport) → video MP4 (AppInfo.encodeVideo).
    property string videoOutput: ""
    function exportVideo(path) {
        window.videoOutput = path
        const frames = path.replace(/\.mp4$/i, "") + " - cadre"
        viewport.exportAnimation(frames, doc.timeline.fps ?? 30)
        toast.show("Se exportă animația… (progresul e în timeline)")
    }
    Connections {
        target: viewport
        function onAnimationExported(directory, frames, fps) {
            if (frames === 0) {
                toast.show("Exportul animației a fost anulat.")
                return
            }
            const error = AppInfo.encodeVideo(directory, fps, window.videoOutput)
            toast.show(error.length === 0 ? "Video salvat: " + window.videoOutput.split("/").pop() : error)
        }
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
        showHeader: false

        Behavior on width { NumberAnimation { duration: Theme.motionNormal; easing.type: Easing.OutCubic } }

        // Acordeon: Obiecte, Bibliotecă, Tag-uri, Materiale, Scene, Interacțiuni — toate cu același antet (iconiță, titlu,
        // număr, acțiuni). Fiecare secțiune deschisă are înălțimea ei (trasă de marginea de jos, păstrată în preferințe);
        // când nu încap toate, panoul întreg are scroll.
        Flickable {
            id: accordion
            anchors.fill: parent
            anchors.topMargin: Theme.space1
            contentHeight: sectionsColumn.height + Theme.space3
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            readonly property bool scrollable: contentHeight > height + 1

            readonly property var heights: {
                try { return JSON.parse(preferences.leftSectionHeights) } catch (e) { return {} }
            }
            function heightOf(key, fallback) { return accordion.heights[key] ?? fallback }
            function setHeight(key, value) {
                const next = Object.assign({}, accordion.heights)
                next[key] = Math.round(value)
                preferences.leftSectionHeights = JSON.stringify(next)
            }
            function resetHeight(key) {
                const next = Object.assign({}, accordion.heights)
                delete next[key]
                preferences.leftSectionHeights = JSON.stringify(next)
            }

            component HeaderButton: IconButton {
                width: 22; height: 22; iconSize: 13
            }

            Column {
                id: sectionsColumn
                // Loc pentru bara de scroll, cât e vizibilă.
                width: accordion.width - (accordion.scrollable ? 10 : 0)
                spacing: Theme.space1

                AccordionSection {
                    id: objectsSection
                    width: parent.width
                    separator: false
                    title: "Obiecte"
                    iconName: "layers"
                    count: doc.outline.length + doc.cameras.length + doc.lights.length
                    expanded: preferences.leftOutliner
                    bodyHeight: accordion.heightOf("objects", 300)
                    onResizeRequested: (value) => accordion.setHeight("objects", value)
                    onResetRequested: accordion.resetHeight("objects")
                    onToggled: preferences.leftOutliner = !preferences.leftOutliner
                    actions: HeaderButton {
                        id: addObjectButton
                        iconName: "plus"
                        tooltip: "Adaugă o cameră sau o lumină"
                        checked: addObjectMenu.opened
                        onClicked: addObjectMenu.opened ? addObjectMenu.close() : addObjectMenu.open()
                        Popover {
                            id: addObjectMenu
                            x: parent.width - width
                            y: parent.height + 4
                            width: 240
                            padding: Theme.space1
                            contentItem: Column {
                                Repeater {
                                    model: [
                                        { id: "camera", icon: "camera", label: "Cameră (din vederea curentă)" },
                                        { id: "point", icon: "lightbulb", label: "Lumină punctuală (bec)" },
                                        { id: "spot", icon: "light-spot", label: "Spot" },
                                        { id: "directional", icon: "sun", label: "Lumină direcțională" },
                                        { id: "area", icon: "light-area", label: "Panou luminos" }
                                    ]
                                    delegate: Rectangle {
                                        id: addItem
                                        required property var modelData
                                        width: 232
                                        height: Theme.controlHeightSm
                                        radius: Theme.radiusSm
                                        color: addItemArea.containsMouse ? Theme.bgHover : "transparent"
                                        Row {
                                            anchors.left: parent.left
                                            anchors.leftMargin: Theme.space2
                                            anchors.verticalCenter: parent.verticalCenter
                                            spacing: Theme.space2
                                            Icon { anchors.verticalCenter: parent.verticalCenter; name: addItem.modelData.icon; size: 14; color: Theme.textSecondary }
                                            Text {
                                                anchors.verticalCenter: parent.verticalCenter
                                                text: addItem.modelData.label
                                                color: Theme.textPrimary
                                                font.pixelSize: Theme.fontSizeSm
                                            }
                                        }
                                        MouseArea {
                                            id: addItemArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: {
                                                addObjectMenu.close()
                                                outliner.addObject(addItem.modelData.id)
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    Outliner {
                        id: outliner
                        anchors.fill: parent
                        document: doc
                        viewport: viewport
                    }
                }
                AccordionSection {
                    id: librarySection
                    visible: doc.workMode === 0 // după modul de lucru
                    width: parent.width
                    title: "Bibliotecă"
                    iconName: "book-open"
                    expanded: preferences.leftLibrary
                    bodyHeight: accordion.heightOf("library", 260)
                    onResizeRequested: (value) => accordion.setHeight("library", value)
                    onResetRequested: accordion.resetHeight("library")
                    onToggled: preferences.leftLibrary = !preferences.leftLibrary
                    actions: HeaderButton {
                        iconName: "plus"
                        tooltip: "Salvează obiectul selectat în bibliotecă"
                        onClicked: window.saveToLibrary()
                    }
                    LibraryPanel {
                        anchors.fill: parent
                        document: doc
                        onPlaceRequested: (id) => {
                            const c = viewport.cameraState()
                            doc.placeLibraryItem(id, Qt.vector3d(c.tx, c.ty, 0))
                        }
                        onDragStarted: (id, label, icon) => {
                            libraryGhost.itemId = id
                            libraryGhost.label = label
                            libraryGhost.icon = icon
                        }
                        onDragMoved: (p) => libraryGhost.position = p
                        onDragEnded: libraryGhost.finish()
                    }
                }
                AccordionSection {
                    id: tagsSection
                    visible: doc.workMode === 0 // după modul de lucru
                    width: parent.width
                    title: "Tag-uri"
                    iconName: "tag"
                    count: doc.tags.length
                    expanded: preferences.leftTags
                    bodyHeight: accordion.heightOf("tags", 160)
                    onResizeRequested: (value) => accordion.setHeight("tags", value)
                    onResetRequested: accordion.resetHeight("tags")
                    onToggled: preferences.leftTags = !preferences.leftTags
                    actions: HeaderButton {
                        iconName: "plus"
                        tooltip: "Tag nou"
                        onClicked: {
                            doc.addTag("Tag " + (doc.tags.length + 1))
                            preferences.leftTags = true
                        }
                    }
                    TagsPanel {
                        anchors.fill: parent
                        document: doc
                    }
                }
                AccordionSection {
                    id: materialsSection
                    visible: doc.workMode === 0 // după modul de lucru
                    width: parent.width
                    title: "Materiale"
                    iconName: "palette"
                    count: doc.modelMaterials.length
                    expanded: preferences.leftMaterials
                    bodyHeight: accordion.heightOf("materials", 300)
                    onResizeRequested: (value) => accordion.setHeight("materials", value)
                    onResetRequested: accordion.resetHeight("materials")
                    onToggled: preferences.leftMaterials = !preferences.leftMaterials
                    actions: HeaderButton {
                        iconName: "plus"
                        tooltip: "Material nou (personalizat)"
                        onClicked: {
                            preferences.leftMaterials = true
                            materialsPanel.newMaterial()
                        }
                    }
                    MaterialsPanel {
                        id: materialsPanel
                        anchors.fill: parent
                        document: doc
                        onPaintRequested: window.selectTool("paint")
                        onDragStarted: (id, label) => {
                            libraryGhost.itemId = id
                            libraryGhost.label = label
                            libraryGhost.icon = "paint-bucket"
                        }
                        onDragMoved: (p) => libraryGhost.position = p
                        onDragEnded: libraryGhost.finish()
                        onPickTextureImage: {
                            fileDialog.textureCallback = (path) => materialsPanel.textureChosen(path)
                            fileDialog.ask("textureImage")
                        }
                    }
                }
                AccordionSection {
                    id: scenesSection
                    visible: doc.workMode !== 2 // după modul de lucru
                    width: parent.width
                    title: "Scene"
                    iconName: "monitor"
                    count: doc.scenes.length
                    expanded: preferences.leftScenes
                    bodyHeight: accordion.heightOf("scenes", 200)
                    onResizeRequested: (value) => accordion.setHeight("scenes", value)
                    onResetRequested: accordion.resetHeight("scenes")
                    onToggled: preferences.leftScenes = !preferences.leftScenes
                    actions: [
                        HeaderButton {
                            id: sceneSettingsButton
                            iconName: "settings"
                            tooltip: "Setările scenelor (tranziția)"
                            onClicked: scenesPanel.openSettings(sceneSettingsButton)
                        },
                        HeaderButton {
                            iconName: "plus"
                            tooltip: "Scenă nouă din vederea curentă"
                            onClicked: {
                                doc.addScene(viewport.cameraState(), viewport.displayStyle)
                                preferences.leftScenes = true
                            }
                        }
                    ]
                    ScenesPanel {
                        id: scenesPanel
                        anchors.fill: parent
                        document: doc
                        viewport: viewport
                        transition: preferences.sceneTransition
                        onTransitionChanged: preferences.sceneTransition = transition
                    }
                }
                AccordionSection {
                    id: interactionsSection
                    visible: doc.workMode === 2 // după modul de lucru
                    width: parent.width
                    title: "Interacțiuni"
                    iconName: "hand-pointer"
                    count: doc.interactions.length
                    expanded: preferences.leftInteractions
                    bodyHeight: accordion.heightOf("interactions", 200)
                    onResizeRequested: (value) => accordion.setHeight("interactions", value)
                    onResetRequested: accordion.resetHeight("interactions")
                    onToggled: preferences.leftInteractions = !preferences.leftInteractions
                    actions: [
                        HeaderButton {
                            iconName: "play"
                            tooltip: "Folosește interacțiunile (Interact, F5)"
                            checked: window.activeTool === "interact"
                            onClicked: window.runCommand("mode.interaction")
                        },
                        HeaderButton {
                            iconName: "plus"
                            tooltip: "Interacțiune nouă pentru obiectul selectat"
                            onClicked: window.openRulesEditor()
                        }
                    ]
                    InteractionsPanel {
                        anchors.fill: parent
                        document: doc
                        onEditRequested: (id, name) => rulesEditor.openFor(id, name)
                    }
                }
            }
        }

        // Bara de scroll a panoului (trasă sau cu rotița), vizibilă doar când secțiunile nu încap.
        Rectangle {
            id: accordionScrollbar
            visible: accordion.scrollable
            anchors.right: parent.right
            anchors.rightMargin: 1
            y: accordion.y + accordion.visibleArea.yPosition * accordion.height
            width: 6
            height: Math.max(24, accordion.visibleArea.heightRatio * accordion.height)
            radius: 3
            color: scrollbarArea.pressed || scrollbarArea.containsMouse ? Theme.textTertiary : Theme.borderStrong
            MouseArea {
                id: scrollbarArea
                anchors.fill: parent
                anchors.margins: -3
                hoverEnabled: true
                preventStealing: true
                property real startY: 0
                property real startContentY: 0
                onPressed: (mouse) => { startY = mapToItem(accordion, 0, mouse.y).y; startContentY = accordion.contentY }
                onPositionChanged: (mouse) => {
                    if (!pressed)
                        return
                    const dy = mapToItem(accordion, 0, mouse.y).y - startY
                    const range = accordion.contentHeight - accordion.height
                    const track = accordion.height - accordionScrollbar.height
                    accordion.contentY = Math.max(0, Math.min(range, startContentY + dy * (track > 0 ? range / track : 0)))
                }
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
        environment: preferences.environment
        ambientOcclusion: preferences.ambientOcclusion
        progressiveAntialiasing: preferences.progressiveAA
        antialiasing: preferences.msaa
        shadowQuality: preferences.shadowQuality
        onFramesPerSecondChanged: if (AppInfo.startupActions.length > 0) console.info("fps:", framesPerSecond)
        anchors.top: titleBar.bottom
        anchors.bottom: timeline.visible ? timeline.top : statusBar.top
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
            anchors.topMargin: 56 // sub bara de vizualizare din colț
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

        // Bara de redare a animației scenelor (nu apare în cadrele exportate).
        Rectangle {
            visible: viewport.scenesPlaying
            anchors.top: parent.top
            anchors.topMargin: 56 // sub bara de vizualizare din colț
            anchors.horizontalCenter: parent.horizontalCenter
            width: playerRow.implicitWidth + Theme.space3 * 2
            height: 36
            radius: 18
            color: Theme.bgElevated
            border.color: Theme.borderStrong
            Row {
                id: playerRow
                anchors.centerIn: parent
                spacing: Theme.space2
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    name: "play"
                    size: 14
                    color: Theme.accent
                }
                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    text: viewport.playingScene >= 0 && viewport.playingScene < doc.scenes.length
                          ? doc.scenes[viewport.playingScene].name + "  ·  " + (viewport.playingScene + 1) + " / " + doc.scenes.length
                          : ""
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeSm
                }
                IconButton {
                    anchors.verticalCenter: parent.verticalCenter
                    width: 26
                    height: 26
                    iconSize: Theme.iconSizeSm
                    iconName: "x"
                    tooltip: "Oprește animația (sau click în viewport)"
                    onClicked: viewport.stopScenes()
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

        // Interact: un click pe un obiect fără interacțiuni arată un mesaj lângă cursor.
        Item {
            id: noInteractionAnchor
            parent: viewport
            width: 1
            height: 1
            z: 6
            Tooltip {
                id: noInteractionTip
                delay: 0
                timeout: 2600
                text: "Obiectul nu are interacțiuni. Alege Select și adaugă una din Inspector."
            }
        }
        Connections {
            target: viewport
            function onNoInteractionAt(position) {
                noInteractionAnchor.x = position.x
                noInteractionAnchor.y = position.y
                noInteractionTip.close()
                // După click (deschis în timpul lui, s-ar închide la „release în afara părintelui”).
                Qt.callLater(() => noInteractionTip.open())
            }
        }

        ViewportControls {
            anchors.top: parent.top
            anchors.topMargin: Theme.space3
            anchors.right: gizmo.left
            anchors.rightMargin: Theme.space1
            viewport: viewport
            document: doc
            onEnvironmentChosen: (key) => preferences.environment = key
        }

        NavigationGizmo {
            id: gizmo
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
            keyOverrides: window.shortcutOverrides
            shortcutOwners: window.shortcutOwners
            workMode: doc.workMode
            shortcutFor: (id) => { const c = window.commands.find(x => x.id === id); return c ? c.shortcut : "" }
            onModeSelected: (m) => window.setWorkMode(m)
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

            ObjectInfo {
                width: parent.width
                document: doc
                viewport: viewport
            }
            EntityInfo {
                visible: doc.selectedObject.kind === "none"
                width: parent.width
                document: doc
                timelineActive: window.timelineVisible
                workMode: doc.workMode
                onEditRules: window.openRulesEditor()
                onToolRequested: (id) => window.selectTool(id)
            }

            // Fără selecție, Inspector-ul descrie modelul întreg.
            Section {
                visible: doc.entityInfo.kind === "none" && doc.selectedObject.kind === "none"
                width: parent.width
                title: "Model"
                Text {
                    text: (doc.faceCount === 1 ? "1 față" : doc.faceCount + " fețe") + " · "
                          + (doc.edgeCount === 1 ? "1 muchie" : doc.edgeCount + " muchii")
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontSizeSm
                    font.family: Theme.fontMono
                }
                Text {
                    width: parent.width
                    wrapMode: Text.Wrap
                    text: "Selectează ceva ca să-i vezi proprietățile. Stilul, umbrele, secțiunile și proiecția sunt "
                          + "în bara din colțul viewport-ului."
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSizeXs
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

    // Timeline-ul animațiilor, sub viewport (⌥T).
    Timeline {
        id: timeline
        visible: window.timelineVisible
        anchors.bottom: statusBar.top
        anchors.left: leftPanel.right
        anchors.right: rightPanel.left
        height: 200
        document: doc
        viewport: viewport
        onExportRequested: fileDialog.ask("exportVideo")
    }

    StatusBar {
        id: statusBar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        framesPerSecond: UserPrefs.showFps ? viewport.framesPerSecond : 0
        // Modul de lucru, apoi indicația uneltei.
        message: ["Modelare", "Animare", "Interacțiune"][doc.workMode] + "  ·  "
               + (window.pendingToolMessage.length > 0 ? window.pendingToolMessage
                  : doc.toolStatus.length > 0 ? doc.toolStatus : "Gata")
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
        // Doar comenzile modului de lucru curent (plus cele comune).
        commands: (doc.workMode, window.commands.filter(c => doc.commandAllowed(c.id)))
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
