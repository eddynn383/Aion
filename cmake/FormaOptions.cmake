option(FORMA_BUILD_APP "Compilează aplicația Qt (render + app)" ON)
option(FORMA_BUILD_TESTS "Compilează testele" ON)
option(FORMA_ENABLE_SANITIZERS "Activează AddressSanitizer și UndefinedBehaviorSanitizer" OFF)
option(FORMA_WARNINGS_AS_ERRORS "Tratează warning-urile ca erori" OFF)

# Setări comune pentru toate țintele noastre (nu și pentru dependențe).
function(forma_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        if(FORMA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
        )
        if(FORMA_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(FORMA_ENABLE_SANITIZERS AND NOT MSVC)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()
