#ifndef ALTCROSS_PLATFORM_INPUT_H
#define ALTCROSS_PLATFORM_INPUT_H

#include "altcross/export.h"
#include "altcross/keycode.h"

/* Botões de mouse, neutros entre plataformas. */
typedef enum {
    ALTCROSS_MOUSE_BUTTON_LEFT = 0,
    ALTCROSS_MOUSE_BUTTON_RIGHT = 1,
    ALTCROSS_MOUSE_BUTTON_MIDDLE = 2,
} altcross_mouse_button_t;

typedef struct {
    /* Cursor local se moveu para (x, y) (posição absoluta) enquanto o
     * controle ainda é local — é isso que o chamador usa pra checar hotzone
     * via altcross_hotzone_detect/altcross_input_control_on_local_move. */
    void (*on_local_mouse_move)(int x, int y, void *user_data);

    /* Só disparado enquanto capturado (ver altcross_platform_input_set_captured):
     * o evento já foi engolido, não chega a nenhum app local. */
    void (*on_captured_mouse_delta)(int dx, int dy, void *user_data);
    void (*on_captured_mouse_button)(altcross_mouse_button_t button, int down,
                                      void *user_data);
    void (*on_captured_key)(altcross_keycode_t key, int down, void *user_data);

    /* Combinação de pânico (ver altcross_input_control_release no
     * AGENTS.md) — chamada mesmo em modo capturado; nunca é engolida. */
    void (*on_panic_key)(void *user_data);

    void *user_data;
} altcross_platform_input_callbacks_t;

/* Instala o hook global de mouse/teclado da plataforma e começa a despachar
 * eventos pra callbacks (via altcross_platform_input_pump, ver abaixo). Só
 * deve ser chamado uma vez por processo.
 *
 * No macOS isso pede permissão de Acessibilidade (o SO mostra o prompt na
 * primeira vez rodando fora de um binário já confiado) — sem essa permissão
 * o hook simplesmente não recebe eventos. No Windows precisa rodar com
 * privilégio suficiente pra hook de baixo nível (WH_MOUSE_LL/WH_KEYBOARD_LL).
 *
 * Retorna 0 em sucesso, diferente de zero em erro. */
ALTCROSS_API int altcross_platform_input_start(
    const altcross_platform_input_callbacks_t *callbacks);

/* Bombeia o loop de eventos da plataforma até chamar altcross_platform_input_stop
 * de outra thread, ou até timeout_ms se > 0 (0 = espera indefinidamente).
 * Precisa ser chamada pela mesma thread que chamou start() — no macOS isso
 * roda o CFRunLoop; no Windows, o GetMessage loop. */
ALTCROSS_API void altcross_platform_input_pump(int timeout_ms);

ALTCROSS_API void altcross_platform_input_stop(void);

/* Liga/desliga o modo de captura: ligado, mouse/teclado local param de
 * chegar aos apps locais (retorno de altcross_platform_input_start's
 * callbacks vira on_captured_*); desligado, tudo volta a passar direto
 * (comportamento padrão — "dono local"). */
ALTCROSS_API void altcross_platform_input_set_captured(int captured);

/* Injeta localmente eventos vindos de outra máquina (usado quando este
 * dispositivo é o DESTINO do input, não a origem). */
ALTCROSS_API void altcross_platform_input_inject_mouse_delta(int dx, int dy);
ALTCROSS_API void
altcross_platform_input_inject_mouse_button(altcross_mouse_button_t button,
                                             int down);
ALTCROSS_API void altcross_platform_input_inject_key(altcross_keycode_t key,
                                                       int down);

#endif
