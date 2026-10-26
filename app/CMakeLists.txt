qt_add_executable(forma_app
    src/main.cpp
    src/AppInfo.cpp
    src/AppInfo.h
    src/bridge/CommandSearch.cpp
    src/bridge/CommandSearch.h
    src/bridge/IconProvider.cpp
    src/bridge/IconProvider.h
    src/platform/Platform.h
)

if(APPLE)
    enable_language(OBJCXX)
    target_sources(forma_app PRIVATE src/platform/PlatformMac.mm)
    target_link_libraries(forma_app PRIVATE "-framework AppKit")
else()
    target_sources(forma_app PRIVATE src/platform/PlatformOther.cpp)
endif()

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
target_include_directories(forma_app PRIVATE src src/bridge)

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
        qml/components/Button.qml
        qml/components/Icon.qml
        qml/components/IconButton.qml
        qml/components/Input.qml
        qml/components/Kbd.qml
        qml/components/NumberField.qml
        qml/components/Panel.qml
        qml/components/Popover.qml
        qml/components/Section.qml
        qml/components/Separator.qml
        qml/components/Tabs.qml
        qml/components/Toggle.qml
        qml/components/Tooltip.qml
        qml/shell/NavigationGizmo.qml
        qml/shell/StatusBar.qml
        qml/shell/TitleBar.qml
        qml/shell/Toolbar.qml
        qml/overlays/CommandPalette.qml
        qml/storybook/Storybook.qml
)

# Fonturile (OFL) și iconițele Lucide (ISC), incluse în aplicație.
file(GLOB FORMA_FONT_FILES "${PROJECT_SOURCE_DIR}/resources/fonts/*.ttf")
file(GLOB FORMA_ICON_FILES "${PROJECT_SOURCE_DIR}/resources/icons/*.svg")
qt_add_resources(forma_app "forma_assets"
    PREFIX "/forma"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES ${FORMA_FONT_FILES} ${FORMA_ICON_FILES}
)

target_link_libraries(forma_app PRIVATE
    forma::core
    forma::render
    forma_renderplugin
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
)

forma_configure_target(forma_app)

install(TARGETS forma_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
