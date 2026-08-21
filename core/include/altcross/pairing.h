#ifndef ALTCROSS_PAIRING_H
#define ALTCROSS_PAIRING_H

#include <stddef.h>

#include "altcross/export.h"

#define ALTCROSS_DEVICE_ID_SIZE 33  /* 32 hex chars + '\0' */
#define ALTCROSS_SECRET_SIZE 65     /* 64 hex chars + '\0' */
#define ALTCROSS_DEVICE_NAME_SIZE 64
#define ALTCROSS_HOST_SIZE 46       /* cabe IPv4 e IPv6 */
#define ALTCROSS_MAX_TRUSTED_DEVICES 32

/* Um dispositivo com quem já paramos antes. device_id é estável (gerado uma
 * vez por máquina, ver altcross_pairing_local_identity_load_or_create) — o
 * host/port são só o último endereço conhecido e podem ficar defasados se a
 * outra máquina trocar de IP; a descoberta na rede reencontra o device_id no
 * IP novo e chama altcross_pairing_store_update_address, sem precisar
 * parear de novo. */
typedef struct {
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    char name[ALTCROSS_DEVICE_NAME_SIZE];
    char secret[ALTCROSS_SECRET_SIZE];
    char last_host[ALTCROSS_HOST_SIZE];
    int last_port;
} altcross_trusted_device_t;

typedef struct {
    altcross_trusted_device_t devices[ALTCROSS_MAX_TRUSTED_DEVICES];
    int count;
} altcross_pairing_store_t;

ALTCROSS_API void altcross_pairing_store_init(altcross_pairing_store_t *store);

/* Insere um dispositivo novo, ou atualiza todos os campos se device_id já
 * existir no cadastro. Retorna 0 em sucesso, diferente de zero se o cadastro
 * já estiver cheio (ALTCROSS_MAX_TRUSTED_DEVICES) e device_id for novo. */
ALTCROSS_API int altcross_pairing_store_upsert(altcross_pairing_store_t *store,
                                                const char *device_id,
                                                const char *name,
                                                const char *secret,
                                                const char *host, int port);

ALTCROSS_API const altcross_trusted_device_t *
altcross_pairing_store_find(const altcross_pairing_store_t *store,
                            const char *device_id);

/* Atualiza só last_host/last_port de um dispositivo já confiável — chamado
 * quando a descoberta encontra esse device_id respondendo de um IP
 * diferente do último conhecido. Retorna 0 em sucesso, diferente de zero se
 * device_id não estiver no cadastro (nesse caso não confiar/atualizar às
 * cegas — precisa ter pareado antes). */
ALTCROSS_API int
altcross_pairing_store_update_address(altcross_pairing_store_t *store,
                                       const char *device_id, const char *host,
                                       int port);

ALTCROSS_API int altcross_pairing_store_remove(altcross_pairing_store_t *store,
                                                const char *device_id);

/* Persistência em arquivo texto simples (1 linha por dispositivo). */
ALTCROSS_API int altcross_pairing_store_save(
    const altcross_pairing_store_t *store, const char *path);
ALTCROSS_API int altcross_pairing_store_load(altcross_pairing_store_t *store,
                                              const char *path);

/* Atalho pra quem só quer o último endereço conhecido de um dispositivo já
 * pareado, sem lidar com o store inteiro (carrega de path, procura
 * device_id) — é o que permite a tela de Arranjo perguntar as telas reais
 * de um dispositivo (ver screen_sync_protocol.h) sabendo pra onde mandar a
 * pergunta. Retorna 0 e preenche out_host/out_port em sucesso, diferente de
 * zero se o arquivo não existir ou device_id não estiver cadastrado. */
ALTCROSS_API int altcross_pairing_lookup_host(const char *device_id,
                                               const char *store_path,
                                               char *out_host,
                                               size_t out_host_size,
                                               int *out_port);

/* Código de confirmação de 6 dígitos (100000-999999) mostrado em quem RECEBE
 * o pedido de pareamento; quem pede precisa informar o mesmo código de
 * volta antes de virar dispositivo confiável — prova presença física em
 * ambas as máquinas, não só acesso de rede. */
ALTCROSS_API int altcross_pairing_generate_code(void);

/* Token secreto persistente (64 chars hex) trocado só depois da confirmação
 * do código — é essa credencial, não o código de 6 dígitos, que autentica
 * cada pacote depois do pareamento (ver "Boas Práticas" no AGENTS.md: toda
 * comunicação entre dispositivos deve ser autenticada). */
ALTCROSS_API void
altcross_pairing_generate_secret(char out_secret[ALTCROSS_SECRET_SIZE]);

/* Identidade estável desta máquina: lê device_id de path se o arquivo já
 * existir, ou gera um novo e grava em path na primeira vez. Chamar sempre
 * com o mesmo path faz a mesma máquina ter sempre o mesmo device_id — é
 * isso que permite reconhecer "essa é a mesma máquina de antes" mesmo que o
 * IP tenha mudado. Retorna 0 em sucesso. */
ALTCROSS_API int altcross_pairing_local_identity_load_or_create(
    const char *path, char out_device_id[ALTCROSS_DEVICE_ID_SIZE]);

#endif
