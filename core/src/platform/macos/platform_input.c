/* Implementação real de captura/injeção global de mouse e teclado no macOS,
 * via Quartz Event Services (CGEventTap/CGEventPost).
 *
 * IMPORTANTE: altcross_platform_input_start() instala um hook global de
 * verdade — a partir do momento em que ele é chamado E o app tiver permissão
 * de Acessibilidade concedida, TODO mouse/teclado do sistema passa por aqui.
 * Nunca chamar isso automaticamente em teste/CI; só via um binário que o
 * usuário roda de propósito (ver core/tools/altcrossd.c) sabendo o que está
 * ativando. */
#include "altcross/platform_input.h"

#include <ApplicationServices/ApplicationServices.h>
#include <math.h>
#include <stdint.h>

/* Ponto mais alto (menor Y, convenção nativa do CG: Y pra baixo, ancorado
 * no topo da tela PRINCIPAL) entre todos os monitores ativos agora — é o
 * offset que converte entre a origem nativa do CG (sempre a principal) e a
 * convenção deste app (Y=0 no topo de TODAS as telas, ver displays.h /
 * altcross_displays_enumerate). Puro CoreGraphics, sem AppKit — mantém este
 * arquivo compilável como .c simples (NSScreen exigiria Objective-C, ver
 * displays.m). */
static double cg_min_display_top(void) {
    CGDirectDisplayID displays[32];
    uint32_t count = 0;
    CGGetActiveDisplayList(32, displays, &count);
    double min_top = 0;
    int first = 1;
    for (uint32_t i = 0; i < count; i++) {
        CGRect bounds = CGDisplayBounds(displays[i]);
        if (first || bounds.origin.y < min_top) {
            min_top = bounds.origin.y;
            first = 0;
        }
    }
    return min_top;
}

int altcross_platform_get_cursor_position(int *out_x, int *out_y) {
    CGEventRef event = CGEventCreate(NULL);
    if (!event) {
        return 1;
    }
    CGPoint loc = CGEventGetLocation(event);
    CFRelease(event);

    double min_top = cg_min_display_top();
    *out_x = (int)lround(loc.x);
    *out_y = (int)lround(loc.y - min_top);
    return 0;
}

int altcross_platform_warp_cursor(int x, int y) {
    double min_top = cg_min_display_top();
    CGPoint target = CGPointMake((double)x, (double)y + min_top);
    CGError err = CGWarpMouseCursorPosition(target);
    /* Sem isso, o SO "esquece" onde o mouse de verdade está por um instante
     * depois do warp e o próximo movimento físico do usuário fica com um
     * salto/lag estranho — reassocia a posição do cursor com os eventos de
     * mouse reais imediatamente. */
    CGAssociateMouseAndMouseCursorPosition(true);
    return err == kCGErrorSuccess ? 0 : 1;
}

/* Códigos de tecla virtuais do macOS (estáveis desde sempre, não mudam entre
 * versões do SO). Evitamos depender de <Carbon/Carbon.h> só por essas
 * constantes. */
