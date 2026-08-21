#ifndef ALTCROSS_HANDOFF_PROTOCOL_H
#define ALTCROSS_HANDOFF_PROTOCOL_H

#include "altcross/export.h"
#include "altcross/net_socket.h"
#include "altcross/pairing.h"
#include "altcross/protocol.h"
#include "altcross/sha256.h"

/* Envelope de rede que AUTENTICA cada altcross_packet_t (ver protocol.h)
 * trocado durante um handoff real de mouse/teclado entre 2 máquinas —
 * porta própria, separada de descoberta/pareamento/screen-sync. Sem isso,
 * qualquer coisa na rede local poderia mandar MOUSE_DELTA/KEY falsos pro
 * dispositivo de destino (ver AGENTS.md: "autenticar de fato o tráfego de
 * mouse/teclado... hoje esse segredo é gerado e persistido, mas nada
 * valida ele antes de aceitar pacotes" — isso aqui é o que resolve isso).
 *
 * Formato: magic(4) + versão(1) + device_id de quem mandou(33) +
 * sequência(4, anti-replay) + HMAC-SHA256(payload, segredo compartilhado
 * do pareamento)(32) + altcross_packet_t codificado (protocol.c). */
#define ALTCROSS_HANDOFF_PORT 45204
#define ALTCROSS_HANDOFF_SECRET_SIZE 32

typedef struct {
    char sender_device_id[ALTCROSS_DEVICE_ID_SIZE];
    uint32_t seq;
    altcross_packet_t packet;
} altcross_handoff_received_t;

/* Assina pkt com secret (32 bytes brutos — não a string hex de 64 chars
 * que altcross_pairing_store_t guarda; ver altcross_handoff_secret_from_hex)
 * e manda pra peer_host:ALTCROSS_HANDOFF_PORT. seq deve ser estritamente
 * crescente por sessão de quem manda — ver altcross_handoff_replay_check
 * do lado de quem recebe. Retorna 0 em sucesso. */
ALTCROSS_API int altcross_handoff_send(
    altcross_socket_t *sock, const char *peer_host, const char *my_device_id,
    const uint8_t secret[ALTCROSS_HANDOFF_SECRET_SIZE], uint32_t seq,
    const altcross_packet_t *pkt);

/* Resolve o segredo certo pra validar um envelope recebido de
 * sender_device_id — normalmente altcross_pairing_store_find nesse
 * device_id. Retorna 1 e preenche out_secret se esse device_id for
 * confiável, 0 caso contrário (o pacote correspondente é descartado sem
 * processar, nem chega a decodificar o payload). */
typedef int (*altcross_handoff_secret_lookup_t)(
    const char *sender_device_id,
    uint8_t out_secret[ALTCROSS_HANDOFF_SECRET_SIZE], void *user_data);

/* Espera até timeout_ms por um envelope válido (magic/versão batendo, HMAC
 * batendo com o segredo que lookup_secret resolver). Retorna 1 e preenche
 * *out em sucesso; retorna 0 se: timeout, pacote malformado, device_id
 * desconhecido (lookup_secret devolveu 0), ou HMAC não bateu (adulterado
 * ou segredo errado) — em QUALQUER desses casos o pacote é simplesmente
 * ignorado, sem distinguir o motivo pra quem chama (não vaza informação
 * sobre por que um pacote foi rejeitado). NÃO faz verificação de replay —
 * ver altcross_handoff_replay_check, separado de propósito (estado por
 * sessão fica com quem chama). */
ALTCROSS_API int altcross_handoff_receive(
    altcross_socket_t *sock, int timeout_ms,
    altcross_handoff_secret_lookup_t lookup_secret, void *user_data,
    altcross_handoff_received_t *out);

/* Guarda anti-replay: um contador "última sequência aceita" por
 * remetente. Chame antes de confiar num altcross_handoff_received_t já
 * validado por HMAC — retorna 1 (aceita, atualiza *last_seq) se seq for
 * MAIOR que o último aceito, 0 (rejeita — repetido ou fora de ordem) caso
 * contrário. *last_seq deve começar em 0 (nenhum pacote aceito ainda). */
ALTCROSS_API int altcross_handoff_replay_check(uint32_t *last_seq,
                                                uint32_t seq);

#endif
