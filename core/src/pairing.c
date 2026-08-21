#if defined(_WIN32)
/* Precisa vir antes da primeira inclusão de <stdlib.h> (direta ou via
 * header próprio), senão rand_s não é declarado (C4013). */
#define _CRT_RAND_S
#endif
#include "altcross/pairing.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
/* arc4random_buf já vem de <stdlib.h> no macOS. */
static void random_bytes(uint8_t *buf, size_t len) { arc4random_buf(buf, len); }
#elif defined(_WIN32)
static void random_bytes(uint8_t *buf, size_t len) {
    size_t i = 0;
    while (i < len) {
        unsigned int r;
        rand_s(&r);
        size_t chunk = (len - i < sizeof(r)) ? (len - i) : sizeof(r);
        memcpy(buf + i, &r, chunk);
        i += chunk;
    }
}
#else
static void random_bytes(uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = (uint8_t)(rand() & 0xFF);
    }
}
#endif

static void bytes_to_hex(const uint8_t *bytes, size_t len, char *out_hex) {
    static const char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < len; i++) {
        out_hex[i * 2] = digits[(bytes[i] >> 4) & 0xF];
        out_hex[i * 2 + 1] = digits[bytes[i] & 0xF];
    }
    out_hex[len * 2] = '\0';
}

void altcross_pairing_store_init(altcross_pairing_store_t *store) {
    store->count = 0;
}

static int find_index(const altcross_pairing_store_t *store,
                       const char *device_id) {
    for (int i = 0; i < store->count; i++) {
        if (strcmp(store->devices[i].device_id, device_id) == 0) {
            return i;
        }
    }
    return -1;
}

int altcross_pairing_store_upsert(altcross_pairing_store_t *store,
                                   const char *device_id, const char *name,
                                   const char *secret, const char *host,
                                   int port) {
    int idx = find_index(store, device_id);
    if (idx < 0) {
        if (store->count >= ALTCROSS_MAX_TRUSTED_DEVICES) {
            return 1;
        }
        idx = store->count++;
    }

    altcross_trusted_device_t *dev = &store->devices[idx];
    snprintf(dev->device_id, ALTCROSS_DEVICE_ID_SIZE, "%s", device_id);
    snprintf(dev->name, ALTCROSS_DEVICE_NAME_SIZE, "%s", name);
    snprintf(dev->secret, ALTCROSS_SECRET_SIZE, "%s", secret);
    snprintf(dev->last_host, ALTCROSS_HOST_SIZE, "%s", host);
    dev->last_port = port;
    return 0;
}

const altcross_trusted_device_t *
altcross_pairing_store_find(const altcross_pairing_store_t *store,
                            const char *device_id) {
    int idx = find_index(store, device_id);
    return idx < 0 ? NULL : &store->devices[idx];
}

int altcross_pairing_store_update_address(altcross_pairing_store_t *store,
                                           const char *device_id,
                                           const char *host, int port) {
    int idx = find_index(store, device_id);
    if (idx < 0) {
        return 1;
    }
    snprintf(store->devices[idx].last_host, ALTCROSS_HOST_SIZE, "%s", host);
    store->devices[idx].last_port = port;
    return 0;
}

int altcross_pairing_store_remove(altcross_pairing_store_t *store,
                                   const char *device_id) {
    int idx = find_index(store, device_id);
    if (idx < 0) {
        return 1;
    }
    for (int i = idx; i < store->count - 1; i++) {
        store->devices[i] = store->devices[i + 1];
    }
    store->count--;
    return 0;
}

int altcross_pairing_store_save(const altcross_pairing_store_t *store,
                                 const char *path) {
    FILE *f = fopen(path, "w");
    if (!f) {
        return 1;
    }
    for (int i = 0; i < store->count; i++) {
        const altcross_trusted_device_t *dev = &store->devices[i];
        fprintf(f, "%s|%s|%s|%d|%s\n", dev->device_id, dev->secret,
                dev->last_host, dev->last_port, dev->name);
    }
    fclose(f);
    return 0;
}

int altcross_pairing_store_load(altcross_pairing_store_t *store,
                                 const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        return 1;
    }

    altcross_pairing_store_init(store);

    char line[512];
    while (fgets(line, sizeof(line), f) &&
           store->count < ALTCROSS_MAX_TRUSTED_DEVICES) {
        char *device_id = strtok(line, "|");
        char *secret = strtok(NULL, "|");
        char *host = strtok(NULL, "|");
        char *port_str = strtok(NULL, "|");
        char *name = strtok(NULL, "\n");
        if (!device_id || !secret || !host || !port_str || !name) {
            continue;
        }
        altcross_pairing_store_upsert(store, device_id, name, secret, host,
                                       atoi(port_str));
    }

    fclose(f);
    return 0;
}

int altcross_pairing_generate_code(void) {
    uint32_t r;
    random_bytes((uint8_t *)&r, sizeof(r));
    return 100000 + (int)(r % 900000);
}

void altcross_pairing_generate_secret(char out_secret[ALTCROSS_SECRET_SIZE]) {
    uint8_t bytes[32];
    random_bytes(bytes, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), out_secret);
}

int altcross_pairing_local_identity_load_or_create(
    const char *path, char out_device_id[ALTCROSS_DEVICE_ID_SIZE]) {
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(out_device_id, ALTCROSS_DEVICE_ID_SIZE, f)) {
            size_t len = strlen(out_device_id);
            if (len > 0 && out_device_id[len - 1] == '\n') {
                out_device_id[len - 1] = '\0';
            }
            fclose(f);
            return 0;
        }
        fclose(f);
    }

    uint8_t bytes[16];
    random_bytes(bytes, sizeof(bytes));
    bytes_to_hex(bytes, sizeof(bytes), out_device_id);

    FILE *out = fopen(path, "w");
    if (!out) {
        return 1;
    }
    fprintf(out, "%s\n", out_device_id);
    fclose(out);
    return 0;
}