#define MAC_VK_A 0x00
#define MAC_VK_S 0x01
#define MAC_VK_D 0x02
#define MAC_VK_F 0x03
#define MAC_VK_H 0x04
#define MAC_VK_G 0x05
#define MAC_VK_Z 0x06
#define MAC_VK_X 0x07
#define MAC_VK_C 0x08
#define MAC_VK_V 0x09
#define MAC_VK_B 0x0B
#define MAC_VK_Q 0x0C
#define MAC_VK_W 0x0D
#define MAC_VK_E 0x0E
#define MAC_VK_R 0x0F
#define MAC_VK_Y 0x10
#define MAC_VK_T 0x11
#define MAC_VK_1 0x12
#define MAC_VK_2 0x13
#define MAC_VK_3 0x14
#define MAC_VK_4 0x15
#define MAC_VK_6 0x16
#define MAC_VK_5 0x17
#define MAC_VK_EQUAL 0x18
#define MAC_VK_9 0x19
#define MAC_VK_7 0x1A
#define MAC_VK_MINUS 0x1B
#define MAC_VK_8 0x1C
#define MAC_VK_0 0x1D
#define MAC_VK_RIGHT_BRACKET 0x1E
#define MAC_VK_O 0x1F
#define MAC_VK_U 0x20
#define MAC_VK_LEFT_BRACKET 0x21
#define MAC_VK_I 0x22
#define MAC_VK_P 0x23
#define MAC_VK_RETURN 0x24
#define MAC_VK_L 0x25
#define MAC_VK_J 0x26
#define MAC_VK_QUOTE 0x27
#define MAC_VK_K 0x28
#define MAC_VK_SEMICOLON 0x29
#define MAC_VK_BACKSLASH 0x2A
#define MAC_VK_COMMA 0x2B
#define MAC_VK_SLASH 0x2C
#define MAC_VK_N 0x2D
#define MAC_VK_M 0x2E
#define MAC_VK_PERIOD 0x2F
#define MAC_VK_TAB 0x30
#define MAC_VK_SPACE 0x31
#define MAC_VK_GRAVE 0x32
#define MAC_VK_DELETE 0x33
#define MAC_VK_ESCAPE 0x35
#define MAC_VK_RIGHT_COMMAND 0x36
#define MAC_VK_COMMAND 0x37
#define MAC_VK_SHIFT 0x38
#define MAC_VK_OPTION 0x3A
#define MAC_VK_CONTROL 0x3B
#define MAC_VK_RIGHT_SHIFT 0x3C
#define MAC_VK_RIGHT_OPTION 0x3D
#define MAC_VK_RIGHT_CONTROL 0x3E
#define MAC_VK_LEFT_ARROW 0x7B
#define MAC_VK_RIGHT_ARROW 0x7C
#define MAC_VK_DOWN_ARROW 0x7D
#define MAC_VK_UP_ARROW 0x7E

