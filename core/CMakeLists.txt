cmake_minimum_required(VERSION 3.16)
project(altcross_core C)

if(APPLE)
    # displays.m usa NSScreen (AppKit), só dá pra chamar de Objective-C.
    enable_language(OBJC)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Níveis de warning altos em qualquer compilador: -Wextra/-Wpedantic são
# específicos de GCC/Clang — o cl.exe do MSVC rejeita esses flags (D8021) e
# usa /W4 como equivalente.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
    add_compile_options(/W4)
endif()

set(ALTCROSS_CORE_SOURCES
    src/hotzone.c
    src/input_control.c
    src/protocol.c
    src/net_socket.c
    src/pairing.c
    src/discovery.c
)

# discovery.c usa pthread (POSIX) pra thread do respondedor de descoberta —
# no Windows usa CreateThread/kernel32 direto, não precisa disso.
if(NOT WIN32)
    find_package(Threads REQUIRED)
endif()

# Código de captura/injeção de mouse e teclado é específico de plataforma —
# um arquivo por SO, atrás da interface comum altcross/platform_input.h (ver
# "Boas Práticas" no AGENTS.md). Fora de macOS/Windows ele simplesmente não
# entra no build (esses são os únicos SOs no escopo de desenvolvimento atual).
set(ALTCROSS_PLATFORM_LIBS "")
if(APPLE)
    list(APPEND ALTCROSS_CORE_SOURCES
        src/platform/macos/platform_input.c
        src/platform/macos/displays.m
    )
    list(APPEND ALTCROSS_PLATFORM_LIBS "-framework ApplicationServices" "-framework AppKit" Threads::Threads)
elseif(WIN32)
    list(APPEND ALTCROSS_CORE_SOURCES
        src/platform/windows/platform_input.c
        src/platform/windows/displays.c
    )
    list(APPEND ALTCROSS_PLATFORM_LIBS user32 ws2_32 iphlpapi)
else()
    list(APPEND ALTCROSS_PLATFORM_LIBS Threads::Threads)
endif()

# Lib estática: usada pelo executável de testes (build rápido, sem custo de
# exportação de símbolos).
add_library(altcross_core STATIC ${ALTCROSS_CORE_SOURCES})
target_include_directories(altcross_core PUBLIC include)
target_link_libraries(altcross_core PUBLIC ${ALTCROSS_PLATFORM_LIBS})

# Lib compartilhada: o artefato de verdade, carregado pelo Flutter via
# dart:ffi (DynamicLibrary.open). Mesmas fontes, alvo separado porque exportar
# símbolos (ALTCROSS_API) e virar .dylib/.so/.dll é uma preocupação diferente
# de compilar rápido pros testes.
add_library(altcross_core_shared SHARED ${ALTCROSS_CORE_SOURCES})
target_include_directories(altcross_core_shared PUBLIC include)
target_compile_definitions(altcross_core_shared PRIVATE ALTCROSS_BUILDING_SHARED)
target_link_libraries(altcross_core_shared PUBLIC ${ALTCROSS_PLATFORM_LIBS})
set_target_properties(altcross_core_shared PROPERTIES
    OUTPUT_NAME altcross_core
    C_VISIBILITY_PRESET hidden
)

enable_testing()

add_executable(altcross_core_tests
    tests/test_main.c
    tests/test_framework.c
    tests/test_hotzone.c
    tests/test_input_control.c
    tests/test_protocol.c
    tests/test_net_socket.c
    tests/test_pairing.c
    tests/test_discovery.c
)
target_link_libraries(altcross_core_tests PRIVATE altcross_core)
target_include_directories(altcross_core_tests PRIVATE tests)

add_test(NAME altcross_core_tests COMMAND altcross_core_tests)

# Daemon real (não é rodado automaticamente por nenhum teste/CI — captura
# mouse/teclado globais de verdade, ver aviso em tools/altcrossd.c).
if(APPLE OR WIN32)
    add_executable(altcrossd tools/altcrossd.c)
    target_link_libraries(altcrossd PRIVATE altcross_core)

    # Ferramentas seguras (não capturam input nem hook global) — podem rodar
    # automaticamente/manualmente sem aviso nenhum.
    add_executable(list_displays tools/list_displays.c)
    target_link_libraries(list_displays PRIVATE altcross_core)

    add_executable(discovery_demo tools/discovery_demo.c)
    target_link_libraries(discovery_demo PRIVATE altcross_core)
endif()
