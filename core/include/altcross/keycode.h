#ifndef ALTCROSS_KEYCODE_H
#define ALTCROSS_KEYCODE_H

/* Keycode neutro, independente de plataforma. Cada implementação de
 * altcross/platform_input.h (macOS, Windows) traduz o keycode nativo do SO
 * pra um destes valores antes de mandar pela rede, e faz o caminho inverso ao
 * injetar — é isso que permite "digitar no Mac aparece certo no Windows" e
 * vice-versa. Cobre só o conjunto comum de digitação; teclas exóticas (F13+,
 * mídia, etc.) ficam de fora por enquanto. */
typedef enum {
    ALTCROSS_KEY_UNKNOWN = 0,

    ALTCROSS_KEY_A,
    ALTCROSS_KEY_B,
    ALTCROSS_KEY_C,
    ALTCROSS_KEY_D,
    ALTCROSS_KEY_E,
    ALTCROSS_KEY_F,
    ALTCROSS_KEY_G,
    ALTCROSS_KEY_H,
    ALTCROSS_KEY_I,
    ALTCROSS_KEY_J,
    ALTCROSS_KEY_K,
    ALTCROSS_KEY_L,
    ALTCROSS_KEY_M,
    ALTCROSS_KEY_N,
    ALTCROSS_KEY_O,
    ALTCROSS_KEY_P,
    ALTCROSS_KEY_Q,
    ALTCROSS_KEY_R,
    ALTCROSS_KEY_S,
    ALTCROSS_KEY_T,
    ALTCROSS_KEY_U,
    ALTCROSS_KEY_V,
    ALTCROSS_KEY_W,
    ALTCROSS_KEY_X,
    ALTCROSS_KEY_Y,
    ALTCROSS_KEY_Z,

    ALTCROSS_KEY_0,
    ALTCROSS_KEY_1,
    ALTCROSS_KEY_2,
    ALTCROSS_KEY_3,
    ALTCROSS_KEY_4,
    ALTCROSS_KEY_5,
    ALTCROSS_KEY_6,
    ALTCROSS_KEY_7,
    ALTCROSS_KEY_8,
    ALTCROSS_KEY_9,

    ALTCROSS_KEY_SPACE,
    ALTCROSS_KEY_ENTER,
    ALTCROSS_KEY_TAB,
    ALTCROSS_KEY_BACKSPACE,
    ALTCROSS_KEY_ESCAPE,

    ALTCROSS_KEY_ARROW_LEFT,
    ALTCROSS_KEY_ARROW_RIGHT,
    ALTCROSS_KEY_ARROW_UP,
    ALTCROSS_KEY_ARROW_DOWN,

    ALTCROSS_KEY_SHIFT_LEFT,
    ALTCROSS_KEY_SHIFT_RIGHT,
    ALTCROSS_KEY_CONTROL_LEFT,
    ALTCROSS_KEY_CONTROL_RIGHT,
    ALTCROSS_KEY_ALT_LEFT,
    ALTCROSS_KEY_ALT_RIGHT,
    /* Cmd no macOS / tecla Windows no Windows — mesmo papel de "tecla do SO". */
    ALTCROSS_KEY_META_LEFT,
    ALTCROSS_KEY_META_RIGHT,

    ALTCROSS_KEY_MINUS,
    ALTCROSS_KEY_EQUAL,
    ALTCROSS_KEY_COMMA,
    ALTCROSS_KEY_PERIOD,
    ALTCROSS_KEY_SLASH,
    ALTCROSS_KEY_SEMICOLON,
    ALTCROSS_KEY_QUOTE,
    ALTCROSS_KEY_BACKSLASH,
    ALTCROSS_KEY_LEFT_BRACKET,
    ALTCROSS_KEY_RIGHT_BRACKET,
    ALTCROSS_KEY_GRAVE,
} altcross_keycode_t;

#endif
