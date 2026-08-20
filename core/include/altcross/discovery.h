#ifndef ALTCROSS_DISCOVERY_H
#define ALTCROSS_DISCOVERY_H

#include <stddef.h>
#include <stdint.h>

#include "altcross/export.h"
#include "altcross/pairing.h"

/* Descoberta de máquinas AltCross na rede local. NÃO é mDNS/DNS-SD de
 * verdade — é um protocolo de broadcast UDP próprio e simples (ver
 * "Fluxo de Desenvolvimento" no AGENTS.md pra decisão e trade-off). Uma
 * máquina manda ALTCROSS_DISCOVERY_MSG_QUERY em broadcast na porta
 * ALTCROSS_DISCOVERY_PORT; toda máquina rodando o AltCross que receber
 * responde com ALTCROSS_DISCOVERY_MSG_REPLY (unicast, de volta pra quem
 * perguntou) contendo seu device_id/nome/porta de pareamento. */
#define ALTCROSS_DISCOVERY_PORT 45201
#define ALTCROSS_DISCOVERY_VERSION 1
#define ALTCROSS_DISCOVERY_MAX_SIZE (4 + ALTCROSS_DEVICE_ID_SIZE + 2 + 1 + \
                                      ALTCROSS_DEVICE_NAME_SIZE)

typedef enum {
    ALTCROSS_DISCOVERY_MSG_QUERY = 1,
    ALTCROSS_DISCOVERY_MSG_REPLY = 2,
} altcross_discovery_msg_type_t;

typedef struct {
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    char name[ALTCROSS_DEVICE_NAME_SIZE];
    int port; /* porta de pareamento/controle de quem respondeu */
} altcross_discovery_reply_t;

/* Serializa uma pergunta de descoberta. from_device_id é o device_id de
 * quem está perguntando (permite ignorar a própria pergunta ecoada de
 * volta). Retorna bytes escritos, ou -1 se buf_size for pequeno demais. */
ALTCROSS_API int altcross_discovery_encode_query(const char *from_device_id,
                                                   uint8_t *buf,
                                                   size_t buf_size);

/* Serializa uma resposta de descoberta. Retorna bytes escritos, ou -1 em
 * erro (buf pequeno demais, ou device_id/name maiores que os limites de
 * altcross/pairing.h). */
ALTCROSS_API int altcross_discovery_encode_reply(const char *device_id,
                                                   const char *name, int port,
                                                   uint8_t *buf,
                                                   size_t buf_size);

/* Decodifica uma mensagem de descoberta. Preenche *out_type sempre; se for
 * MSG_QUERY, preenche out_query_from_device_id (deve ter pelo menos
 * ALTCROSS_DEVICE_ID_SIZE bytes); se for MSG_REPLY, preenche
 * *out_reply. Retorna 1 em sucesso, 0 se os dados forem inválidos. */
ALTCROSS_API int altcross_discovery_decode(
    const uint8_t *buf, size_t len, altcross_discovery_msg_type_t *out_type,
    char *out_query_from_device_id, altcross_discovery_reply_t *out_reply);

#endif
