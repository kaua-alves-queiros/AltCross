/* Ferramenta segura de verificação manual: só lista os monitores físicos
 * detectados, não captura mouse/teclado nem abre nenhuma porta de rede. */
#include <stdio.h>

#include "altcross/displays.h"

int main(void) {
    altcross_display_t displays[ALTCROSS_MAX_DISPLAYS];
    int count = altcross_displays_enumerate(displays, ALTCROSS_MAX_DISPLAYS);

    printf("%d monitor(es) encontrado(s):\n", count);
    int shown = count < ALTCROSS_MAX_DISPLAYS ? count : ALTCROSS_MAX_DISPLAYS;
    for (int i = 0; i < shown; i++) {
        printf("  [%d] x=%.0f y=%.0f w=%.0f h=%.0f primario=%s\n", i,
               displays[i].x, displays[i].y, displays[i].width,
               displays[i].height, displays[i].is_primary ? "sim" : "nao");
    }
    return 0;
}
