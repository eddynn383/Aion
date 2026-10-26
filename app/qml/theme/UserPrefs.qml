pragma Singleton

import QtQuick
import QtCore

// Preferințele de interfață care trebuie citite din mai multe componente (păstrate între sesiuni).
Settings {
    category: "ui"

    // Ghidurile animate la hover lung pe unelte.
    property bool hoverGuides: true
}
