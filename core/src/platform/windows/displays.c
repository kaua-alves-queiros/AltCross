/* Enumeração real dos monitores físicos conectados, via Win32
 * (EnumDisplayMonitors/GetMonitorInfo). NÃO VERIFICADO EM WINDOWS DE VERDADE
 * — mesma ressalva do platform_input.c desta pasta (ver comentário lá).
 * O espaço de coordenadas virtual do Windows já usa origem no canto superior
 * esquerdo do monitor primário com Y pra baixo, então não precisa de
 * normalização nenhuma (compare com displays.m do macOS). */
#include "altcross/displays.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef struct {
    altcross_display_t *out;
    int max_count;
    int count;
} enum_ctx_t;

static BOOL CALLBACK monitor_enum_proc(HMONITOR hMonitor, HDC hdc,
                                        LPRECT rect, LPARAM lparam) {
    (void)hdc;
    (void)rect;
    enum_ctx_t *ctx = (enum_ctx_t *)lparam;

    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfo(hMonitor, &info)) {
        if (ctx->count < ctx->max_count) {
            altcross_display_t *d = &ctx->out[ctx->count];
            d->x = info.rcMonitor.left;
            d->y = info.rcMonitor.top;
            d->width = info.rcMonitor.right - info.rcMonitor.left;
            d->height = info.rcMonitor.bottom - info.rcMonitor.top;
            d->is_primary = (info.dwFlags & MONITORINFOF_PRIMARY) ? 1 : 0;
        }
        ctx->count++;
    }
    return TRUE;
}

int altcross_displays_enumerate(altcross_display_t *out, int max_count) {
    enum_ctx_t ctx;
    ctx.out = out;
    ctx.max_count = max_count;
    ctx.count = 0;
    EnumDisplayMonitors(NULL, NULL, monitor_enum_proc, (LPARAM)&ctx);
    return ctx.count;
}
