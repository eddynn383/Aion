add_library(forma_render STATIC
    include/forma/render/GraphicsApi.h
    include/forma/render/Viewport.h
    src/GraphicsApi.cpp
    src/Viewport.cpp
    src/ViewportFrame.h
    src/ViewportRenderer.cpp
    src/ViewportRenderer.h
)
add_library(forma::render ALIAS forma_render)

target_include_directories(forma_render PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

# Fișierul generat pentru înregistrarea tipurilor QML include header-ele după nume („Viewport.h”).
target_include_directories(forma_render PRIVATE include/forma/render)

# QRhi e un API „semi-public”: header-ele lui vin cu ținta GuiPrivate.
target_link_libraries(forma_render
    PUBLIC forma::core Qt6::Gui Qt6::Quick
    PRIVATE Qt6::GuiPrivate
)

# Viewport-ul se înregistrează în QML ca modulul Forma.Render.
qt_add_qml_module(forma_render
    URI Forma.Render
    VERSION 1.0
    NO_PLUGIN_OPTIONAL
)

# Shaderele se compilează la build (qsb) pentru Metal, Direct3D, Vulkan și OpenGL / WebGL 2.
qt_add_shaders(forma_render "forma_render_shaders"
    PREFIX "/forma/shaders"
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

forma_configure_target(forma_render)
