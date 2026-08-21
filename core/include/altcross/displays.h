#ifndef ALTCROSS_DISPLAYS_H
#define ALTCROSS_DISPLAYS_H

#include "altcross/export.h"

/* Um monitor físico conectado à máquina, em coordenadas do "espaço virtual"
 * do SO (origem no canto superior esquerdo do monitor principal, Y crescendo
 * pra baixo — mesma convenção do Windows; a implementação macOS normaliza
 * pra isso, já que o AppKit usa Y crescendo pra cima por padrão). É essa
 * mesma convenção de coordenadas que a tela de arranjo de monitores do
 * Flutter usa pra desenhar os retângulos na escala certa. */
typedef struct {
    double x;
    double y;
    double width;
    double height;
    int is_primary;
} altcross_display_t;

#define ALTCROSS_MAX_DISPLAYS 16

/* Preenche out (até max_count elementos) com os monitores físicos conectados
 * nesta máquina agora. Retorna a quantidade real de monitores encontrados
 * (pode ser maior que max_count; nesse caso só os primeiros max_count são
 * escritos em out, mas o retorno reflete o total real). */
ALTCROSS_API int altcross_displays_enumerate(altcross_display_t *out,
                                              int max_count);

#endif
