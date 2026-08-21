/* Implementação real de captura/injeção global de mouse e teclado no
 * Windows, via low-level hooks (WH_MOUSE_LL/WH_KEYBOARD_LL) e SendInput.
 *
 * *** NÃO VERIFICADO EM WINDOWS DE VERDADE ***
 * Este arquivo foi escrito e revisado, mas nunca compilado nem executado —
 * esta sessão de desenvolvimento roda em macOS e não há acesso a uma máquina
 * Windows (ver limitação já registrada no AGENTS.md). Antes de confiar nele
 * em produção, compile e teste manualmente num Windows real: mova o mouse
 * pelas 4 bordas, digite letras/números/pontuação e confira que chegam
 * certas do outro lado, e confirme que Ctrl+Esc sempre solta o controle
 * mesmo em modo capturado.
 *
 * IMPORTANTE (mesma observação da versão macOS): a partir de
 * altcross_platform_input_start(), todo mouse/teclado do sistema passa por
 * aqui. Nunca chamar isso automaticamente em teste/CI. */
#include "altcross/platform_input.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static altcross_platform_input_callbacks_t g_callbacks;
static int g_captured = 0;
static HHOOK g_mouse_hook = NULL;
static HHOOK g_keyboard_hook = NULL;
static LONG g_last_x = 0;
static LONG g_last_y = 0;
static LONG g_injected_x = 0;
static LONG g_injected_y = 0;
static int g_injected_position_valid = 0;

static altcross_keycode_t vk_to_neutral(DWORD vk) {
    if (vk >= 'A' && vk <= 'Z') {
        return (altcross_keycode_t)(ALTCROSS_KEY_A + (vk - 'A'));
    }
    if (vk >= '0' && vk <= '9') {
        return (altcross_keycode_t)(ALTCROSS_KEY_0 + (vk - '0'));
    }
    switch (vk) {
    case VK_SPACE: return ALTCROSS_KEY_SPACE;
    case VK_RETURN: return ALTCROSS_KEY_ENTER;
    case VK_TAB: return ALTCROSS_KEY_TAB;
    case VK_BACK: return ALTCROSS_KEY_BACKSPACE;
    case VK_ESCAPE: return ALTCROSS_KEY_ESCAPE;
    case VK_LEFT: return ALTCROSS_KEY_ARROW_LEFT;
    case VK_RIGHT: return ALTCROSS_KEY_ARROW_RIGHT;
    case VK_UP: return ALTCROSS_KEY_ARROW_UP;
    case VK_DOWN: return ALTCROSS_KEY_ARROW_DOWN;
    case VK_LSHIFT: return ALTCROSS_KEY_SHIFT_LEFT;
    case VK_RSHIFT: return ALTCROSS_KEY_SHIFT_RIGHT;
    case VK_LCONTROL: return ALTCROSS_KEY_CONTROL_LEFT;
    case VK_RCONTROL: return ALTCROSS_KEY_CONTROL_RIGHT;
    case VK_LMENU: return ALTCROSS_KEY_ALT_LEFT;
    case VK_RMENU: return ALTCROSS_KEY_ALT_RIGHT;
    case VK_LWIN: return ALTCROSS_KEY_META_LEFT;
    case VK_RWIN: return ALTCROSS_KEY_META_RIGHT;
    case VK_OEM_MINUS: return ALTCROSS_KEY_MINUS;
    case VK_OEM_PLUS: return ALTCROSS_KEY_EQUAL;
    case VK_OEM_COMMA: return ALTCROSS_KEY_COMMA;
    case VK_OEM_PERIOD: return ALTCROSS_KEY_PERIOD;
    case VK_OEM_2: return ALTCROSS_KEY_SLASH;
    case VK_OEM_1: return ALTCROSS_KEY_SEMICOLON;
    case VK_OEM_7: return ALTCROSS_KEY_QUOTE;
    case VK_OEM_5: return ALTCROSS_KEY_BACKSLASH;
    case VK_OEM_4: return ALTCROSS_KEY_LEFT_BRACKET;
    case VK_OEM_6: return ALTCROSS_KEY_RIGHT_BRACKET;
    case VK_OEM_3: return ALTCROSS_KEY_GRAVE;
    default: return ALTCROSS_KEY_UNKNOWN;
    }
}

