#include <string.h>

#include "altcross/discovery.h"
#include "test_framework.h"

static void test_query_round_trips(void) {
    uint8_t buf[ALTCROSS_DISCOVERY_MAX_SIZE];
    int n = altcross_discovery_encode_query("device-abc", buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    altcross_discovery_msg_type_t type;
    char from_id[ALTCROSS_DEVICE_ID_SIZE];
    altcross_discovery_reply_t reply;
    int ok = altcross_discovery_decode(buf, (size_t)n, &type, from_id, &reply);

    ASSERT_EQ(1, ok);
    ASSERT_EQ(ALTCROSS_DISCOVERY_MSG_QUERY, type);
    ASSERT_EQ(0, strcmp("device-abc", from_id));
}

static void test_reply_round_trips(void) {
    uint8_t buf[ALTCROSS_DISCOVERY_MAX_SIZE];
    int n = altcross_discovery_encode_reply("device-xyz", "PC do Kauã", 45100,
                                             buf, sizeof(buf));
    ASSERT_TRUE(n > 0);

    altcross_discovery_msg_type_t type;
    char from_id[ALTCROSS_DEVICE_ID_SIZE];
    altcross_discovery_reply_t reply;
    int ok = altcross_discovery_decode(buf, (size_t)n, &type, from_id, &reply);

    ASSERT_EQ(1, ok);
    ASSERT_EQ(ALTCROSS_DISCOVERY_MSG_REPLY, type);
    ASSERT_EQ(0, strcmp("device-xyz", reply.device_id));
    ASSERT_EQ(0, strcmp("PC do Kauã", reply.name));
    ASSERT_EQ(45100, reply.port);
}

static void test_decode_rejects_truncated_buffer(void) {
    uint8_t buf[ALTCROSS_DISCOVERY_MAX_SIZE];
    int n = altcross_discovery_encode_query("device-abc", buf, sizeof(buf));

    altcross_discovery_msg_type_t type;
    char from_id[ALTCROSS_DEVICE_ID_SIZE];
    altcross_discovery_reply_t reply;
    int ok = altcross_discovery_decode(buf, (size_t)(n - 1), &type, from_id,
                                        &reply);
    ASSERT_EQ(0, ok);
}

static void test_decode_rejects_bad_magic(void) {
    uint8_t buf[ALTCROSS_DISCOVERY_MAX_SIZE];
    int n = altcross_discovery_encode_query("device-abc", buf, sizeof(buf));
    buf[0] = 'X'; /* corrompe o magic */

    altcross_discovery_msg_type_t type;
    char from_id[ALTCROSS_DEVICE_ID_SIZE];
    altcross_discovery_reply_t reply;
    int ok = altcross_discovery_decode(buf, (size_t)n, &type, from_id, &reply);
    ASSERT_EQ(0, ok);
}

static void test_encode_query_rejects_buffer_too_small(void) {
    uint8_t buf[4];
    int n = altcross_discovery_encode_query("device-abc", buf, sizeof(buf));
    ASSERT_EQ(-1, n);
}

void run_discovery_tests(void) {
    RUN_TEST(test_query_round_trips);
    RUN_TEST(test_reply_round_trips);
    RUN_TEST(test_decode_rejects_truncated_buffer);
    RUN_TEST(test_decode_rejects_bad_magic);
    RUN_TEST(test_encode_query_rejects_buffer_too_small);
}
