#include <string.h>

#include "altcross/net_socket.h"
#include "test_framework.h"

static void test_open_assigns_a_local_port_when_zero_is_requested(void) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    ASSERT_TRUE(sock != NULL);
    ASSERT_TRUE(altcross_socket_local_port(sock) > 0);
    altcross_socket_close(sock);
}

static void test_send_and_receive_round_trip_over_loopback(void) {
    altcross_socket_t *receiver = altcross_socket_open_udp(0);
    altcross_socket_t *sender = altcross_socket_open_udp(0);
    ASSERT_TRUE(receiver != NULL);
    ASSERT_TRUE(sender != NULL);

    int receiver_port = altcross_socket_local_port(receiver);

    const uint8_t message[] = {1, 2, 3, 4, 5};
    int sent = altcross_socket_send_to(sender, "127.0.0.1", receiver_port,
                                        message, sizeof(message));
    ASSERT_EQ((int)sizeof(message), sent);

    uint8_t buf[64];
    int received = altcross_socket_receive(receiver, buf, sizeof(buf), 2000);

    ASSERT_EQ((int)sizeof(message), received);
    ASSERT_EQ(0, memcmp(buf, message, sizeof(message)));

    altcross_socket_close(sender);
    altcross_socket_close(receiver);
}

static void test_receive_times_out_when_nothing_arrives(void) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    uint8_t buf[64];

    int result = altcross_socket_receive(sock, buf, sizeof(buf), 100);

    ASSERT_EQ(ALTCROSS_SOCKET_TIMEOUT, result);
    altcross_socket_close(sock);
}

static void test_receive_from_reports_the_sender_address(void) {
    altcross_socket_t *receiver = altcross_socket_open_udp(0);
    altcross_socket_t *sender = altcross_socket_open_udp(0);
    int receiver_port = altcross_socket_local_port(receiver);
    int sender_port = altcross_socket_local_port(sender);

    const uint8_t message[] = {9, 9};
    altcross_socket_send_to(sender, "127.0.0.1", receiver_port, message,
                             sizeof(message));

    uint8_t buf[64];
    char from_host[64];
    int from_port = -1;
    int received = altcross_socket_receive_from(
        receiver, buf, sizeof(buf), 2000, from_host, sizeof(from_host),
        &from_port);

    ASSERT_EQ((int)sizeof(message), received);
    ASSERT_EQ(0, strcmp("127.0.0.1", from_host));
    ASSERT_EQ(sender_port, from_port);

    altcross_socket_close(sender);
    altcross_socket_close(receiver);
}

static void test_enable_broadcast_succeeds(void) {
    altcross_socket_t *sock = altcross_socket_open_udp(0);
    ASSERT_EQ(0, altcross_socket_enable_broadcast(sock));
    altcross_socket_close(sock);
}

static void test_list_broadcast_addresses_finds_at_least_one_real_interface(
    void) {
    char addrs[ALTCROSS_MAX_NETWORK_INTERFACES][ALTCROSS_BROADCAST_ADDRESS_SIZE];
    int count = altcross_net_list_broadcast_addresses(
        (char *)addrs, ALTCROSS_BROADCAST_ADDRESS_SIZE,
        ALTCROSS_MAX_NETWORK_INTERFACES);

    /* toda máquina de dev tem pelo menos 1 interface ativa não-loopback
     * (Wi-Fi/Ethernet); não travamos num IP específico pra não depender de
     * qual máquina está rodando o teste. */
    ASSERT_TRUE(count >= 1);

    int shown = count < ALTCROSS_MAX_NETWORK_INTERFACES
                    ? count
                    : ALTCROSS_MAX_NETWORK_INTERFACES;
    for (int i = 0; i < shown; i++) {
        /* cada entrada deve parecer um IPv4 em texto (3 pontos) */
        int dots = 0;
        for (const char *p = addrs[i]; *p; p++) {
            if (*p == '.') {
                dots++;
            }
        }
        ASSERT_EQ(3, dots);
    }
}

void run_net_socket_tests(void) {
    RUN_TEST(test_open_assigns_a_local_port_when_zero_is_requested);
    RUN_TEST(test_send_and_receive_round_trip_over_loopback);
    RUN_TEST(test_receive_times_out_when_nothing_arrives);
    RUN_TEST(test_receive_from_reports_the_sender_address);
    RUN_TEST(test_enable_broadcast_succeeds);
    RUN_TEST(test_list_broadcast_addresses_finds_at_least_one_real_interface);
}
