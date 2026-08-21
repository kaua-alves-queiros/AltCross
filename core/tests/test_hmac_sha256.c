#include <stdio.h>
#include <string.h>

#include "altcross/hmac_sha256.h"
#include "test_framework.h"

static void to_hex(const uint8_t *bytes, size_t len, char *out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = digits[bytes[i] >> 4];
        out[i * 2 + 1] = digits[bytes[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static uint8_t hex_nibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    return (uint8_t)(c - 'a' + 10);
}

static size_t from_hex(const char *hex, uint8_t *out) {
    size_t len = strlen(hex) / 2;
    for (size_t i = 0; i < len; i++) {
        out[i] = (uint8_t)((hex_nibble(hex[i * 2]) << 4) | hex_nibble(hex[i * 2 + 1]));
    }
    return len;
}

/* Vetores conferidos com hmac.new(..., hashlib.sha256) do Python nesta
 * sessão (RFC 4231 casos 1, 2 e 6, mais 1 caso com chave de 32 bytes no
 * formato que altcross_pairing_generate_secret produz de verdade). */
static void test_rfc4231_case1(void) {
    uint8_t key[64];
    size_t key_len = from_hex(
        "0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", key);
    const char *data = "Hi There";

    uint8_t tag[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_hmac_sha256(key, key_len, (const uint8_t *)data, strlen(data), tag);

    char hex[ALTCROSS_SHA256_DIGEST_SIZE * 2 + 1];
    to_hex(tag, ALTCROSS_SHA256_DIGEST_SIZE, hex);
    ASSERT_EQ(0, strcmp(hex,
        "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"));
}

static void test_rfc4231_case2_short_key(void) {
    const char *key = "Jefe";
    const char *data = "what do ya want for nothing?";

    uint8_t tag[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_hmac_sha256((const uint8_t *)key, strlen(key),
                          (const uint8_t *)data, strlen(data), tag);

    char hex[ALTCROSS_SHA256_DIGEST_SIZE * 2 + 1];
    to_hex(tag, ALTCROSS_SHA256_DIGEST_SIZE, hex);
    ASSERT_EQ(0, strcmp(hex,
        "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"));
}

/* Chave maior que o bloco (64 bytes) — exercita o caminho "hash a chave
 * primeiro" do HMAC (RFC 2104), não só o caso comum de chave curta. */
static void test_rfc4231_case6_key_longer_than_block(void) {
    uint8_t key[131];
    for (int i = 0; i < 131; i++) {
        key[i] = 0xaa;
    }
    const char *data = "Test Using Larger Than Block-Size Key - Hash Key First";

    uint8_t tag[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_hmac_sha256(key, sizeof(key), (const uint8_t *)data, strlen(data),
                          tag);

    char hex[ALTCROSS_SHA256_DIGEST_SIZE * 2 + 1];
    to_hex(tag, ALTCROSS_SHA256_DIGEST_SIZE, hex);
    ASSERT_EQ(0, strcmp(hex,
        "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54"));
}

/* Chave de 32 bytes (formato real de altcross_pairing_generate_secret, ver
 * pairing.h) — o caso que a autenticação de handoff usa de verdade. */
static void test_32_byte_pairing_style_key(void) {
    uint8_t key[32];
    from_hex(
        "00112233445566778899aabbccddeeff00112233445566778899aabbccddee", key);
    const char *data = "altcross-test-payload";

    uint8_t tag[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_hmac_sha256(key, sizeof(key), (const uint8_t *)data, strlen(data),
                          tag);

    char hex[ALTCROSS_SHA256_DIGEST_SIZE * 2 + 1];
    to_hex(tag, ALTCROSS_SHA256_DIGEST_SIZE, hex);
    printf("DEBUG test_32_byte_pairing_style_key computed=%s\n", hex);
    printf("DEBUG sizeof(key)=%zu key_len_passed=%zu\n", sizeof(key),
           (size_t)32);
    ASSERT_EQ(0, strcmp(hex,
        "f0087146b96a57de8f32b5593ebd440c9486eb35614e43310cafc6384702bc3a"));
}

static void test_constant_time_equal(void) {
    uint8_t a[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t b[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t c[8] = {1, 2, 3, 4, 5, 6, 7, 9};

    ASSERT_TRUE(altcross_hmac_constant_time_equal(a, b, 8));
    ASSERT_TRUE(!altcross_hmac_constant_time_equal(a, c, 8));
}

void run_hmac_sha256_tests(void) {
    RUN_TEST(test_rfc4231_case1);
    RUN_TEST(test_rfc4231_case2_short_key);
    RUN_TEST(test_rfc4231_case6_key_longer_than_block);
    RUN_TEST(test_32_byte_pairing_style_key);
    RUN_TEST(test_constant_time_equal);
}
