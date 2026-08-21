#ifndef ALTCROSS_HANDOFF_H
#define ALTCROSS_HANDOFF_H

#include "altcross/export.h"
#include "altcross/hotzone.h"
#include "altcross/pairing.h"

/* Liga a lógica pura já testada (hotzone.h/input_control.h), a captura/
 * injeção real de mouse+teclado (platform_input.h) e o transporte
 * autenticado (handoff_protocol.h) num daemon controlável a partir do
 * Flutter — mesma coisa que tools/altcrossd.c já fazia manualmente, só que
 * chamável por start()/stop() em vez de um binário à parte, e usando
 * hotzones/telas de VERDADE (não hardcoded via CLI).
 *
 * *** ATENÇÃO ***
 * altcross_handoff_start() instala o hook global de mouse/teclado de
 * verdade (mesma ressalva de altcross_platform_input_start) — a partir daí
 * o mouse/teclado desta máquina passa a poder ser roteado pra outra quando
 * cruzar uma hotzone configurada. Só deve ser chamado a partir de uma ação
 * explícita do usuário na UI, com aviso antes (ver AGENTS.md) — nunca
 * automaticamente ao abrir o app. */
#define ALTCROSS_HANDOFF_MAX_ZONES 16

typedef struct {
    altcross_edge_t edge;
    char target_device_id[ALTCROSS_DEVICE_ID_SIZE];
    /* Tela REAL do dispositivo remoto (ver altcross_screen_sync_query_screens)
     * — usada pra escalar a posição de entrada/saída do cursor entre telas
     * de tamanhos diferentes. corner_size não é usado do lado remoto, pode
     * ser 0. */
    altcross_screen_config_t target_screen;
} altcross_handoff_zone_t;

/* Sobe o handoff de verdade: local_screen é a tela combinada local (mesma
 * convenção de altcross_input_control_init), zones as bordas configuradas
 * (targetScreenIndex ainda não é usado aqui — sempre a tela combinada
 * inteira do alvo, ver AGENTS.md sobre granularidade por tela ainda não
 * implementada). my_device_id é a identidade estável desta máquina
 * (altcross_pairing_local_identity_load_or_create) e pairing_store_path o
 * cadastro de dispositivos confiáveis — de onde vem o segredo compartilhado
 * usado pra assinar/validar cada pacote (ver handoff_protocol.h). Retorna 0
 * em sucesso, diferente de zero se: já está rodando, hook de captura falhou
 * (macOS sem permissão de Acessibilidade), ou não conseguiu abrir a porta.
 */
ALTCROSS_API int altcross_handoff_start(
    const altcross_screen_config_t *local_screen,
    const altcross_handoff_zone_t *zones, int zone_count,
    const char *my_device_id, const char *pairing_store_path);

ALTCROSS_API void altcross_handoff_stop(void);

/* 1 se o handoff está rodando agora (start() já chamado com sucesso e
 * stop() ainda não), 0 caso contrário — pra UI saber o estado real ao
 * reabrir a tela, já que start()/stop() só são chamados a partir de uma
 * ação explícita do usuário e o estado precisa sobreviver a navegação. */
ALTCROSS_API int altcross_handoff_is_active(void);

/* 1 se o controle está atualmente em outra máquina (mouse/teclado local
 * capturados e sendo roteados), 0 se está local — pra UI mostrar um
 * indicador de estado. Seguro de chamar a qualquer momento, mesmo com o
 * handoff parado (sempre 0 nesse caso). */
ALTCROSS_API int altcross_handoff_is_remote(void);

#endif
