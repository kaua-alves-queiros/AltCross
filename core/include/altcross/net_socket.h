#ifndef ALTCROSS_NET_SOCKET_H
#define ALTCROSS_NET_SOCKET_H

#include <stddef.h>
#include <stdint.h>

#include "altcross/export.h"

typedef struct altcross_socket altcross_socket_t;

/* Abre um socket UDP e faz bind em local_port (0 = deixa o SO escolher uma
 * porta livre, útil em testes). Retorna NULL em caso de erro. */
ALTCROSS_API altcross_socket_t *altcross_socket_open_udp(int local_port);

/* Porta local de fato atribuída pelo SO (útil quando local_port foi 0). */
ALTCROSS_API int altcross_socket_local_port(const altcross_socket_t *sock);

/* Envia data (len bytes) para host:port. Retorna o número de bytes enviados,
 * ou -1 em erro. */
ALTCROSS_API int altcross_socket_send_to(altcross_socket_t *sock,
                                          const char *host, int port,
                                          const uint8_t *data, size_t len);

/* Espera até timeout_ms por um datagrama e o copia para buf (até buf_size
 * bytes). Retorna o número de bytes recebidos (>= 0; um datagrama vazio de
 * verdade também retorna 0), ALTCROSS_SOCKET_TIMEOUT se estourar o timeout
 * sem receber nada, ou ALTCROSS_SOCKET_ERROR em erro. */
#define ALTCROSS_SOCKET_TIMEOUT (-2)
#define ALTCROSS_SOCKET_ERROR (-1)
ALTCROSS_API int altcross_socket_receive(altcross_socket_t *sock, uint8_t *buf,
                                          size_t buf_size, int timeout_ms);

/* Igual altcross_socket_receive, mas também informa de onde veio o
 * datagrama (out_host precisa ter pelo menos out_host_size bytes) —
 * necessário pra descoberta, onde não se sabe de antemão quem vai
 * responder. */
ALTCROSS_API int altcross_socket_receive_from(altcross_socket_t *sock,
                                               uint8_t *buf, size_t buf_size,
                                               int timeout_ms, char *out_host,
                                               size_t out_host_size,
                                               int *out_port);

/* Habilita o envio de datagramas broadcast (SO_BROADCAST) — sem isso,
 * mandar pra um endereço de broadcast (ex.: 255.255.255.255) falha.
 * Retorna 0 em sucesso. No macOS/Windows isso é o que aciona o prompt de
 * permissão de Rede Local na primeira vez que um pacote é enviado de
 * verdade — só chamar isso a partir de uma ação explícita do usuário (ex.:
 * botão "Buscar dispositivos"), nunca automaticamente ao abrir uma tela. */
ALTCROSS_API int altcross_socket_enable_broadcast(altcross_socket_t *sock);

ALTCROSS_API void altcross_socket_close(altcross_socket_t *sock);

#endif