static altcross_keycode_t macos_keycode_to_neutral(CGKeyCode code) {
    switch (code) {
    case MAC_VK_A: return ALTCROSS_KEY_A;
    case MAC_VK_B: return ALTCROSS_KEY_B;
    case MAC_VK_C: return ALTCROSS_KEY_C;
    case MAC_VK_D: return ALTCROSS_KEY_D;
    case MAC_VK_E: return ALTCROSS_KEY_E;
    case MAC_VK_F: return ALTCROSS_KEY_F;
    case MAC_VK_G: return ALTCROSS_KEY_G;
    case MAC_VK_H: return ALTCROSS_KEY_H;
    case MAC_VK_I: return ALTCROSS_KEY_I;
    case MAC_VK_J: return ALTCROSS_KEY_J;
    case MAC_VK_K: return ALTCROSS_KEY_K;
    case MAC_VK_L: return ALTCROSS_KEY_L;
    case MAC_VK_M: return ALTCROSS_KEY_M;
    case MAC_VK_N: return ALTCROSS_KEY_N;
    case MAC_VK_O: return ALTCROSS_KEY_O;
    case MAC_VK_P: return ALTCROSS_KEY_P;
    case MAC_VK_Q: return ALTCROSS_KEY_Q;
    case MAC_VK_R: return ALTCROSS_KEY_R;
    case MAC_VK_S: return ALTCROSS_KEY_S;
    case MAC_VK_T: return ALTCROSS_KEY_T;
    case MAC_VK_U: return ALTCROSS_KEY_U;
    case MAC_VK_V: return ALTCROSS_KEY_V;
    case MAC_VK_W: return ALTCROSS_KEY_W;
    case MAC_VK_X: return ALTCROSS_KEY_X;
    case MAC_VK_Y: return ALTCROSS_KEY_Y;
    case MAC_VK_Z: return ALTCROSS_KEY_Z;
    case MAC_VK_0: return ALTCROSS_KEY_0;
    case MAC_VK_1: return ALTCROSS_KEY_1;
    case MAC_VK_2: return ALTCROSS_KEY_2;
    case MAC_VK_3: return ALTCROSS_KEY_3;
    case MAC_VK_4: return ALTCROSS_KEY_4;
    case MAC_VK_5: return ALTCROSS_KEY_5;
    case MAC_VK_6: return ALTCROSS_KEY_6;
    case MAC_VK_7: return ALTCROSS_KEY_7;
    case MAC_VK_8: return ALTCROSS_KEY_8;
    case MAC_VK_9: return ALTCROSS_KEY_9;
    case MAC_VK_SPACE: return ALTCROSS_KEY_SPACE;
    case MAC_VK_RETURN: return ALTCROSS_KEY_ENTER;
    case MAC_VK_TAB: return ALTCROSS_KEY_TAB;
    case MAC_VK_DELETE: return ALTCROSS_KEY_BACKSPACE;
    case MAC_VK_ESCAPE: return ALTCROSS_KEY_ESCAPE;
    case MAC_VK_LEFT_ARROW: return ALTCROSS_KEY_ARROW_LEFT;
    case MAC_VK_RIGHT_ARROW: return ALTCROSS_KEY_ARROW_RIGHT;
    case MAC_VK_UP_ARROW: return ALTCROSS_KEY_ARROW_UP;
    case MAC_VK_DOWN_ARROW: return ALTCROSS_KEY_ARROW_DOWN;
    case MAC_VK_SHIFT: return ALTCROSS_KEY_SHIFT_LEFT;
    case MAC_VK_RIGHT_SHIFT: return ALTCROSS_KEY_SHIFT_RIGHT;
    case MAC_VK_CONTROL: return ALTCROSS_KEY_CONTROL_LEFT;
    case MAC_VK_RIGHT_CONTROL: return ALTCROSS_KEY_CONTROL_RIGHT;
    case MAC_VK_OPTION: return ALTCROSS_KEY_ALT_LEFT;
    case MAC_VK_RIGHT_OPTION: return ALTCROSS_KEY_ALT_RIGHT;
    case MAC_VK_COMMAND: return ALTCROSS_KEY_META_LEFT;
    case MAC_VK_RIGHT_COMMAND: return ALTCROSS_KEY_META_RIGHT;
    case MAC_VK_MINUS: return ALTCROSS_KEY_MINUS;
    case MAC_VK_EQUAL: return ALTCROSS_KEY_EQUAL;
    case MAC_VK_COMMA: return ALTCROSS_KEY_COMMA;
    case MAC_VK_PERIOD: return ALTCROSS_KEY_PERIOD;
    case MAC_VK_SLASH: return ALTCROSS_KEY_SLASH;
    case MAC_VK_SEMICOLON: return ALTCROSS_KEY_SEMICOLON;
    case MAC_VK_QUOTE: return ALTCROSS_KEY_QUOTE;
    case MAC_VK_BACKSLASH: return ALTCROSS_KEY_BACKSLASH;
    case MAC_VK_LEFT_BRACKET: return ALTCROSS_KEY_LEFT_BRACKET;
    case MAC_VK_RIGHT_BRACKET: return ALTCROSS_KEY_RIGHT_BRACKET;
    case MAC_VK_GRAVE: return ALTCROSS_KEY_GRAVE;
    default: return ALTCROSS_KEY_UNKNOWN;
    }
}