static WORD neutral_to_vk(altcross_keycode_t key) {
    if (key >= ALTCROSS_KEY_A && key <= ALTCROSS_KEY_Z) {
        return (WORD)('A' + (key - ALTCROSS_KEY_A));
    }
    if (key >= ALTCROSS_KEY_0 && key <= ALTCROSS_KEY_9) {
        return (WORD)('0' + (key - ALTCROSS_KEY_0));
    }
    switch (key) {
    case ALTCROSS_KEY_SPACE: return VK_SPACE;
    case ALTCROSS_KEY_ENTER: return VK_RETURN;
    case ALTCROSS_KEY_TAB: return VK_TAB;
    case ALTCROSS_KEY_BACKSPACE: return VK_BACK;
    case ALTCROSS_KEY_ESCAPE: return VK_ESCAPE;
    case ALTCROSS_KEY_ARROW_LEFT: return VK_LEFT;
    case ALTCROSS_KEY_ARROW_RIGHT: return VK_RIGHT;
    case ALTCROSS_KEY_ARROW_UP: return VK_UP;
    case ALTCROSS_KEY_ARROW_DOWN: return VK_DOWN;
    case ALTCROSS_KEY_SHIFT_LEFT: return VK_LSHIFT;
    case ALTCROSS_KEY_SHIFT_RIGHT: return VK_RSHIFT;
    case ALTCROSS_KEY_CONTROL_LEFT: return VK_LCONTROL;
    case ALTCROSS_KEY_CONTROL_RIGHT: return VK_RCONTROL;
    case ALTCROSS_KEY_ALT_LEFT: return VK_LMENU;
    case ALTCROSS_KEY_ALT_RIGHT: return VK_RMENU;
    case ALTCROSS_KEY_META_LEFT: return VK_LWIN;
    case ALTCROSS_KEY_META_RIGHT: return VK_RWIN;
    case ALTCROSS_KEY_MINUS: return VK_OEM_MINUS;
    case ALTCROSS_KEY_EQUAL: return VK_OEM_PLUS;
    case ALTCROSS_KEY_COMMA: return VK_OEM_COMMA;
    case ALTCROSS_KEY_PERIOD: return VK_OEM_PERIOD;
    case ALTCROSS_KEY_SLASH: return VK_OEM_2;
    case ALTCROSS_KEY_SEMICOLON: return VK_OEM_1;
    case ALTCROSS_KEY_QUOTE: return VK_OEM_7;
    case ALTCROSS_KEY_BACKSLASH: return VK_OEM_5;
    case ALTCROSS_KEY_LEFT_BRACKET: return VK_OEM_4;
    case ALTCROSS_KEY_RIGHT_BRACKET: return VK_OEM_6;
    case ALTCROSS_KEY_GRAVE: return VK_OEM_3;
    default: return 0;
    }
}

static void recenter_cursor_while_captured(void) {
    int cx = GetSystemMetrics(SM_CXSCREEN) / 2;
    int cy = GetSystemMetrics(SM_CYSCREEN) / 2;
    SetCursorPos(cx, cy);
    g_last_x = cx;
    g_last_y = cy;
}

static LRESULT CALLBACK mouse_hook_proc(int code, WPARAM wparam,
                                         LPARAM lparam) {
    if (code < 0) {
        return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
    }

    MSLLHOOKSTRUCT *info = (MSLLHOOKSTRUCT *)lparam;

    if (!g_captured) {
        if (wparam == WM_MOUSEMOVE && g_callbacks.on_local_mouse_move) {
            g_callbacks.on_local_mouse_move(info->pt.x, info->pt.y,
                                             g_callbacks.user_data);
        }
        return CallNextHookEx(g_mouse_hook, code, wparam, lparam);
    }

    if (wparam == WM_MOUSEMOVE) {
        int dx = info->pt.x - g_last_x;
        int dy = info->pt.y - g_last_y;
        if ((dx != 0 || dy != 0) && g_callbacks.on_captured_mouse_delta) {
            g_callbacks.on_captured_mouse_delta(dx, dy, g_callbacks.user_data);
        }
        recenter_cursor_while_captured();
        return 1; /* engole: não deixa o app local ver o movimento */
    }

    altcross_mouse_button_t button = ALTCROSS_MOUSE_BUTTON_LEFT;
    int down = -1;
    switch (wparam) {
    case WM_LBUTTONDOWN: button = ALTCROSS_MOUSE_BUTTON_LEFT; down = 1; break;
    case WM_LBUTTONUP: button = ALTCROSS_MOUSE_BUTTON_LEFT; down = 0; break;
    case WM_RBUTTONDOWN: button = ALTCROSS_MOUSE_BUTTON_RIGHT; down = 1; break;
    case WM_RBUTTONUP: button = ALTCROSS_MOUSE_BUTTON_RIGHT; down = 0; break;
    case WM_MBUTTONDOWN: button = ALTCROSS_MOUSE_BUTTON_MIDDLE; down = 1; break;
    case WM_MBUTTONUP: button = ALTCROSS_MOUSE_BUTTON_MIDDLE; down = 0; break;
    default: break;
    }
    if (down >= 0) {
        if (g_callbacks.on_captured_mouse_button) {
            g_callbacks.on_captured_mouse_button(button, down,
                                                  g_callbacks.user_data);
        }
        return 1;
    }

    return 1; /* qualquer outro evento de mouse capturado, engole por segurança */
}

