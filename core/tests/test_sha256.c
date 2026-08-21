#include <stdio.h>
#include <string.h>

#include "altcross/sha256.h"
#include "test_framework.h"

static void to_hex(const uint8_t *bytes, size_t len, char *out) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out[i * 2] = digits[bytes[i] >> 4];
        out[i * 2 + 1] = digits[bytes[i] & 0x0F];
    }
    out[len * 2] = '\0';
}

static void assert_sha256_hex(const char *data, const char *expected_hex) {
    uint8_t digest[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_sha256((const uint8_t *)data, strlen(data), digest);
    char hex[ALTCROSS_SHA256_DIGEST_SIZE * 2 + 1];
    to_hex(digest, ALTCROSS_SHA256_DIGEST_SIZE, hex);
    ASSERT_EQ(0, strcmp(hex, expected_hex));
}

/* Vetores conferidos com hashlib.sha256 do Python nesta sessão — não
 * digitados de memória sem checagem (ver test_hmac_sha256.c pro mesmo
 * cuidado com HMAC). */
static void test_empty_string(void) {
    assert_sha256_hex(
        "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

static void test_abc(void) {
    assert_sha256_hex(
        "abc",
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

/* Vetor padrão de 2 blocos do NIST (56 bytes — força padding cruzando um
 * bloco inteiro extra), garante que o caminho de multi-bloco/wrap do
 * padding está certo, não só o caso de 1 bloco. */
static void test_two_block_message(void) {
    assert_sha256_hex(
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

static void test_incremental_update_matches_one_shot(void) {
    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    uint8_t one_shot[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_sha256((const uint8_t *)msg, strlen(msg), one_shot);

    altcross_sha256_ctx_t ctx;
    altcross_sha256_init(&ctx);
    altcross_sha256_update(&ctx, (const uint8_t *)msg, 10);
    altcross_sha256_update(&ctx, (const uint8_t *)msg + 10, strlen(msg) - 10);
    uint8_t incremental[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_sha256_final(&ctx, incremental);

    ASSERT_EQ(0, memcmp(one_shot, incremental, ALTCROSS_SHA256_DIGEST_SIZE));
}

void run_sha256_tests(void) {
    RUN_TEST(test_empty_string);
    RUN_TEST(test_abc);
    RUN_TEST(test_two_block_message);
    RUN_TEST(test_incremental_update_matches_one_shot);
}
