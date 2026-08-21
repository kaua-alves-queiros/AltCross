#include "altcross/net_socket.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
typedef SOCKET altcross_raw_socket_t;
#define ALTCROSS_INVALID_SOCKET INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
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

static int wait_readable(altcross_raw_socket_t fd, int timeout_ms) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);

    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;

    return select((int)(fd + 1), &read_fds, NULL, NULL, &tv);
}

int altcross_socket_receive(altcross_socket_t *sock, uint8_t *buf,
                             size_t buf_size, int timeout_ms) {
    int ready = wait_readable(sock->fd, timeout_ms);
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

int altcross_socket_receive_from(altcross_socket_t *sock, uint8_t *buf,
                                  size_t buf_size, int timeout_ms,
                                  char *out_host, size_t out_host_size,
                                  int *out_port) {
    int ready = wait_readable(sock->fd, timeout_ms);
    if (ready == 0) {
        return ALTCROSS_SOCKET_TIMEOUT;
    }
    if (ready < 0) {
        return ALTCROSS_SOCKET_ERROR;
    }

    struct sockaddr_in from_addr;
    socklen_t from_len = sizeof(from_addr);
    long received = recvfrom(sock->fd, (char *)buf, (int)buf_size, 0,
                              (struct sockaddr *)&from_addr, &from_len);
    if (received < 0) {
        return ALTCROSS_SOCKET_ERROR;
    }

    if (out_host && out_host_size > 0) {
        if (!inet_ntop(AF_INET, &from_addr.sin_addr, out_host,
                        (socklen_t)out_host_size)) {
            out_host[0] = '\0';
        }
    }
    if (out_port) {
        *out_port = ntohs(from_addr.sin_port);
    }
    return (int)received;
}

int altcross_socket_enable_broadcast(altcross_socket_t *sock) {
    int enable = 1;
    return setsockopt(sock->fd, SOL_SOCKET, SO_BROADCAST,
                       (const char *)&enable, sizeof(enable));
}

#if defined(_WIN32)
int altcross_net_list_broadcast_addresses(char *out, size_t out_addr_size,
                                           int max_count) {
    ULONG buf_len = 15000;
    PIP_ADAPTER_ADDRESSES addresses = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
    if (!addresses) {
        return 0;
    }

    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                  GAA_FLAG_SKIP_DNS_SERVER;
    ULONG ret =
        GetAdaptersAddresses(AF_INET, flags, NULL, addresses, &buf_len);
    if (ret == ERROR_BUFFER_OVERFLOW) {
        free(addresses);
        addresses = (PIP_ADAPTER_ADDRESSES)malloc(buf_len);
        if (!addresses) {
            return 0;
        }
        ret = GetAdaptersAddresses(AF_INET, flags, NULL, addresses, &buf_len);
    }
    if (ret != NO_ERROR) {
        free(addresses);
        return 0;
    }

    int count = 0;
    for (PIP_ADAPTER_ADDRESSES adapter = addresses; adapter;
         adapter = adapter->Next) {
        if (adapter->OperStatus != IfOperStatusUp) {
            continue;
        }
        if (adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            continue;
        }

        for (PIP_ADAPTER_UNICAST_ADDRESS unicast =
                 adapter->FirstUnicastAddress;
             unicast; unicast = unicast->Next) {
            if (unicast->Address.lpSockaddr->sa_family != AF_INET) {
                continue;
            }

            struct sockaddr_in *sin =
                (struct sockaddr_in *)(void *)unicast->Address.lpSockaddr;
            ULONG prefix_len = unicast->OnLinkPrefixLength;
            uint32_t ip = ntohl(sin->sin_addr.s_addr);
            uint32_t mask =
                (prefix_len == 0) ? 0u : (0xFFFFFFFFu << (32 - prefix_len));
            uint32_t broadcast = ip | ~mask;

            struct in_addr broadcast_addr;
            broadcast_addr.s_addr = htonl(broadcast);

            char addr_str[ALTCROSS_BROADCAST_ADDRESS_SIZE];
            if (!inet_ntop(AF_INET, &broadcast_addr, addr_str,
                            sizeof(addr_str))) {
                continue;
            }

            if (count < max_count) {
                snprintf(out + (size_t)count * out_addr_size, out_addr_size,
                         "%s", addr_str);
            }
            count++;
        }
    }

    free(addresses);
    return count;
}
#else
int altcross_net_list_broadcast_addresses(char *out, size_t out_addr_size,
                                           int max_count) {
    struct ifaddrs *ifaddr;
    if (getifaddrs(&ifaddr) != 0) {
        return 0;
    }

    int count = 0;
    for (struct ifaddrs *ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr || ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (!(ifa->ifa_flags & IFF_UP) || (ifa->ifa_flags & IFF_LOOPBACK)) {
            continue;
        }
        if (!(ifa->ifa_flags & IFF_BROADCAST) || !ifa->ifa_broadaddr) {
            continue;
        }

        struct sockaddr_in *broadcast_addr =
            (struct sockaddr_in *)(void *)ifa->ifa_broadaddr;
        char addr_str[ALTCROSS_BROADCAST_ADDRESS_SIZE];
        if (!inet_ntop(AF_INET, &broadcast_addr->sin_addr, addr_str,
                        sizeof(addr_str))) {
            continue;
        }

        if (count < max_count) {
            snprintf(out + (size_t)count * out_addr_size, out_addr_size, "%s",
                      addr_str);
        }
        count++;
    }

    freeifaddrs(ifaddr);
    return count;
}
#endif

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