static CGKeyCode neutral_to_macos_keycode(altcross_keycode_t key) {
    switch (key) {
    case ALTCROSS_KEY_A: return MAC_VK_A;
    case ALTCROSS_KEY_B: return MAC_VK_B;
    case ALTCROSS_KEY_C: return MAC_VK_C;
    case ALTCROSS_KEY_D: return MAC_VK_D;
    case ALTCROSS_KEY_E: return MAC_VK_E;
    case ALTCROSS_KEY_F: return MAC_VK_F;
    case ALTCROSS_KEY_G: return MAC_VK_G;
    case ALTCROSS_KEY_H: return MAC_VK_H;
    case ALTCROSS_KEY_I: return MAC_VK_I;
    case ALTCROSS_KEY_J: return MAC_VK_J;
    case ALTCROSS_KEY_K: return MAC_VK_K;
    case ALTCROSS_KEY_L: return MAC_VK_L;
    case ALTCROSS_KEY_M: return MAC_VK_M;
    case ALTCROSS_KEY_N: return MAC_VK_N;
    case ALTCROSS_KEY_O: return MAC_VK_O;
    case ALTCROSS_KEY_P: return MAC_VK_P;
    case ALTCROSS_KEY_Q: return MAC_VK_Q;
    case ALTCROSS_KEY_R: return MAC_VK_R;
    case ALTCROSS_KEY_S: return MAC_VK_S;
    case ALTCROSS_KEY_T: return MAC_VK_T;
    case ALTCROSS_KEY_U: return MAC_VK_U;
    case ALTCROSS_KEY_V: return MAC_VK_V;
    case ALTCROSS_KEY_W: return MAC_VK_W;
    case ALTCROSS_KEY_X: return MAC_VK_X;
    case ALTCROSS_KEY_Y: return MAC_VK_Y;
    case ALTCROSS_KEY_Z: return MAC_VK_Z;
    case ALTCROSS_KEY_0: return MAC_VK_0;
    case ALTCROSS_KEY_1: return MAC_VK_1;
    case ALTCROSS_KEY_2: return MAC_VK_2;
    case ALTCROSS_KEY_3: return MAC_VK_3;
    case ALTCROSS_KEY_4: return MAC_VK_4;
    case ALTCROSS_KEY_5: return MAC_VK_5;
    case ALTCROSS_KEY_6: return MAC_VK_6;
    case ALTCROSS_KEY_7: return MAC_VK_7;
    case ALTCROSS_KEY_8: return MAC_VK_8;
    case ALTCROSS_KEY_9: return MAC_VK_9;
    case ALTCROSS_KEY_SPACE: return MAC_VK_SPACE;
    case ALTCROSS_KEY_ENTER: return MAC_VK_RETURN;
    case ALTCROSS_KEY_TAB: return MAC_VK_TAB;
    case ALTCROSS_KEY_BACKSPACE: return MAC_VK_DELETE;
    case ALTCROSS_KEY_ESCAPE: return MAC_VK_ESCAPE;
    case ALTCROSS_KEY_ARROW_LEFT: return MAC_VK_LEFT_ARROW;
    case ALTCROSS_KEY_ARROW_RIGHT: return MAC_VK_RIGHT_ARROW;
    case ALTCROSS_KEY_ARROW_UP: return MAC_VK_UP_ARROW;
    case ALTCROSS_KEY_ARROW_DOWN: return MAC_VK_DOWN_ARROW;
    case ALTCROSS_KEY_SHIFT_LEFT: return MAC_VK_SHIFT;
    case ALTCROSS_KEY_SHIFT_RIGHT: return MAC_VK_RIGHT_SHIFT;
    case ALTCROSS_KEY_CONTROL_LEFT: return MAC_VK_CONTROL;
    case ALTCROSS_KEY_CONTROL_RIGHT: return MAC_VK_RIGHT_CONTROL;
    case ALTCROSS_KEY_ALT_LEFT: return MAC_VK_OPTION;
    case ALTCROSS_KEY_ALT_RIGHT: return MAC_VK_RIGHT_OPTION;
    case ALTCROSS_KEY_META_LEFT: return MAC_VK_COMMAND;
    case ALTCROSS_KEY_META_RIGHT: return MAC_VK_RIGHT_COMMAND;
    case ALTCROSS_KEY_MINUS: return MAC_VK_MINUS;
    case ALTCROSS_KEY_EQUAL: return MAC_VK_EQUAL;
    case ALTCROSS_KEY_COMMA: return MAC_VK_COMMA;
    case ALTCROSS_KEY_PERIOD: return MAC_VK_PERIOD;
    case ALTCROSS_KEY_SLASH: return MAC_VK_SLASH;
    case ALTCROSS_KEY_SEMICOLON: return MAC_VK_SEMICOLON;
    case ALTCROSS_KEY_QUOTE: return MAC_VK_QUOTE;
    case ALTCROSS_KEY_BACKSLASH: return MAC_VK_BACKSLASH;
    case ALTCROSS_KEY_LEFT_BRACKET: return MAC_VK_LEFT_BRACKET;
    case ALTCROSS_KEY_RIGHT_BRACKET: return MAC_VK_RIGHT_BRACKET;
    case ALTCROSS_KEY_GRAVE: return MAC_VK_GRAVE;
    default: return (CGKeyCode)0xFFFF;
    }
}

