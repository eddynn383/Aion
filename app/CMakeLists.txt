qt_add_executable(aion_app
    src/main.cpp
    src/AppInfo.cpp
    src/AppInfo.h
    src/bridge/CommandSearch.cpp
    src/bridge/CommandSearch.h
    src/bridge/IconProvider.cpp
    src/bridge/IconProvider.h
    src/bridge/NavigationCursors.cpp
    src/bridge/NavigationCursors.h
    src/platform/Platform.h
)

if(APPLE)
    enable_language(OBJCXX)
    target_sources(aion_app PRIVATE src/platform/PlatformMac.mm)
    target_link_libraries(aion_app PRIVATE "-framework AppKit")
else()
    target_sources(aion_app PRIVATE src/platform/PlatformOther.cpp)
endif()

set_target_properties(aion_app PROPERTIES
    OUTPUT_NAME Aion
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME Aion
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.aion.Aion
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    WIN32_EXECUTABLE ON
)

# Header-ele cu QML_ELEMENT trebuie găsite prin include path de fișierul de înregistrare generat.
target_include_directories(aion_app PRIVATE src src/bridge)

# Fiecare subfolder cu fișiere .qml primește propriul qmldir.
qt_policy(SET QTP0004 NEW)

# Trebuie setat înainte de qt_add_qml_module, altfel Theme nu apare ca singleton în qmldir.
set_source_files_properties(qml/theme/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(aion_app
    URI Aion
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/components/Button.qml
        qml/components/Icon.qml
        qml/components/IconButton.qml
        qml/components/Input.qml
        qml/components/Kbd.qml
        qml/components/Menu.qml
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
        qml/shell/ToolGroupButton.qml
        qml/shell/Toolbar.qml
        qml/shell/Outliner.qml
        qml/shell/TagsPanel.qml
        qml/shell/EntityInfo.qml
        qml/overlays/CommandPalette.qml
        qml/overlays/ViewportAnnotations.qml
        qml/overlays/MeasurementsBox.qml
        qml/storybook/Storybook.qml
)

# Fonturile (OFL) și iconițele Lucide (ISC), incluse în aplicație.
file(GLOB AION_FONT_FILES "${PROJECT_SOURCE_DIR}/resources/fonts/*.ttf")
file(GLOB AION_ICON_FILES "${PROJECT_SOURCE_DIR}/resources/icons/*.svg")
qt_add_resources(aion_app "aion_assets"
    PREFIX "/aion"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES ${AION_FONT_FILES} ${AION_ICON_FILES} "${PROJECT_SOURCE_DIR}/resources/branding/aion-icon.svg"
)

# Iconița aplicației: generată la compilare din SVG-urile din resources/branding (fără imagini binare în repo).
qt_add_executable(aion_iconrender "${PROJECT_SOURCE_DIR}/tools/iconrender/main.cpp")
set_target_properties(aion_iconrender PROPERTIES MACOSX_BUNDLE OFF)
target_link_libraries(aion_iconrender PRIVATE Qt6::Gui Qt6::Svg)
if(APPLE)
    set(AION_ICON_BIG "${PROJECT_SOURCE_DIR}/resources/branding/aion-icon.svg")
    set(AION_ICON_SMALL "${PROJECT_SOURCE_DIR}/resources/branding/aion-icon-small.svg")
    set(AION_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/icon")
    add_custom_command(
        OUTPUT "${AION_ICON_DIR}/Aion.icns"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${AION_ICON_DIR}/Aion.iconset"
        COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen
                $<TARGET_FILE:aion_iconrender> "${AION_ICON_BIG}" "${AION_ICON_SMALL}" "${AION_ICON_DIR}/Aion.iconset"
        COMMAND iconutil -c icns -o "${AION_ICON_DIR}/Aion.icns" "${AION_ICON_DIR}/Aion.iconset"
        DEPENDS aion_iconrender "${AION_ICON_BIG}" "${AION_ICON_SMALL}"
        COMMENT "Iconița aplicației (Aion.icns)"
        VERBATIM
    )
    target_sources(aion_app PRIVATE "${AION_ICON_DIR}/Aion.icns")
    set_source_files_properties("${AION_ICON_DIR}/Aion.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    set_target_properties(aion_app PROPERTIES MACOSX_BUNDLE_ICON_FILE Aion.icns)
endif()

target_link_libraries(aion_app PRIVATE
    aion::core
    aion::render
    aion_renderplugin
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Svg
)

aion_configure_target(aion_app)

install(TARGETS aion_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
