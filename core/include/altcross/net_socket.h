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

#define ALTCROSS_MAX_NETWORK_INTERFACES 16
#define ALTCROSS_BROADCAST_ADDRESS_SIZE 46 /* cabe um IPv4 em texto */

/* Lista o endereço de broadcast dirigido (ex.: "192.168.1.255") de cada
 * interface de rede IPv4 ativa e não-loopback desta máquina. Existe porque
 * mandar só pra 255.255.255.255 depende de qual interface o SO escolhe pela
 * tabela de rotas — numa máquina com várias interfaces (VPN, VirtualBox,
 * Docker, Hyper-V, WSL etc.) isso pode escolher a errada e o broadcast nunca
 * chegar na rede de verdade. Mandar pro endereço dirigido de cada interface
 * garante que sai em todas.
 *
 * Preenche out (buffer contíguo, max_count posições de out_addr_size bytes
 * cada). Retorna a quantidade de interfaces encontradas (pode ser maior que
 * max_count; só as primeiras max_count são escritas). */
ALTCROSS_API int altcross_net_list_broadcast_addresses(char *out,
                                                        size_t out_addr_size,
                                                        int max_count);

#endif