static LRESULT CALLBACK keyboard_hook_proc(int code, WPARAM wparam,
                                            LPARAM lparam) {
    if (code < 0) {
        return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
    }

    KBDLLHOOKSTRUCT *info = (KBDLLHOOKSTRUCT *)lparam;
    int down = (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN);
    int up = (wparam == WM_KEYUP || wparam == WM_SYSKEYUP);

    if (down && info->vkCode == VK_ESCAPE &&
        (GetKeyState(VK_CONTROL) & 0x8000)) {
        if (g_callbacks.on_panic_key) {
            g_callbacks.on_panic_key(g_callbacks.user_data);
        }
        return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
    }

    if (!g_captured) {
        return CallNextHookEx(g_keyboard_hook, code, wparam, lparam);
    }

    if (down || up) {
        altcross_keycode_t neutral = vk_to_neutral((DWORD)info->vkCode);
        if (g_callbacks.on_captured_key) {
            g_callbacks.on_captured_key(neutral, down, g_callbacks.user_data);
        }
    }
    return 1; /* engole */
}

int altcross_platform_input_start(
    const altcross_platform_input_callbacks_t *callbacks) {
    g_callbacks = *callbacks;
    g_captured = 0;

    g_mouse_hook =
        SetWindowsHookExW(WH_MOUSE_LL, mouse_hook_proc, GetModuleHandleW(NULL), 0);
    g_keyboard_hook = SetWindowsHookExW(WH_KEYBOARD_LL, keyboard_hook_proc,
                                         GetModuleHandleW(NULL), 0);

    if (!g_mouse_hook || !g_keyboard_hook) {
        altcross_platform_input_stop();
        return 1;
    }
    return 0;
}

void altcross_platform_input_pump(int timeout_ms) {
    MSG msg;
    if (timeout_ms > 0) {
        DWORD result = MsgWaitForMultipleObjects(0, NULL, FALSE,
                                                  (DWORD)timeout_ms,
                                                  QS_ALLINPUT);
        if (result == WAIT_OBJECT_0) {
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }
    } else {
        while (GetMessageW(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
}

void altcross_platform_input_stop(void) {
    if (g_mouse_hook) {
        UnhookWindowsHookEx(g_mouse_hook);
        g_mouse_hook = NULL;
    }
    if (g_keyboard_hook) {
        UnhookWindowsHookEx(g_keyboard_hook);
        g_keyboard_hook = NULL;
    }
}

void altcross_platform_input_set_captured(int captured) {
    g_captured = captured ? 1 : 0;
    if (g_captured) {
        recenter_cursor_while_captured();
        g_injected_position_valid = 0;
    }
}

void altcross_platform_input_inject_mouse_delta(int dx, int dy) {
    if (!g_injected_position_valid) {
        POINT p;
        GetCursorPos(&p);
        g_injected_x = p.x;
        g_injected_y = p.y;
        g_injected_position_valid = 1;
    }
    g_injected_x += dx;
    g_injected_y += dy;

    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    input.mi.dx = (LONG)(g_injected_x * (65536.0 / GetSystemMetrics(SM_CXSCREEN)));
    input.mi.dy = (LONG)(g_injected_y * (65536.0 / GetSystemMetrics(SM_CYSCREEN)));
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    SendInput(1, &input, sizeof(INPUT));
}

void altcross_platform_input_inject_mouse_button(
    altcross_mouse_button_t button, int down) {
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_MOUSE;
    switch (button) {
    case ALTCROSS_MOUSE_BUTTON_LEFT:
        input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        break;
    case ALTCROSS_MOUSE_BUTTON_RIGHT:
        input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
        break;
    case ALTCROSS_MOUSE_BUTTON_MIDDLE:
        input.mi.dwFlags = down ? MOUSEEVENTF_MIDDLEDOWN : MOUSEEVENTF_MIDDLEUP;
        break;
    }
    SendInput(1, &input, sizeof(INPUT));
}

void altcross_platform_input_inject_key(altcross_keycode_t key, int down) {
    WORD vk = neutral_to_vk(key);
    if (vk == 0) {
        return;
    }
    INPUT input;
    ZeroMemory(&input, sizeof(input));
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = vk;
    input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
    SendInput(1, &input, sizeof(INPUT));
}

/* NÃO VERIFICADO EM WINDOWS DE VERDADE (mesma ressalva do resto deste
 * arquivo). GetCursorPos/SetCursorPos já usam o espaço de coordenadas
 * virtual do Windows direto, sem normalização (mesma convenção que
 * altcross_displays_enumerate já usa pro Windows, ver displays.c). */
int altcross_platform_get_cursor_position(int *out_x, int *out_y) {
    POINT p;
    if (!GetCursorPos(&p)) {
        return 1;
    }
    *out_x = p.x;
    *out_y = p.y;
    return 0;
}

int altcross_platform_warp_cursor(int x, int y) {
    return SetCursorPos(x, y) ? 0 : 1;
}
