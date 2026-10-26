import QtQuick
import Qt.labs.platform as Platform
import Aion

// Meniul nativ macOS (bara de sus), construit din aceeași listă de comenzi ca și command palette.
// Scurtăturile cu modificatori (⌘…) sunt ale meniului; Main.qml dezactivează Shortcut-urile QML corespunzătoare
// ca să nu ruleze de două ori. „Despre” și „Preferințe” ajung automat în meniul aplicației (rolurile native).
Platform.MenuBar {
    id: root

    property var host // fereastra principală (commands, runCommand)
    property var document

    signal aboutRequested()
    signal preferencesRequested()

    // Pentru verificări automate (AION_STARTUP="debug.menus"): textele din meniuri, în consolă.
    function dump() {
        for (const menu of root.menus) {
            const titles = []
            for (const item of menu.items)
                titles.push(item.separator ? "—" : (item.subMenu ? item.subMenu.title + " ›" : item.text + (item.shortcut ? " [" + item.shortcut + "]" : "")))
            console.log("Meniu " + menu.title + ": " + titles.join(" | "))
        }
    }

    function command(id) {
        const list = root.host ? root.host.commands : []
        for (let i = 0; i < list.length; ++i) {
            if (list[i].id === id)
                return list[i]
        }
        return null
    }

    // Componentele inline nu văd id-urile din fișier (root), deci bara se transmite explicit (`bar`).
    component CommandItem: Platform.MenuItem {
        property var bar
        property string commandId
        readonly property var info: bar ? bar.command(commandId) : null
        separator: commandId === "-"
        text: info ? info.title : ""
        shortcut: info && info.shortcut.indexOf("Ctrl+") >= 0 ? info.shortcut : ""
        onTriggered: bar.host.runCommand(commandId)
    }

    component CommandMenu: Platform.Menu {
        id: menu
        property var bar
        property var commandIds: []
        Instantiator {
            model: menu.commandIds
            delegate: CommandItem {
                required property string modelData
                bar: menu.bar
                commandId: modelData
            }
            onObjectAdded: (index, object) => menu.insertItem(index, object)
            onObjectRemoved: (index, object) => menu.removeItem(object)
        }
    }

    Platform.Menu {
        title: "Aion"
        Platform.MenuItem {
            text: "Despre Aion"
            role: Platform.MenuItem.AboutRole
            onTriggered: root.aboutRequested()
        }
        Platform.MenuItem {
            text: "Preferințe…"
            shortcut: StandardKey.Preferences
            role: Platform.MenuItem.PreferencesRole
            onTriggered: root.preferencesRequested()
        }
    }

    CommandMenu {
        bar: root
        id: fileMenu
        title: "Fișier"
        commandIds: ["file.new", "file.open", "-", "file.save", "file.saveAs", "-", "file.import",
                     "file.exportObj", "file.exportGlb", "file.exportGltf", "file.exportPng", "-"]

        // Rămâne la sfârșitul meniului: comenzile se inserează înaintea lui.
        Platform.Menu {
            id: recentMenu
            title: "Deschise recent"
            enabled: root.document !== null && root.document.recentFiles.length > 0
            Instantiator {
                model: root.document ? root.document.recentFiles : []
                delegate: Platform.MenuItem {
                    required property string modelData
                    required property int index
                    text: modelData.split("/").pop()
                    onTriggered: root.host.runCommand("file.recent:" + index)
                }
                onObjectAdded: (index, object) => recentMenu.insertItem(index, object)
                onObjectRemoved: (index, object) => recentMenu.removeItem(object)
            }
        }
    }
    CommandMenu {
        bar: root
        title: "Editare"
        commandIds: ["edit.undo", "edit.redo", "-", "edit.delete", "edit.selectAll", "edit.selectNone",
                     "edit.selectConnected", "-", "edit.reverseFaces", "edit.intersectModel", "edit.intersectSelection",
                     "guides.clear"]
    }
    CommandMenu {
        bar: root
        title: "Vedere"
        commandIds: ["view.iso", "view.top", "view.front", "view.right", "view.back", "view.left", "view.bottom", "-",
                     "view.extents", "view.zoomSelection", "view.projection", "view.grid", "-",
                     "ui.leftPanel", "ui.rightPanel", "ui.panels", "ui.theme"]
    }
    CommandMenu {
        bar: root
        title: "Stil"
        commandIds: ["style.shaded", "style.clay", "style.hiddenline", "style.wireframe", "style.xray", "-",
                     "style.shadows", "style.profiles", "style.fog"]
    }
    CommandMenu {
        bar: root
        title: "Organizare"
        commandIds: ["org.makeGroup", "org.makeComponent", "org.explode", "org.makeUnique", "-",
                     "org.enter", "org.exit", "-", "org.hide", "org.unhideAll", "org.lock", "org.unlock", "-",
                     "solid.union", "solid.subtract", "solid.intersect"]
    }
    CommandMenu {
        bar: root
        title: "Unelte"
        commandIds: (root.host ? root.host.commands : []).filter(c => c.id.startsWith("tool.")).map(c => c.id)
    }
    Platform.Menu {
        title: "Ajutor"
        Platform.MenuItem {
            text: "Turul uneltelor"
            onTriggered: root.host.runCommand("app.tour")
        }
        Platform.MenuItem {
            text: "Caută comenzi…"
            onTriggered: root.host.openPalette()
        }
        Platform.MenuItem {
            text: "Despre Aion"
            onTriggered: root.aboutRequested()
        }
    }
}
