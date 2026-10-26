option(AION_BUILD_APP "Compilează aplicația Qt (render + app)" ON)
option(AION_BUILD_TESTS "Compilează testele" ON)
option(AION_ENABLE_SANITIZERS "Activează AddressSanitizer și UndefinedBehaviorSanitizer" OFF)
option(AION_WARNINGS_AS_ERRORS "Tratează warning-urile ca erori" OFF)
# WebAssembly: excepțiile JavaScript (ca Qt for WebAssembly precompilat) în loc de cele native.
option(AION_WASM_JS_EXCEPTIONS "Emscripten: -fexceptions în loc de -fwasm-exceptions (aplicația web cu Qt)" OFF)

# Pe unele combinații compilator + sistem, runtime-ul ASan se blochează încă de la pornire
# (de exemplu Apple clang 17 pe macOS 26: deadlock în AsanInitInternal). Verificăm o dată,
# compilând și rulând un program minim cu limită de timp; dacă nu merge, rămânem doar cu UBSan.
function(aion_check_asan_works)
    if(DEFINED CACHE{AION_ASAN_WORKS})
        return()
    endif()
    set(dir "${CMAKE_BINARY_DIR}/CMakeFiles/aion_asan_check")
    file(WRITE "${dir}/main.cpp" "int main() { return 0; }\n")
    try_compile(compiled "${dir}/build" "${dir}/main.cpp"
        COMPILE_DEFINITIONS -fsanitize=address
        LINK_OPTIONS -fsanitize=address
        COPY_FILE "${dir}/asan_check"
    )
    set(works OFF)
    if(compiled)
        execute_process(COMMAND "${dir}/asan_check" RESULT_VARIABLE result TIMEOUT 10)
        if(result STREQUAL "0")
            set(works ON)
        endif()
    endif()
    if(NOT works)
        message(WARNING "AddressSanitizer nu funcționează cu acest compilator/sistem "
                        "(programul de test nu a pornit). Continuăm doar cu UBSan.")
    endif()
    set(AION_ASAN_WORKS ${works} CACHE INTERNAL "ASan rulează pe această mașină")
endfunction()

if(AION_ENABLE_SANITIZERS AND NOT MSVC)
    aion_check_asan_works()
    if(AION_ASAN_WORKS)
        set(AION_SANITIZER_FLAGS -fsanitize=address,undefined)
    else()
        set(AION_SANITIZER_FLAGS -fsanitize=undefined)
    endif()
endif()

# Setări comune pentru toate țintele noastre (nu și pentru dependențe).
function(aion_configure_target target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus)
        # getenv, fopen etc. sunt „nesigure” pentru MSVC; le folosim corect, fără variantele _s nestandard.
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
        if(AION_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
            -Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual
        )
        if(AION_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    if(AION_SANITIZER_FLAGS)
        target_compile_options(${target} PRIVATE ${AION_SANITIZER_FLAGS} -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE ${AION_SANITIZER_FLAGS})
    endif()

    # WebAssembly: excepții native (ca dependențele din tripletul nostru) și memorie care poate crește.
    if(EMSCRIPTEN AND AION_WASM_JS_EXCEPTIONS)
        # Aplicația web: Qt își pune singur opțiunile de memorie și de rulare.
        target_compile_options(${target} PRIVATE -fexceptions)
        target_link_options(${target} PRIVATE -fexceptions)
    elseif(EMSCRIPTEN)
        target_compile_options(${target} PRIVATE -fwasm-exceptions)
        target_link_options(${target} PRIVATE -fwasm-exceptions -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=4MB
            -sEXIT_RUNTIME=1)
    endif()
endfunction()