static altcross_platform_input_callbacks_t g_callbacks;
static int g_captured = 0;
static CFMachPortRef g_tap = NULL;
static CFRunLoopSourceRef g_tap_source = NULL;
static CFRunLoopRef g_run_loop = NULL;
static CGPoint g_injected_position;
static int g_injected_position_valid = 0;

static CGPoint current_mouse_location(void) {
    CGEventRef event = CGEventCreate(NULL);
    CGPoint point = CGEventGetLocation(event);
    CFRelease(event);
    return point;
}

static altcross_mouse_button_t cg_button_for_event(CGEventType type) {
    switch (type) {
    case kCGEventRightMouseDown:
    case kCGEventRightMouseUp:
    case kCGEventRightMouseDragged:
        return ALTCROSS_MOUSE_BUTTON_RIGHT;
    case kCGEventOtherMouseDown:
    case kCGEventOtherMouseUp:
    case kCGEventOtherMouseDragged:
        return ALTCROSS_MOUSE_BUTTON_MIDDLE;
    default:
        return ALTCROSS_MOUSE_BUTTON_LEFT;
    }
}

static CGEventRef tap_callback(CGEventTapProxy proxy, CGEventType type,
                                CGEventRef event, void *refcon) {
    (void)proxy;
    (void)refcon;

    if (type == kCGEventTapDisabledByTimeout ||
        type == kCGEventTapDisabledByUserInput) {
        if (g_tap) {
            CGEventTapEnable(g_tap, true);
        }
        return event;
    }

    if (type == kCGEventKeyDown) {
        CGKeyCode code = (CGKeyCode)CGEventGetIntegerValueField(
            event, kCGKeyboardEventKeycode);
        CGEventFlags flags = CGEventGetFlags(event);
        if (code == MAC_VK_ESCAPE && (flags & kCGEventFlagMaskControl)) {
            if (g_callbacks.on_panic_key) {
                g_callbacks.on_panic_key(g_callbacks.user_data);
            }
            return event; /* tecla de pânico nunca é engolida */
        }
    }

    if (!g_captured) {
        if (type == kCGEventMouseMoved || type == kCGEventLeftMouseDragged ||
            type == kCGEventRightMouseDragged ||
            type == kCGEventOtherMouseDragged) {
            CGPoint p = CGEventGetLocation(event);
            if (g_callbacks.on_local_mouse_move) {
                g_callbacks.on_local_mouse_move((int)p.x, (int)p.y,
                                                 g_callbacks.user_data);
            }
        }
        return event;
    }

    switch (type) {
    case kCGEventMouseMoved:
    case kCGEventLeftMouseDragged:
    case kCGEventRightMouseDragged:
    case kCGEventOtherMouseDragged: {
        int dx = (int)CGEventGetIntegerValueField(event, kCGMouseEventDeltaX);
        int dy = (int)CGEventGetIntegerValueField(event, kCGMouseEventDeltaY);
        if (g_callbacks.on_captured_mouse_delta) {
            g_callbacks.on_captured_mouse_delta(dx, dy, g_callbacks.user_data);
        }
        return NULL;
    }
    case kCGEventLeftMouseDown:
    case kCGEventLeftMouseUp:
    case kCGEventRightMouseDown:
    case kCGEventRightMouseUp:
    case kCGEventOtherMouseDown:
    case kCGEventOtherMouseUp: {
        int down = (type == kCGEventLeftMouseDown ||
                    type == kCGEventRightMouseDown ||
                    type == kCGEventOtherMouseDown);
        if (g_callbacks.on_captured_mouse_button) {
            g_callbacks.on_captured_mouse_button(cg_button_for_event(type),
                                                  down, g_callbacks.user_data);
        }
        return NULL;
    }
    case kCGEventKeyDown:
    case kCGEventKeyUp: {
        CGKeyCode code = (CGKeyCode)CGEventGetIntegerValueField(
            event, kCGKeyboardEventKeycode);
        if (g_callbacks.on_captured_key) {
            g_callbacks.on_captured_key(macos_keycode_to_neutral(code),
                                         type == kCGEventKeyDown,
                                         g_callbacks.user_data);
        }
        return NULL;
    }
    default:
        /* Inclui kCGEventFlagsChanged (modificadores) — fora de escopo no
         * MVP, engolido por segurança quando capturado (ver limitação no
         * AGENTS.md). */
        return NULL;
    }
}

