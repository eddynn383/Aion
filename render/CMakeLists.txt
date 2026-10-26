add_library(aion_render STATIC
    include/aion/render/Document.h
    include/aion/render/GraphicsApi.h
    include/aion/render/Viewport.h
    src/Document.cpp
    src/GraphicsApi.cpp
    src/Viewport.cpp
    src/ViewportFrame.h
    src/ViewportRenderer.cpp
    src/ViewportRenderer.h
)
add_library(aion::render ALIAS aion_render)

target_include_directories(aion_render PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Fișierul generat pentru înregistrarea tipurilor QML include header-ele după nume („Viewport.h”).
target_include_directories(aion_render PRIVATE include/aion/render)

# QRhi e un API „semi-public”: header-ele lui vin cu ținta GuiPrivate.
target_link_libraries(aion_render
    PUBLIC aion::core Qt6::Gui Qt6::Quick
    PRIVATE Qt6::GuiPrivate
)

# Viewport-ul se înregistrează în QML ca modulul Aion.Render.
qt_add_qml_module(aion_render
    URI Aion.Render
    VERSION 1.0
    NO_PLUGIN_OPTIONAL
)

# Shaderele se compilează la build (qsb) pentru Metal, Direct3D, Vulkan și OpenGL / WebGL 2.
qt_add_shaders(aion_render "aion_render_shaders"
    PREFIX "/aion/shaders"
    BASE shaders
    GLSL "300es,330"
    HLSL 50
    MSL 12
    FILES
        shaders/fullscreen.vert
        shaders/background.frag
        shaders/grid.frag
        shaders/line.vert
        shaders/line.frag
        shaders/point.vert
        shaders/point.frag
        shaders/mesh.vert
        shaders/mesh.frag
)

aion_configure_target(aion_render)
