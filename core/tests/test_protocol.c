#include <string.h>

#include "altcross/protocol.h"
#include "test_framework.h"

static void test_encode_decode_round_trips_enter(void) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_ENTER;
    pkt.data.enter.x = 42;
    pkt.data.enter.y = -7;

    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_protocol_encode(&pkt, buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    altcross_packet_t decoded;
    memset(&decoded, 0, sizeof(decoded));
    int ok = altcross_protocol_decode(buf, (size_t)n, &decoded);

    ASSERT_EQ(1, ok);
    ASSERT_EQ(ALTCROSS_PKT_ENTER, decoded.type);
    ASSERT_EQ(42, decoded.data.enter.x);
    ASSERT_EQ(-7, decoded.data.enter.y);
}

static void test_encode_decode_round_trips_mouse_delta(void) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_MOUSE_DELTA;
    pkt.data.mouse_delta.dx = -100;
    pkt.data.mouse_delta.dy = 250;

    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_protocol_encode(&pkt, buf, sizeof(buf));

    altcross_packet_t decoded;
    int ok = altcross_protocol_decode(buf, (size_t)n, &decoded);

    ASSERT_EQ(1, ok);
    ASSERT_EQ(ALTCROSS_PKT_MOUSE_DELTA, decoded.type);
    ASSERT_EQ(-100, decoded.data.mouse_delta.dx);
    ASSERT_EQ(250, decoded.data.mouse_delta.dy);
}

static void test_encode_decode_round_trips_mouse_button(void) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_MOUSE_BUTTON;
    pkt.data.mouse_button.button = 1;
    pkt.data.mouse_button.down = 1;

    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_protocol_encode(&pkt, buf, sizeof(buf));

    altcross_packet_t decoded;
    int ok = altcross_protocol_decode(buf, (size_t)n, &decoded);

    ASSERT_EQ(1, ok);
    ASSERT_EQ(ALTCROSS_PKT_MOUSE_BUTTON, decoded.type);
    ASSERT_EQ(1, decoded.data.mouse_button.button);
    ASSERT_EQ(1, decoded.data.mouse_button.down);
}

static void test_encode_decode_round_trips_key(void) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_KEY;
    pkt.data.key.keycode = 65;
    pkt.data.key.down = 0;

    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_protocol_encode(&pkt, buf, sizeof(buf));

    altcross_packet_t decoded;
    int ok = altcross_protocol_decode(buf, (size_t)n, &decoded);

    ASSERT_EQ(1, ok);
    ASSERT_EQ(ALTCROSS_PKT_KEY, decoded.type);
    ASSERT_EQ(65, decoded.data.key.keycode);
    ASSERT_EQ(0, decoded.data.key.down);
}

static void test_encode_decode_round_trips_leave_with_no_payload(void) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_LEAVE;

    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_protocol_encode(&pkt, buf, sizeof(buf));
    ASSERT_EQ(2, n); /* versão + tipo, sem payload */

    altcross_packet_t decoded;
    int ok = altcross_protocol_decode(buf, (size_t)n, &decoded);

    ASSERT_EQ(1, ok);
    ASSERT_EQ(ALTCROSS_PKT_LEAVE, decoded.type);
}

static void test_encode_rejects_buffer_too_small(void) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_ENTER;
    pkt.data.enter.x = 1;
    pkt.data.enter.y = 1;

    uint8_t buf[1];
    int n = altcross_protocol_encode(&pkt, buf, sizeof(buf));
    ASSERT_EQ(-1, n);
}

static void test_decode_rejects_truncated_buffer(void) {
    altcross_packet_t pkt;
    pkt.type = ALTCROSS_PKT_ENTER;
    pkt.data.enter.x = 1;
    pkt.data.enter.y = 1;

    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE];
    int n = altcross_protocol_encode(&pkt, buf, sizeof(buf));

    altcross_packet_t decoded;
    int ok = altcross_protocol_decode(buf, (size_t)(n - 1), &decoded);
    ASSERT_EQ(0, ok);
}

static void test_decode_rejects_unknown_version(void) {
    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE] = {0};
    buf[0] = 99; /* versão inexistente */
    buf[1] = (uint8_t)ALTCROSS_PKT_LEAVE;

    altcross_packet_t decoded;
    int ok = altcross_protocol_decode(buf, 2, &decoded);
    ASSERT_EQ(0, ok);
}

static void test_decode_rejects_unknown_type(void) {
    uint8_t buf[ALTCROSS_PACKET_MAX_SIZE] = {0};
    buf[0] = ALTCROSS_PROTOCOL_VERSION;
    buf[1] = 99; /* tipo inexistente */

    altcross_packet_t decoded;
    int ok = altcross_protocol_decode(buf, 2, &decoded);
    ASSERT_EQ(0, ok);
}

void run_protocol_tests(void) {
    RUN_TEST(test_encode_decode_round_trips_enter);
    RUN_TEST(test_encode_decode_round_trips_mouse_delta);
    RUN_TEST(test_encode_decode_round_trips_mouse_button);
    RUN_TEST(test_encode_decode_round_trips_key);
    RUN_TEST(test_encode_decode_round_trips_leave_with_no_payload);
    RUN_TEST(test_encode_rejects_buffer_too_small);
    RUN_TEST(test_decode_rejects_truncated_buffer);
    RUN_TEST(test_decode_rejects_unknown_version);
    RUN_TEST(test_decode_rejects_unknown_type);
}