int altcross_platform_input_start(
    const altcross_platform_input_callbacks_t *callbacks) {
    g_callbacks = *callbacks;
    g_captured = 0;

    CGEventMask mask =
        CGEventMaskBit(kCGEventMouseMoved) |
        CGEventMaskBit(kCGEventLeftMouseDragged) |
        CGEventMaskBit(kCGEventRightMouseDragged) |
        CGEventMaskBit(kCGEventOtherMouseDragged) |
        CGEventMaskBit(kCGEventLeftMouseDown) |
        CGEventMaskBit(kCGEventLeftMouseUp) |
        CGEventMaskBit(kCGEventRightMouseDown) |
        CGEventMaskBit(kCGEventRightMouseUp) |
        CGEventMaskBit(kCGEventOtherMouseDown) |
        CGEventMaskBit(kCGEventOtherMouseUp) |
        CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp);

    g_tap = CGEventTapCreate(kCGSessionEventTap, kCGHeadInsertEventTap,
                              kCGEventTapOptionDefault, mask, tap_callback,
                              NULL);
    if (!g_tap) {
        return 1; /* provavelmente sem permissão de Acessibilidade */
    }

    g_tap_source = CFMachPortCreateRunLoopSource(NULL, g_tap, 0);
    g_run_loop = CFRunLoopGetCurrent();
    CFRunLoopAddSource(g_run_loop, g_tap_source, kCFRunLoopCommonModes);
    CGEventTapEnable(g_tap, true);
    return 0;
}

void altcross_platform_input_pump(int timeout_ms) {
    if (timeout_ms > 0) {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout_ms / 1000.0, false);
    } else {
        CFRunLoopRun();
    }
}

void altcross_platform_input_stop(void) {
    if (g_tap) {
        CGEventTapEnable(g_tap, false);
    }
    if (g_tap_source && g_run_loop) {
        CFRunLoopRemoveSource(g_run_loop, g_tap_source, kCFRunLoopCommonModes);
    }
    if (g_run_loop) {
        CFRunLoopStop(g_run_loop);
    }
    if (g_tap_source) {
        CFRelease(g_tap_source);
        g_tap_source = NULL;
    }
    if (g_tap) {
        CFRelease(g_tap);
        g_tap = NULL;
    }
}

void altcross_platform_input_set_captured(int captured) {
    g_captured = captured ? 1 : 0;
    if (g_captured) {
        g_injected_position_valid = 0;
    }
}

void altcross_platform_input_inject_mouse_delta(int dx, int dy) {
    if (!g_injected_position_valid) {
        g_injected_position = current_mouse_location();
        g_injected_position_valid = 1;
    }
    g_injected_position.x += dx;
    g_injected_position.y += dy;

    CGEventRef move = CGEventCreateMouseEvent(
        NULL, kCGEventMouseMoved, g_injected_position, kCGMouseButtonLeft);
    CGEventPost(kCGHIDEventTap, move);
    CFRelease(move);
}

void altcross_platform_input_inject_mouse_button(
    altcross_mouse_button_t button, int down) {
    CGMouseButton cg_button = kCGMouseButtonLeft;
    CGEventType type = down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp;
    if (button == ALTCROSS_MOUSE_BUTTON_RIGHT) {
        cg_button = kCGMouseButtonRight;
        type = down ? kCGEventRightMouseDown : kCGEventRightMouseUp;
    } else if (button == ALTCROSS_MOUSE_BUTTON_MIDDLE) {
        cg_button = kCGMouseButtonCenter;
        type = down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp;
    }

    CGPoint pos =
        g_injected_position_valid ? g_injected_position : current_mouse_location();
    CGEventRef ev = CGEventCreateMouseEvent(NULL, type, pos, cg_button);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}

void altcross_platform_input_inject_key(altcross_keycode_t key, int down) {
    CGKeyCode code = neutral_to_macos_keycode(key);
    if (code == 0xFFFF) {
        return;
    }
    CGEventRef ev = CGEventCreateKeyboardEvent(NULL, code, down ? true : false);
    CGEventPost(kCGHIDEventTap, ev);
    CFRelease(ev);
}
