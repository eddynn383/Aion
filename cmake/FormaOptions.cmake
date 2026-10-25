option(FORMA_BUILD_APP "Compilează aplicația Qt (render + app)" ON)
option(FORMA_BUILD_TESTS "Compilează testele" ON)
option(FORMA_ENABLE_SANITIZERS "Activează AddressSanitizer și UndefinedBehaviorSanitizer" OFF)
option(FORMA_WARNINGS_AS_ERRORS "Tratează warning-urile ca erori" OFF)

# Pe unele combinații compilator + sistem, runtime-ul ASan se blochează încă de la pornire
# (de exemplu Apple clang 17 pe macOS 26: deadlock în AsanInitInternal). Verificăm o dată,
# compilând și rulând un program minim cu limită de timp; dacă nu merge, rămânem doar cu UBSan.
function(forma_check_asan_works)
    if(DEFINED CACHE{FORMA_ASAN_WORKS})
        return()
    endif()
    set(dir "${CMAKE_BINARY_DIR}/CMakeFiles/forma_asan_check")
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
    set(FORMA_ASAN_WORKS ${works} CACHE INTERNAL "ASan rulează pe această mașină")
endfunction()

if(FORMA_ENABLE_SANITIZERS AND NOT MSVC)
    forma_check_asan_works()
    if(FORMA_ASAN_WORKS)
        set(FORMA_SANITIZER_FLAGS -fsanitize=address,undefined)
    else()
        set(FORMA_SANITIZER_FLAGS -fsanitize=undefined)
    endif()
endif()

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

    if(FORMA_SANITIZER_FLAGS)
        target_compile_options(${target} PRIVATE ${FORMA_SANITIZER_FLAGS} -fno-omit-frame-pointer)
        target_link_options(${target} PRIVATE ${FORMA_SANITIZER_FLAGS})
    endif()
endfunction()
