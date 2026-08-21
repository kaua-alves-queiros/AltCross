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

typedef struct {
    altcross_display_ex_t *out;
    int max_count;
    int count;
} enum_ex_ctx_t;

static BOOL CALLBACK monitor_enum_ex_proc(HMONITOR hMonitor, HDC hdc,
                                           LPRECT rect, LPARAM lparam) {
    (void)hdc;
    (void)rect;
    enum_ex_ctx_t *ctx = (enum_ex_ctx_t *)lparam;

    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfo(hMonitor, &info)) {
        if (ctx->count < ctx->max_count) {
            altcross_display_ex_t *d = &ctx->out[ctx->count];
            /* Sem um id de monitor persistente exposto pelo Win32 de forma
             * simples (HMONITOR pode mudar entre chamadas), usamos a
             * posição na enumeração como id sintético — estável dentro
             * desta execução do processo, não sobrevive hot-plug no meio
             * (ver ressalva em displays.h). */
            d->display_id = (unsigned int)ctx->count;
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

int altcross_displays_enumerate_ex(altcross_display_ex_t *out,
                                    int max_count) {
    enum_ex_ctx_t ctx;
    ctx.out = out;
    ctx.max_count = max_count;
    ctx.count = 0;
    EnumDisplayMonitors(NULL, NULL, monitor_enum_ex_proc, (LPARAM)&ctx);
    return ctx.count;
}

/* display_id aqui é o índice sintético de altcross_displays_enumerate_ex —
 * precisamos reenumerar (EnumDisplayDevices, dessa vez) na MESMA ordem pra
 * achar o DISPLAY_DEVICE.DeviceName real que ChangeDisplaySettingsEx exige.
 * NÃO VERIFICADO EM WINDOWS DE VERDADE (mesma ressalva de platform_input.c
 * e do resto deste arquivo). */
int altcross_displays_set_origin(unsigned int display_id, int x, int y) {
    DISPLAY_DEVICE device;
    device.cb = sizeof(device);
    if (!EnumDisplayDevices(NULL, display_id, &device, 0)) {
        return 1;
    }

    DEVMODE mode;
    ZeroMemory(&mode, sizeof(mode));
    mode.dmSize = sizeof(mode);
    if (!EnumDisplaySettings(device.DeviceName, ENUM_CURRENT_SETTINGS,
                              &mode)) {
        return 1;
    }

    mode.dmFields |= DM_POSITION;
    mode.dmPosition.x = x;
    mode.dmPosition.y = y;

    LONG rc = ChangeDisplaySettingsEx(device.DeviceName, &mode, NULL,
                                       CDS_UPDATEREGISTRY | CDS_NORESET, NULL);
    if (rc != DISP_CHANGE_SUCCESSFUL) {
        return 1;
    }

    /* Aplica de fato — sem essa segunda chamada (destino NULL) as mudanças
     * com CDS_NORESET ficam só "prontas", nunca entram em vigor. */
    rc = ChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
    return rc == DISP_CHANGE_SUCCESSFUL ? 0 : 1;
}
