#pragma once

#include <QSGRendererInterface>
#include <QString>

namespace forma::render {

// Alege backend-ul grafic potrivit platformei (Metal pe macOS, Direct3D pe Windows).
// Trebuie apelat înainte de a crea primul QQuickWindow.
void selectPreferredGraphicsApi();

// Numele lizibil al unui backend, pentru status bar și rapoarte de eroare.
QString graphicsApiName(QSGRendererInterface::GraphicsApi api);

} // namespace forma::render
