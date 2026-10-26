pragma Singleton

import QtQuick
import QtCore

// Preferințele de interfață care trebuie citite din mai multe componente (păstrate între sesiuni).
Settings {
    category: "ui"

    // Ghidurile animate la hover lung pe unelte.
    property bool hoverGuides: true
    // fps-ul în bara de jos.
    property bool showFps: true
    // Tema personalizată: culorile schimbate, separat pentru dark și light (JSON: {"dark": {"accent": "#…"}, …}).
    property string themeOverrides: "{}"
}
