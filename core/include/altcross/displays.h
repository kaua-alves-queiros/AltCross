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

/* Igual altcross_display_t, mas com um id estável (dentro desta execução do
 * processo) que identifica QUAL monitor físico é — necessário só quando se
 * vai *reposicionar* um monitor de verdade (altcross_displays_set_origin),
 * já que "índice na lista" sozinho não é suficiente pra apontar um monitor
 * específico depois de outras chamadas. No macOS é o CGDirectDisplayID de
 * verdade (estável até o próximo hot-plug); no Windows é um índice sintético
 * (posição em EnumDisplayMonitors) — não sobrevive hot-plug/replug no meio
 * do processo, mas é real dentro de uma sessão (mesma ressalva de
 * plataforma já documentada em platform/windows/displays.c). */
typedef struct {
    unsigned int display_id;
    double x;
    double y;
    double width;
    double height;
    int is_primary;
} altcross_display_ex_t;

ALTCROSS_API int altcross_displays_enumerate_ex(altcross_display_ex_t *out,
                                                  int max_count);

/* Reposiciona DE VERDADE, no sistema operacional, o monitor físico
 * identificado por display_id pra que o canto superior esquerdo dele fique
 * em (x, y) — mesmo espaço de coordenadas que altcross_displays_enumerate
 * devolve (Y crescendo pra baixo, ver comentário no topo deste arquivo).
 * Isso muda a configuração real de telas do SO (afeta todo mundo, não só
 * este app) — é o que faz "conectar 2 telas locais" no Arranjo virar
 * arrumação de verdade, não só visual.
 *
 * No macOS precisa que o app NÃO esteja com App Sandbox habilitado — o
 * sistema bloqueia CGConfigureDisplayOrigin de dentro do sandbox sem
 * exceção nenhuma, não tem entitlement que libere isso (ver
 * macos/Runner entitlements). Retorna 0 em sucesso, diferente de zero se
 * o SO recusou (inclui: sandboxed no macOS, display_id não existe mais). */
ALTCROSS_API int altcross_displays_set_origin(unsigned int display_id,
                                                int x, int y);

#endif
