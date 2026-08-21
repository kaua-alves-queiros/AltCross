#include "altcross/hmac_sha256.h"

#include <string.h>

void altcross_hmac_sha256(const uint8_t *key, size_t key_len,
                           const uint8_t *data, size_t data_len,
                           uint8_t out[ALTCROSS_SHA256_DIGEST_SIZE]) {
    uint8_t key_block[ALTCROSS_SHA256_BLOCK_SIZE];
    memset(key_block, 0, sizeof(key_block));

    if (key_len > ALTCROSS_SHA256_BLOCK_SIZE) {
        /* Chave maior que 1 bloco: usa o hash dela em vez da chave crua
         * (parte padrão do RFC 2104). */
        altcross_sha256(key, key_len, key_block);
    } else {
        memcpy(key_block, key, key_len);
    }

    uint8_t ipad[ALTCROSS_SHA256_BLOCK_SIZE];
    uint8_t opad[ALTCROSS_SHA256_BLOCK_SIZE];
    for (size_t i = 0; i < ALTCROSS_SHA256_BLOCK_SIZE; i++) {
        ipad[i] = key_block[i] ^ 0x36;
        opad[i] = key_block[i] ^ 0x5c;
    }

    altcross_sha256_ctx_t ctx;
    uint8_t inner[ALTCROSS_SHA256_DIGEST_SIZE];
    altcross_sha256_init(&ctx);
    altcross_sha256_update(&ctx, ipad, sizeof(ipad));
    altcross_sha256_update(&ctx, data, data_len);
    altcross_sha256_final(&ctx, inner);

    altcross_sha256_init(&ctx);
    altcross_sha256_update(&ctx, opad, sizeof(opad));
    altcross_sha256_update(&ctx, inner, sizeof(inner));
    altcross_sha256_final(&ctx, out);
}

int altcross_hmac_constant_time_equal(const uint8_t *a, const uint8_t *b,
                                       size_t len) {
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= (uint8_t)(a[i] ^ b[i]);
    }
    return diff == 0;
}
