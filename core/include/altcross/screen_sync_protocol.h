#ifndef ALTCROSS_SCREEN_SYNC_PROTOCOL_H
#define ALTCROSS_SCREEN_SYNC_PROTOCOL_H

#include "altcross/displays.h"
#include "altcross/export.h"
#include "altcross/pairing.h"

/* Handshake de verdade pela rede pra 2 coisas que a tela de Arranjo do
 * Flutter precisava e não tinha:
 *
 *   1) Perguntar pra um dispositivo já pareado quais são as telas FÍSICAS
 *      reais dele agora (não um tamanho fixo chutado) — SCREENS_QUERY /
 *      SCREENS_REPLY, reaproveitando `altcross_displays_enumerate` (mesma
 *      função que já lê os monitores reais desta máquina, ver displays.h).
 *
 *   2) Avisar o outro lado quando EU conecto uma borda minha nele, pra ele
 *      também salvar a conexão (espelhada) do lado dele, em vez de cada
 *      lado precisar configurar a mesma ligação 2 vezes — ZONE_PUSH.
 *
 * Porta própria (separada de descoberta/pareamento), unicast direto — mesmo
 * padrão de pairing_protocol.h: testável de ponta a ponta em 127.0.0.1 sem
 * risco de prompt de permissão (não é broadcast). */
#define ALTCROSS_SCREEN_SYNC_PORT 45203

typedef struct {
    int pending;
    char sender_device_id[ALTCROSS_DEVICE_ID_SIZE];
    char sender_name[ALTCROSS_DEVICE_NAME_SIZE];
    /* Índice de HotZoneEdge do lado Dart (ver HotZoneEdge em hot_zone.dart)
     * — a semântica da borda mora só no Dart; o Core só carrega o número. */
    int sender_edge;
    /* Qual das MINHAS telas (na ordem que EU devolvo pra ele quando ele me
     * pergunta via SCREENS_QUERY) o lado de lá escolheu como alvo. */
    int sender_target_screen_index;
} altcross_screen_sync_incoming_zone_t;

/* Sobe a thread de fundo que escuta ALTCROSS_SCREEN_SYNC_PORT e responde
 * SCREENS_QUERY com as telas físicas reais desta máquina, além de guardar
 * ZONE_PUSH recebido pra poll_incoming_zone consumir. Seguro de ligar
 * automaticamente — só responde perguntas explícitas, não captura input
 * nenhum (mesma categoria seguraça de discovery/pairing responders, ver
 * AGENTS.md). Retorna 0 em sucesso. */
ALTCROSS_API int altcross_screen_sync_start_responder(
    const char *my_device_id, const char *my_name);

ALTCROSS_API void altcross_screen_sync_stop_responder(void);

/* Consome (lê e limpa) uma notificação de ZONE_PUSH recebida, se houver.
 * Retorna 1 se havia uma pendente (preenche *out), 0 caso contrário. */
ALTCROSS_API int altcross_screen_sync_poll_incoming_zone(
    altcross_screen_sync_incoming_zone_t *out);

/* Pergunta pra peer_host quais são as telas físicas reais dele agora.
 * Bloqueia até timeout_ms esperando a resposta. Preenche out (até
 * max_count elementos) e retorna a quantidade real recebida, 0 se não
 * respondeu nada (não deveria acontecer com contagem >=1, já que toda
 * máquina tem pelo menos 1 tela), ou -1 em erro/timeout de rede. */
ALTCROSS_API int altcross_screen_sync_query_screens(const char *peer_host,
                                                      int timeout_ms,
                                                      altcross_display_t *out,
                                                      int max_count);

/* Avisa peer_host que EU (my_device_id/my_name) acabei de conectar minha
 * borda my_edge (índice de HotZoneEdge) numa tela dele (target_screen_index,
 * na ordem que ELE devolve as telas dele). Não espera resposta — é
 * fire-and-forget, igual altcross_pairing_send_request. Retorna 0 se
 * conseguiu mandar o pacote. */
ALTCROSS_API int altcross_screen_sync_push_zone(const char *peer_host,
                                                  const char *my_device_id,
                                                  const char *my_name,
                                                  int my_edge,
                                                  int target_screen_index);

#endif
