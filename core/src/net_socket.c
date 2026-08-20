#include "altcross/net_socket.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET altcross_raw_socket_t;
#define ALTCROSS_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int altcross_raw_socket_t;
#define ALTCROSS_INVALID_SOCKET (-1)
#endif

struct altcross_socket {
    altcross_raw_socket_t fd;
};

#if defined(_WIN32)
static int winsock_ensure_initialized(void) {
    static int initialized = 0;
    if (!initialized) {
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
            return 0;
        }
        initialized = 1;
    }
    return 1;
}
#endif

altcross_socket_t *altcross_socket_open_udp(int local_port) {
#if defined(_WIN32)
    if (!winsock_ensure_initialized()) {
        return NULL;
    }
#endif

    altcross_raw_socket_t fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == ALTCROSS_INVALID_SOCKET) {
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t)local_port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
#if defined(_WIN32)
        closesocket(fd);
#else
        close(fd);
#endif
        return NULL;
    }

    altcross_socket_t *sock = (altcross_socket_t *)malloc(sizeof(*sock));
    if (!sock) {
#if defined(_WIN32)
        closesocket(fd);
#else
        close(fd);
#endif
        return NULL;
    }
    sock->fd = fd;
    return sock;
}

int altcross_socket_local_port(const altcross_socket_t *sock) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    if (getsockname(sock->fd, (struct sockaddr *)&addr, &addr_len) != 0) {
        return -1;
    }
    return ntohs(addr.sin_port);
}

int altcross_socket_send_to(altcross_socket_t *sock, const char *host,
                             int port, const uint8_t *data, size_t len) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        return ALTCROSS_SOCKET_ERROR;
    }

    long sent = sendto(sock->fd, (const char *)data, (int)len, 0,
                        (struct sockaddr *)&addr, sizeof(addr));
    if (sent < 0) {
        return ALTCROSS_SOCKET_ERROR;
    }
    return (int)sent;
}

int altcross_socket_receive(altcross_socket_t *sock, uint8_t *buf,
                             size_t buf_size, int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sock->fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    int ready = select((int)(sock->fd + 1), &read_fds, NULL, NULL, &tv);
    if (ready == 0) {
        return ALTCROSS_SOCKET_TIMEOUT;
    }
    if (ready < 0) {
        return ALTCROSS_SOCKET_ERROR;
    }

    long received =
        recvfrom(sock->fd, (char *)buf, (int)buf_size, 0, NULL, NULL);
    if (received < 0) {
        return ALTCROSS_SOCKET_ERROR;
    }
    return (int)received;
}

void altcross_socket_close(altcross_socket_t *sock) {
    if (!sock) {
        return;
    }
#if defined(_WIN32)
    closesocket(sock->fd);
#else
    close(sock->fd);
#endif
    free(sock);
}
