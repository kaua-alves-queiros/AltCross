#include "test_framework.h"
#include "altcross/connection_monitor.h"

static void test_heartbeat_encode_decode_ping(void) {
    uint8_t buf[ALTCROSS_HEARTBEAT_MAX_SIZE];
    int len = altcross_heartbeat_encode(ALTCROSS_HEARTBEAT_PING, "device-001",
                                        buf, sizeof(buf));
    ASSERT_GT(len, 0);

    altcross_heartbeat_type_t type;
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    ASSERT_EQ(1, altcross_heartbeat_decode(buf, (size_t)len, &type,
                                            device_id, sizeof(device_id)));
    ASSERT_EQ(ALTCROSS_HEARTBEAT_PING, type);
    ASSERT_STREQ("device-001", device_id);
}

static void test_heartbeat_encode_decode_pong(void) {
    uint8_t buf[ALTCROSS_HEARTBEAT_MAX_SIZE];
    int len = altcross_heartbeat_encode(ALTCROSS_HEARTBEAT_PONG, "device-002",
                                        buf, sizeof(buf));
    ASSERT_GT(len, 0);

    altcross_heartbeat_type_t type;
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    ASSERT_EQ(1, altcross_heartbeat_decode(buf, (size_t)len, &type,
                                            device_id, sizeof(device_id)));
    ASSERT_EQ(ALTCROSS_HEARTBEAT_PONG, type);
    ASSERT_STREQ("device-002", device_id);
}

static void test_heartbeat_decode_invalid_magic(void) {
    uint8_t buf[ALTCROSS_HEARTBEAT_MAX_SIZE] = {0};
    buf[0] = 'X';
    buf[1] = 'X';
    buf[2] = 'X';
    buf[3] = 'X';
    buf[4] = 1;
    buf[5] = 1;

    altcross_heartbeat_type_t type;
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    ASSERT_EQ(0, altcross_heartbeat_decode(buf, sizeof(buf), &type,
                                            device_id, sizeof(device_id)));
}

static void test_heartbeat_decode_wrong_version(void) {
    uint8_t buf[ALTCROSS_HEARTBEAT_MAX_SIZE] = {0};
    buf[0] = 'A';
    buf[1] = 'L';
    buf[2] = 'T';
    buf[3] = 'H';
    buf[4] = 99;
    buf[5] = 1;

    altcross_heartbeat_type_t type;
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    ASSERT_EQ(0, altcross_heartbeat_decode(buf, sizeof(buf), &type,
                                            device_id, sizeof(device_id)));
}

static void test_heartbeat_encode_buffer_too_small(void) {
    uint8_t buf[4];
    int len = altcross_heartbeat_encode(ALTCROSS_HEARTBEAT_PING, "device-001",
                                        buf, sizeof(buf));
    ASSERT_EQ(-1, len);
}

static void test_heartbeat_decode_unknown_type(void) {
    uint8_t buf[ALTCROSS_HEARTBEAT_MAX_SIZE] = {0};
    buf[0] = 'A';
    buf[1] = 'L';
    buf[2] = 'T';
    buf[3] = 'H';
    buf[4] = 1;
    buf[5] = 99;

    altcross_heartbeat_type_t type;
    char device_id[ALTCROSS_DEVICE_ID_SIZE];
    ASSERT_EQ(0, altcross_heartbeat_decode(buf, sizeof(buf), &type,
                                            device_id, sizeof(device_id)));
}

void run_connection_monitor_tests(void) {
    RUN_TEST(test_heartbeat_encode_decode_ping);
    RUN_TEST(test_heartbeat_encode_decode_pong);
    RUN_TEST(test_heartbeat_decode_invalid_magic);
    RUN_TEST(test_heartbeat_decode_wrong_version);
    RUN_TEST(test_heartbeat_encode_buffer_too_small);
    RUN_TEST(test_heartbeat_decode_unknown_type);
}
