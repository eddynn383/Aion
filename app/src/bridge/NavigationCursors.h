#pragma once

class QQuickWindow;

// Cursoarele pentru navigarea în viewport (orbit, zoom), desenate din iconițele Lucide.
// Le setează pe toate Viewport-urile din fereastră.
void installNavigationCursors(QQuickWindow* window);
