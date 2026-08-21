#ifndef ALTCROSS_SHA256_H
#define ALTCROSS_SHA256_H

#include <stddef.h>
#include <stdint.h>

#include "altcross/export.h"

#define ALTCROSS_SHA256_DIGEST_SIZE 32
#define ALTCROSS_SHA256_BLOCK_SIZE 64

/* Implementação própria, em C puro, do algoritmo padrão SHA-256 (FIPS
 * 180-4) — sem depender de biblioteca de criptografia da plataforma
 * (CommonCrypto no macOS, BCrypt no Windows), pra não precisar linkar
 * framework nenhum extra e continuar portável (ver escopo Linux futuro no
 * AGENTS.md). É o algoritmo padrão, não um esquema inventado — testado
 * contra os vetores de teste oficiais do NIST (ver test_sha256.c). Usado
 * como base de HMAC-SHA256 (ver hmac_sha256.h) pra autenticar o tráfego de
 * mouse/teclado entre 2 máquinas. */
typedef struct {
    uint32_t state[8];
    uint64_t bit_count;
    uint8_t buffer[ALTCROSS_SHA256_BLOCK_SIZE];
    size_t buffer_len;
} altcross_sha256_ctx_t;

ALTCROSS_API void altcross_sha256_init(altcross_sha256_ctx_t *ctx);
ALTCROSS_API void altcross_sha256_update(altcross_sha256_ctx_t *ctx,
                                          const uint8_t *data, size_t len);
ALTCROSS_API void altcross_sha256_final(
    altcross_sha256_ctx_t *ctx, uint8_t out[ALTCROSS_SHA256_DIGEST_SIZE]);

/* Atalho pra digest de uma vez só (init+update+final). */
ALTCROSS_API void altcross_sha256(const uint8_t *data, size_t len,
                                   uint8_t out[ALTCROSS_SHA256_DIGEST_SIZE]);

#endif
