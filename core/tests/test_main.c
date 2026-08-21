#include <stdio.h>

#include "test_framework.h"

void run_hotzone_tests(void);
void run_input_control_tests(void);
void run_protocol_tests(void);
void run_net_socket_tests(void);
void run_pairing_tests(void);
void run_pairing_protocol_tests(void);
void run_screen_sync_protocol_tests(void);
void run_discovery_tests(void);
void run_platform_input_tests(void);

int main(void) {
    run_hotzone_tests();
    run_input_control_tests();
    run_protocol_tests();
    run_net_socket_tests();
    run_pairing_tests();
    run_pairing_protocol_tests();
    run_screen_sync_protocol_tests();
    run_discovery_tests();
    run_platform_input_tests();

    printf("\n%d tests, %d failures\n", altcross_test_count,
           altcross_test_failures);
    return altcross_test_failures > 0 ? 1 : 0;
}
