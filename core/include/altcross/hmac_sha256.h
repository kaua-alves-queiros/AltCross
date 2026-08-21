#ifndef ALTCROSS_HMAC_SHA256_H
#define ALTCROSS_HMAC_SHA256_H

#include <stddef.h>
#include <stdint.h>

#include "altcross/export.h"
#include "altcross/sha256.h"

/* HMAC-SHA256 (RFC 2104 / FIPS 198-1), construído em cima de sha256.h —
 * usado pra autenticar cada pacote de mouse/teclado trocado entre 2
 * máquinas (ver handoff_protocol.h) com o segredo compartilhado que o
 * pareamento já gera (altcross_pairing_generate_secret). Testado contra os
 * vetores de teste oficiais do RFC 4231 (ver test_hmac_sha256.c). */
ALTCROSS_API void altcross_hmac_sha256(
    const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len,
    uint8_t out[ALTCROSS_SHA256_DIGEST_SIZE]);

/* Compara 2 tags de tamanho fixo em tempo constante (não vaza quanto do
 * prefixo bateu via timing) — sempre usar isso pra checar uma HMAC recebida
 * contra a calculada, nunca memcmp direto. */
ALTCROSS_API int altcross_hmac_constant_time_equal(const uint8_t *a,
                                                     const uint8_t *b,
                                                     size_t len);

#endif
