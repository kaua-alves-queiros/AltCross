#include <stdio.h>
#include <string.h>

#include "altcross/pairing_protocol.h"
#include "test_framework.h"

#if defined(_WIN32)
#include <windows.h>
#define TEST_SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define TEST_SLEEP_MS(ms) usleep((unsigned int)(ms) * 1000)
#endif

/* Espera até 1s por um pedido pendente aparecer (a thread do respondedor
 * processa em background) antes de desistir. */
static int wait_for_incoming_request(altcross_pairing_incoming_request_t *out) {
    for (int i = 0; i < 20; i++) {
        if (altcross_pairing_poll_incoming_request(out)) {
            return 1;
        }
        TEST_SLEEP_MS(50);
    }
    return 0;
}

static void test_poll_returns_zero_when_nothing_pending(void) {
    altcross_pairing_incoming_request_t incoming;
    ASSERT_EQ(0, altcross_pairing_poll_incoming_request(&incoming));
}

static void test_pairing_handshake_happy_path(void) {
    const char *store_a = "altcross_test_pairing_proto_a.txt";
    const char *store_b = "altcross_test_pairing_proto_b.txt";
    remove(store_a);
    remove(store_b);

    ASSERT_EQ(0, altcross_pairing_start_responder("device-A", "PC A",
                                                    store_b));
    ASSERT_EQ(0, altcross_pairing_send_request("device-Z", "PC Z",
                                                 "127.0.0.1"));

    altcross_pairing_incoming_request_t incoming;
    ASSERT_TRUE(wait_for_incoming_request(&incoming));
    ASSERT_EQ(0, strcmp("device-Z", incoming.requester_device_id));
    ASSERT_EQ(0, strcmp("PC Z", incoming.requester_name));
    ASSERT_TRUE(incoming.code >= 100000 && incoming.code <= 999999);

    char accepted_device_id[ALTCROSS_DEVICE_ID_SIZE];
    char accepted_name[ALTCROSS_DEVICE_NAME_SIZE];
    int confirm_rc = altcross_pairing_confirm(
        "device-Z", "127.0.0.1", incoming.code, 2000, store_a,
        accepted_device_id, accepted_name);

    ASSERT_EQ(1, confirm_rc);
    ASSERT_EQ(0, strcmp("device-A", accepted_device_id));
    ASSERT_EQ(0, strcmp("PC A", accepted_name));

    altcross_pairing_stop_responder();

    altcross_pairing_store_t loaded_a;
    altcross_pairing_store_init(&loaded_a);
    ASSERT_EQ(0, altcross_pairing_store_load(&loaded_a, store_a));
    const altcross_trusted_device_t *a_sees_b =
        altcross_pairing_store_find(&loaded_a, "device-A");
    ASSERT_TRUE(a_sees_b != NULL);

    altcross_pairing_store_t loaded_b;
    altcross_pairing_store_init(&loaded_b);
    ASSERT_EQ(0, altcross_pairing_store_load(&loaded_b, store_b));
    const altcross_trusted_device_t *b_sees_a =
        altcross_pairing_store_find(&loaded_b, "device-Z");
    ASSERT_TRUE(b_sees_a != NULL);

    /* os 2 lados têm que ter salvo exatamente o mesmo segredo — é essa
     * credencial compartilhada que faz o pareamento valer, não o código de
     * 6 dígitos (que só prova presença física na hora). */
    ASSERT_EQ(0, strcmp(a_sees_b->secret, b_sees_a->secret));

    remove(store_a);
    remove(store_b);
}

static void test_pairing_wrong_code_is_rejected(void) {
    const char *store_b = "altcross_test_pairing_proto_b2.txt";
    remove(store_b);

    ASSERT_EQ(0, altcross_pairing_start_responder("device-A2", "PC A2",
                                                    store_b));
    ASSERT_EQ(0, altcross_pairing_send_request("device-Z2", "PC Z2",
                                                 "127.0.0.1"));

    altcross_pairing_incoming_request_t incoming;
    ASSERT_TRUE(wait_for_incoming_request(&incoming));

    int wrong_code = incoming.code == 100000 ? 999999 : 100000;
    int confirm_rc =
        altcross_pairing_confirm("device-Z2", "127.0.0.1", wrong_code, 2000,
                                  "altcross_test_pairing_proto_a2.txt", NULL,
                                  NULL);

    ASSERT_EQ(0, confirm_rc);

    altcross_pairing_stop_responder();
    remove(store_b);
    remove("altcross_test_pairing_proto_a2.txt");
}

static void test_confirm_without_pending_request_is_rejected(void) {
    const char *store_b = "altcross_test_pairing_proto_b3.txt";
    remove(store_b);

    ASSERT_EQ(0, altcross_pairing_start_responder("device-A3", "PC A3",
                                                    store_b));

    /* nunca mandou send_request — não deveria existir pedido pra confirmar */
    int confirm_rc =
        altcross_pairing_confirm("device-Z3", "127.0.0.1", 123456, 2000,
                                  "altcross_test_pairing_proto_a3.txt", NULL,
                                  NULL);

    ASSERT_EQ(0, confirm_rc);

    altcross_pairing_stop_responder();
    remove(store_b);
    remove("altcross_test_pairing_proto_a3.txt");
}

void run_pairing_protocol_tests(void) {
    RUN_TEST(test_poll_returns_zero_when_nothing_pending);
    RUN_TEST(test_pairing_handshake_happy_path);
    RUN_TEST(test_pairing_wrong_code_is_rejected);
    RUN_TEST(test_confirm_without_pending_request_is_rejected);
}
