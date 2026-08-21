/* Ferramenta de verificação manual da descoberta de rede — sem captura de
 * mouse/teclado nem hook global, seguro de rodar automaticamente.
 *
 * Uso:
 *   ./discovery_demo responder <device_id> <name> <port>
 *   ./discovery_demo query <device_id> [timeout_ms]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#define SLEEP_MS(ms) Sleep(ms)
#else
#include <unistd.h>
#define SLEEP_MS(ms) usleep((unsigned int)(ms) * 1000)
#endif

#include "altcross/discovery.h"

#define MAX_RESULTS 16

static int run_responder(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "uso: responder <device_id> <name> <port>\n");
        return 1;
    }
    const char *device_id = argv[2];
    const char *name = argv[3];
    int port = atoi(argv[4]);

    if (altcross_discovery_start_responder(device_id, name, port) != 0) {
        fprintf(stderr, "falha ao iniciar o respondedor\n");
        return 1;
    }

    printf("respondedor rodando como '%s' (%s), porta %d. Ctrl+C pra sair.\n",
           device_id, name, port);
    for (;;) {
        SLEEP_MS(1000);
    }
}

static int run_query(int argc, char **argv) {
    const char *device_id = argv[2];
    int timeout_ms = argc > 3 ? atoi(argv[3]) : 1500;

    char device_ids[MAX_RESULTS][ALTCROSS_DEVICE_ID_SIZE];
    char names[MAX_RESULTS][ALTCROSS_DEVICE_NAME_SIZE];
    char hosts[MAX_RESULTS][ALTCROSS_DISCOVERY_HOST_SIZE];
    int ports[MAX_RESULTS];

    printf("buscando por até %d ms...\n", timeout_ms);
    int found = altcross_discovery_run_query(
        device_id, timeout_ms, (char *)device_ids, (char *)names, ports,
        (char *)hosts, MAX_RESULTS);

    if (found < 0) {
        fprintf(stderr, "erro ao iniciar a busca\n");
        return 1;
    }

    printf("%d dispositivo(s) encontrado(s):\n", found);
    int shown = found < MAX_RESULTS ? found : MAX_RESULTS;
    for (int i = 0; i < shown; i++) {
        printf("  - %s (%s) em %s:%d\n", names[i], device_ids[i], hosts[i],
               ports[i]);
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
                "uso: %s responder <device_id> <name> <port> | query "
                "<device_id> [timeout_ms]\n",
                argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "responder") == 0) {
        return run_responder(argc, argv);
    }
    if (strcmp(argv[1], "query") == 0) {
        return run_query(argc, argv);
    }

    fprintf(stderr, "comando desconhecido: %s\n", argv[1]);
    return 1;
}
