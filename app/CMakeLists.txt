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
    src/bridge/WebFiles.cpp
    src/bridge/WebFiles.h
    src/platform/Platform.h
    src/platform/Video.h
)

if(APPLE)
    enable_language(OBJCXX)
    target_sources(aion_app PRIVATE src/platform/PlatformMac.mm src/platform/VideoMac.mm)
    set_source_files_properties(src/platform/VideoMac.mm PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_link_libraries(aion_app PRIVATE "-framework AppKit" "-framework AVFoundation" "-framework CoreVideo"
        "-framework CoreMedia")
else()
    target_sources(aion_app PRIVATE src/platform/PlatformOther.cpp src/platform/VideoOther.cpp)
endif()

set_target_properties(aion_app PROPERTIES
    OUTPUT_NAME Aion
    MACOSX_BUNDLE ON
    MACOSX_BUNDLE_BUNDLE_NAME Aion
    MACOSX_BUNDLE_GUI_IDENTIFIER dev.aion.Aion
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    # Asocierea cu fișierele .aion (CFBundleDocumentTypes, UTExportedTypeDeclarations).
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/macos/Info.plist.in
    WIN32_EXECUTABLE ON
)

# Header-ele cu QML_ELEMENT trebuie găsite prin include path de fișierul de înregistrare generat.
target_include_directories(aion_app PRIVATE src src/bridge)

# Fiecare subfolder cu fișiere .qml primește propriul qmldir.
qt_policy(SET QTP0004 NEW)

# Trebuie setat înainte de qt_add_qml_module, altfel Theme nu apare ca singleton în qmldir.
set_source_files_properties(qml/theme/Theme.qml qml/theme/UserPrefs.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
# Web: Main.qml (mare) nu trece prin compilatorul QML → C++ (qmlcachegen): pe runner-ele CI dura ~2 minute și uneori
# era oprit (exit 143). Se compilează în browser la pornire; restul fișierelor rămân compilate dinainte.
if(EMSCRIPTEN)
    set_source_files_properties(qml/Main.qml PROPERTIES QT_QML_SKIP_CACHEGEN TRUE)
endif()

qt_add_qml_module(aion_app
    URI Aion
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/theme/Theme.qml
        qml/theme/UserPrefs.qml
        qml/components/Button.qml
        qml/components/Icon.qml
        qml/components/IconButton.qml
        qml/components/Input.qml
        qml/components/Kbd.qml
        qml/components/ShortcutRecorder.qml
        qml/components/SettingsGroup.qml
        qml/components/SettingsRow.qml
        qml/components/Menu.qml
        qml/components/ContextMenu.qml
        qml/components/Slider.qml
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
        qml/shell/ModeSwitch.qml
        qml/shell/Outliner.qml
        qml/shell/EntityInfo.qml
        qml/shell/MaterialsPanel.qml
        qml/shell/ScenesPanel.qml
        qml/overlays/CommandPalette.qml
        qml/overlays/ViewportAnnotations.qml
        qml/overlays/MeasurementsBox.qml
        qml/overlays/PreferencesPage.qml
        qml/overlays/AboutDialog.qml
        qml/overlays/StartScreen.qml
        qml/overlays/ToolTour.qml
        qml/overlays/QuantitiesDialog.qml
        qml/shell/ObjectInfo.qml
        qml/shell/TagsPanel.qml
        qml/shell/LibraryPanel.qml
        qml/shell/InteractionsPanel.qml
        qml/overlays/ArrayPanel.qml
        qml/overlays/SoftenPanel.qml
        qml/overlays/SubdividePanel.qml
        qml/overlays/BevelPanel.qml
        qml/overlays/ScriptConsole.qml
        qml/overlays/RulesEditor.qml
        qml/overlays/RulesActionBlock.qml
        qml/overlays/RulesActionList.qml
        qml/components/DataTable.qml
        qml/components/MaterialSwatch.qml
        qml/components/AccordionSection.qml
        qml/components/RowAction.qml
        qml/components/PropertyGroup.qml
        qml/components/ToolDemo.qml
        qml/components/GuideCard.qml
        qml/components/RichTooltip.qml
        qml/shell/ToolGuides.js
        qml/shell/AppMenuBar.qml
        qml/shell/ViewportControls.qml
        qml/shell/Timeline.qml
        qml/shell/CameraFrame.qml
        qml/overlays/EasingEditor.qml
        qml/components/EasingCurve.qml
        qml/components/EasingMath.js
        qml/components/MathExpr.js
        qml/shell/TimelineKey.qml
        qml/shell/TimelineRow.qml
        qml/shell/TimelineRuler.qml
        qml/storybook/Storybook.qml
        qml/storybook/DemoGallery.qml
)

# Fonturile (OFL) și iconițele Lucide (ISC), incluse în aplicație.
file(GLOB AION_FONT_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/resources/fonts/*.ttf")
file(GLOB AION_ICON_FILES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/resources/icons/*.svg")
qt_add_resources(aion_app "aion_assets"
    PREFIX "/aion"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES ${AION_FONT_FILES} ${AION_ICON_FILES} "${PROJECT_SOURCE_DIR}/resources/branding/aion-icon.svg"
)

# Iconița aplicației: generată la compilare din SVG-urile din resources/branding (fără imagini binare în repo).
# (Pe web nu e nevoie: iconița paginii e SVG-ul, iar unealta n-ar putea rula la build.)
if(NOT EMSCRIPTEN)
    qt_add_executable(aion_iconrender "${PROJECT_SOURCE_DIR}/tools/iconrender/main.cpp")
    set_target_properties(aion_iconrender PROPERTIES MACOSX_BUNDLE OFF)
    target_link_libraries(aion_iconrender PRIVATE Qt6::Gui Qt6::Svg)
endif()
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

    # macOS ține minte iconița unei aplicații după cale; fără reînregistrare, Dock-ul și Finder-ul pot arăta
    # în continuare iconița generică dintr-o compilare mai veche.
    set(AION_LSREGISTER
        "/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister")
    if(EXISTS "${AION_LSREGISTER}")
        add_custom_command(TARGET aion_app POST_BUILD
            COMMAND touch "$<TARGET_BUNDLE_DIR:aion_app>"
            COMMAND "${AION_LSREGISTER}" -f "$<TARGET_BUNDLE_DIR:aion_app>"
            VERBATIM
        )
    endif()
endif()

if(WIN32)
    # Iconița din Explorer și din bara de activități: un .ico generat din aceleași SVG-uri, legat printr-un .rc.
    set(AION_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/icon")
    add_custom_command(
        OUTPUT "${AION_ICON_DIR}/Aion.ico"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${AION_ICON_DIR}"
        COMMAND ${CMAKE_COMMAND} -E env QT_QPA_PLATFORM=offscreen
                $<TARGET_FILE:aion_iconrender> "${PROJECT_SOURCE_DIR}/resources/branding/aion-icon.svg"
                "${PROJECT_SOURCE_DIR}/resources/branding/aion-icon-small.svg" "${AION_ICON_DIR}/Aion.ico"
        DEPENDS aion_iconrender
        COMMENT "Iconița aplicației (Aion.ico)"
        VERBATIM
    )
    file(WRITE "${AION_ICON_DIR}/Aion.rc" "IDI_ICON1 ICON \"Aion.ico\"\n")
    target_sources(aion_app PRIVATE "${AION_ICON_DIR}/Aion.rc" "${AION_ICON_DIR}/Aion.ico")
endif()

if(EMSCRIPTEN)
    # QFileDialog::getOpenFileContent / saveFileContent (selectorul și download-ul browserului).
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    target_link_libraries(aion_app PRIVATE Qt6::Widgets)
endif()

target_link_libraries(aion_app PRIVATE
    aion::core
    aion::render
    aion_renderplugin
    Qt6::Quick
    Qt6::GuiPrivate
    Qt6::QuickControls2
    Qt6::Svg
)

aion_configure_target(aion_app)

install(TARGETS aion_app
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
