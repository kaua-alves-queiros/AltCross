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

void run_net_socket_tests(void) {
    RUN_TEST(test_open_assigns_a_local_port_when_zero_is_requested);
    RUN_TEST(test_send_and_receive_round_trip_over_loopback);
    RUN_TEST(test_receive_times_out_when_nothing_arrives);
}
