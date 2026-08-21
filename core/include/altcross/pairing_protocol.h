#ifndef ALTCROSS_PAIRING_PROTOCOL_H
#define ALTCROSS_PAIRING_PROTOCOL_H

#include "altcross/export.h"
#include "altcross/pairing.h"

/* Handshake de pareamento de verdade pela rede, usando a lógica pura de
 * altcross/pairing.h (código de confirmação, segredo, cadastro persistente).
 * Fluxo (como pareamento de Apple TV/Bluetooth — prova presença física nas
 * duas máquinas, não só acesso de rede):
 *
 *   A (quem inicia)                          B (quem recebe)
 *   ---------------                          ---------------
 *   altcross_pairing_send_request()  ---->   responder gera um código de 6
 *                                             dígitos e guarda como "pedido
 *                                             pendente" (ver poll_incoming_
 *                                             request) — a UI de B mostra
 *                                             esse código na tela
 *   usuário lê o código em B e digita
 *   em A
 *   altcross_pairing_confirm(código) ---->   responder confere o código;
 *                                             se bateu, gera um segredo,
 *                                             salva B como confiável e
 *                                             responde ACCEPT com o segredo
 *   (recebe o segredo, salva A como
 *    confiável com esse mesmo segredo)
 *
 * A partir daí os dois lados têm o mesmo segredo persistido (sobrevive a
 * troca de IP, ver altcross/pairing.h) — é essa credencial, não o código de
 * 6 dígitos, que deve autenticar o tráfego de verdade depois (ainda não
 * conectado a altcrossd/protocol.h nesta versão). */
#define ALTCROSS_PAIRING_PORT 45202

typedef struct {
    int pending;
    char requester_device_id[ALTCROSS_DEVICE_ID_SIZE];
    char requester_name[ALTCROSS_DEVICE_NAME_SIZE];
    int code;
} altcross_pairing_incoming_request_t;

/* Notificação pro lado de quem RECEBEU o pedido (B): o CONFIRM chegou com o
 * código certo e o pareamento foi salvo como confiável — sem isso, B nunca
 * fica sabendo que A digitou o código certo e terminou o pareamento; só A
 * sabe (via altcross_pairing_confirm retornando 1). */
typedef struct {
    int pending;
    char peer_device_id[ALTCROSS_DEVICE_ID_SIZE];
    char peer_name[ALTCROSS_DEVICE_NAME_SIZE];
} altcross_pairing_completed_t;

/* Sobe a thread de fundo que escuta ALTCROSS_PAIRING_PORT e processa
 * REQUEST/CONFIRM de verdade pela rede. my_device_id/my_name são copiados
 * internamente. store_path é onde o cadastro de dispositivos confiáveis é
 * lido/salvo (altcross_pairing_store_load/save) quando um pareamento é
 * aceito. Retorna 0 em sucesso, diferente de zero se já tiver uma thread
 * rodando ou não conseguir abrir a porta. */
ALTCROSS_API int altcross_pairing_start_responder(const char *my_device_id,
                                                    const char *my_name,
                                                    const char *store_path);

ALTCROSS_API void altcross_pairing_stop_responder(void);

/* Consome (lê e limpa a flag `pending`) o pedido de pareamento recebido,
 * se houver — é assim que a UI descobre que precisa mostrar um código de
 * confirmação na tela. Não cancela o pedido do lado do responder (ele
 * continua esperando o CONFIRM); só avisa a UI. Retorna 1 se havia um
 * pedido pendente (preenche *out), 0 caso contrário. */
ALTCROSS_API int altcross_pairing_poll_incoming_request(
    altcross_pairing_incoming_request_t *out);

/* Consome (lê e limpa a flag `pending`) a notificação de pareamento
 * concluído, do lado de quem RECEBEU o pedido — é assim que a UI de B
 * descobre que A confirmou o código certo e já pode mostrar sucesso /
 * cadastrar a conexão do lado dela também. Retorna 1 se havia uma
 * notificação pendente (preenche *out), 0 caso contrário. */
ALTCROSS_API int altcross_pairing_poll_completed(
    altcross_pairing_completed_t *out);

/* Lado de quem inicia: manda um PAIR_REQUEST de verdade pra peer_host (porta
 * fixa ALTCROSS_PAIRING_PORT). Não espera resposta — o código aparece do
 * lado de lá; o usuário precisa olhar a tela do outro dispositivo. Retorna
 * 0 se conseguiu mandar. */
ALTCROSS_API int altcross_pairing_send_request(const char *my_device_id,
                                                 const char *my_name,
                                                 const char *peer_host);

/* Depois que o usuário digitou o código mostrado no outro lado, manda
 * PAIR_CONFIRM pra peer_host e espera até timeout_ms pela resposta. Se
 * aceito, já salva o dispositivo confiável (peer) em store_path com o
 * segredo recebido, e preenche out_device_id/out_name (buffers de pelo
 * menos ALTCROSS_DEVICE_ID_SIZE/ALTCROSS_DEVICE_NAME_SIZE — podem ser NULL
 * se não precisar). Retorna 1 se pareado com sucesso, 0 se rejeitado
 * (código errado) ou sem pedido pendente do lado de lá, -1 em erro de
 * rede/timeout. */
ALTCROSS_API int altcross_pairing_confirm(const char *my_device_id,
                                           const char *peer_host, int code,
                                           int timeout_ms,
                                           const char *store_path,
                                           char *out_device_id,
                                           char *out_name);

#endif
