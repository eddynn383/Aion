qt_add_executable(forma_app
    src/main.cpp
    src/AppInfo.cpp
    src/AppInfo.h
)

set_target_properties(forma_app PROPERTIES
    OUTPUT_NAME Forma
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME Forma
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.forma.Forma
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    WIN32_EXECUTABLE ON
)

# Header-ele cu QML_ELEMENT trebuie găsite prin include path de fișierul de înregistrare generat.
target_include_directories(forma_app PRIVATE src)

# Fiecare subfolder cu fișiere .qml primește propriul qmldir.
qt_policy(SET QTP0004 NEW)

# Trebuie setat înainte de qt_add_qml_module, altfel Theme nu apare ca singleton în qmldir.
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(forma_app
    URI Forma
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/IconButton.qml
        qml/components/Panel.qml
        qml/shell/TitleBar.qml
        qml/shell/StatusBar.qml
        qml/shell/Toolbar.qml
)

target_link_libraries(forma_app PRIVATE
    forma::core
    forma::render
    Qt6::Quick
)

forma_configure_target(forma_app)

install(TARGETS forma_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
