add_library(forma_render STATIC
    src/GraphicsApi.cpp
)
add_library(forma::render ALIAS forma_render)

target_include_directories(forma_render PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(forma_render
    PUBLIC forma::core Qt6::Gui Qt6::Quick
)

forma_configure_target(forma_render)
