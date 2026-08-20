#include <stdio.h>
#include <string.h>

#include "altcross/pairing.h"
#include "test_framework.h"

static void test_store_starts_empty(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    ASSERT_EQ(0, store.count);
}

static void test_upsert_adds_new_device(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);

    int rc = altcross_pairing_store_upsert(&store, "device-1", "PC do Kauã",
                                            "secret-abc", "192.168.0.10",
                                            45100);

    ASSERT_EQ(0, rc);
    ASSERT_EQ(1, store.count);
    const altcross_trusted_device_t *found =
        altcross_pairing_store_find(&store, "device-1");
    ASSERT_TRUE(found != NULL);
    ASSERT_EQ(0, strcmp(found->name, "PC do Kauã"));
    ASSERT_EQ(0, strcmp(found->secret, "secret-abc"));
    ASSERT_EQ(0, strcmp(found->last_host, "192.168.0.10"));
    ASSERT_EQ(45100, found->last_port);
}

static void test_upsert_same_device_id_updates_in_place(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    altcross_pairing_store_upsert(&store, "device-1", "Nome antigo", "s1",
                                   "10.0.0.1", 1000);

    altcross_pairing_store_upsert(&store, "device-1", "Nome novo", "s2",
                                   "10.0.0.2", 2000);

    ASSERT_EQ(1, store.count);
    const altcross_trusted_device_t *found =
        altcross_pairing_store_find(&store, "device-1");
    ASSERT_EQ(0, strcmp(found->name, "Nome novo"));
    ASSERT_EQ(2000, found->last_port);
}

static void test_find_returns_null_when_not_present(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    ASSERT_TRUE(altcross_pairing_store_find(&store, "nope") == NULL);
}

static void test_update_address_changes_host_and_port(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    altcross_pairing_store_upsert(&store, "device-1", "PC", "secret",
                                   "192.168.0.10", 45100);

    int rc = altcross_pairing_store_update_address(&store, "device-1",
                                                     "192.168.0.55", 45100);

    ASSERT_EQ(0, rc);
    const altcross_trusted_device_t *found =
        altcross_pairing_store_find(&store, "device-1");
    ASSERT_EQ(0, strcmp(found->last_host, "192.168.0.55"));
    /* segredo e nome não mudam quando só o endereço é atualizado */
    ASSERT_EQ(0, strcmp(found->secret, "secret"));
}

static void test_update_address_fails_for_unknown_device(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    int rc = altcross_pairing_store_update_address(&store, "unknown",
                                                     "192.168.0.55", 1);
    ASSERT_TRUE(rc != 0);
}

static void test_remove_deletes_device(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    altcross_pairing_store_upsert(&store, "device-1", "PC", "secret",
                                   "10.0.0.1", 1);

    int rc = altcross_pairing_store_remove(&store, "device-1");

    ASSERT_EQ(0, rc);
    ASSERT_EQ(0, store.count);
    ASSERT_TRUE(altcross_pairing_store_find(&store, "device-1") == NULL);
}

static void test_save_and_load_round_trip(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    altcross_pairing_store_upsert(&store, "device-1", "PC do Kauã",
                                   "abc123secret", "192.168.0.10", 45100);
    altcross_pairing_store_upsert(&store, "device-2", "Windows Gamer",
                                   "def456secret", "192.168.0.20", 45200);

    const char *path = "/tmp/altcross_test_pairing_store.txt";
    int save_rc = altcross_pairing_store_save(&store, path);
    ASSERT_EQ(0, save_rc);

    altcross_pairing_store_t loaded;
    altcross_pairing_store_init(&loaded);
    int load_rc = altcross_pairing_store_load(&loaded, path);

    ASSERT_EQ(0, load_rc);
    ASSERT_EQ(2, loaded.count);
    const altcross_trusted_device_t *d1 =
        altcross_pairing_store_find(&loaded, "device-1");
    ASSERT_TRUE(d1 != NULL);
    ASSERT_EQ(0, strcmp(d1->name, "PC do Kauã"));
    ASSERT_EQ(0, strcmp(d1->secret, "abc123secret"));
    ASSERT_EQ(45100, d1->last_port);
    const altcross_trusted_device_t *d2 =
        altcross_pairing_store_find(&loaded, "device-2");
    ASSERT_TRUE(d2 != NULL);
    ASSERT_EQ(45200, d2->last_port);

    remove(path);
}

static void test_load_missing_file_returns_error_without_crashing(void) {
    altcross_pairing_store_t store;
    altcross_pairing_store_init(&store);
    int rc =
        altcross_pairing_store_load(&store, "/tmp/altcross_never_exists.txt");
    ASSERT_TRUE(rc != 0);
    ASSERT_EQ(0, store.count);
}

static void test_generate_code_is_always_six_digits(void) {
    for (int i = 0; i < 200; i++) {
        int code = altcross_pairing_generate_code();
        ASSERT_TRUE(code >= 100000 && code <= 999999);
    }
}

static void test_generate_secret_is_64_hex_chars(void) {
    char secret[ALTCROSS_SECRET_SIZE];
    altcross_pairing_generate_secret(secret);

    ASSERT_EQ(64, (int)strlen(secret));
    for (int i = 0; i < 64; i++) {
        char c = secret[i];
        int is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        ASSERT_TRUE(is_hex);
    }
}

static void test_generate_secret_is_not_always_the_same(void) {
    char a[ALTCROSS_SECRET_SIZE];
    char b[ALTCROSS_SECRET_SIZE];
    altcross_pairing_generate_secret(a);
    altcross_pairing_generate_secret(b);
    ASSERT_TRUE(strcmp(a, b) != 0);
}

static void test_local_identity_creates_and_persists(void) {
    const char *path = "/tmp/altcross_test_identity.txt";
    remove(path);

    char id_first[ALTCROSS_DEVICE_ID_SIZE];
    int rc1 = altcross_pairing_local_identity_load_or_create(path, id_first);
    ASSERT_EQ(0, rc1);
    ASSERT_EQ(32, (int)strlen(id_first));

    char id_second[ALTCROSS_DEVICE_ID_SIZE];
    int rc2 = altcross_pairing_local_identity_load_or_create(path, id_second);
    ASSERT_EQ(0, rc2);

    ASSERT_EQ(0, strcmp(id_first, id_second));

    remove(path);
}

void run_pairing_tests(void) {
    RUN_TEST(test_store_starts_empty);
    RUN_TEST(test_upsert_adds_new_device);
    RUN_TEST(test_upsert_same_device_id_updates_in_place);
    RUN_TEST(test_find_returns_null_when_not_present);
    RUN_TEST(test_update_address_changes_host_and_port);
    RUN_TEST(test_update_address_fails_for_unknown_device);
    RUN_TEST(test_remove_deletes_device);
    RUN_TEST(test_save_and_load_round_trip);
    RUN_TEST(test_load_missing_file_returns_error_without_crashing);
    RUN_TEST(test_generate_code_is_always_six_digits);
    RUN_TEST(test_generate_secret_is_64_hex_chars);
    RUN_TEST(test_generate_secret_is_not_always_the_same);
    RUN_TEST(test_local_identity_creates_and_persists);
}
