#include "altcross/platform_input.h"
#include "test_framework.h"

/* altcross_platform_get_cursor_position é só leitura (não instala hook
 * global nenhum, não intercepta nada) — seguro de chamar automaticamente.
 * altcross_platform_warp_cursor de propósito NÃO tem teste automatizado
 * aqui: ele move o cursor de verdade de quem estiver rodando o teste, o
 * que seria intrusivo/inesperado rodando em CI ou na máquina de um
 * desenvolvedor sem aviso — mesma cautela de altcross_platform_input_start
 * (ver aviso no topo de platform_input.c). */
static void test_get_cursor_position_succeeds(void) {
    int x = 0;
    int y = 0;
    int rc = altcross_platform_get_cursor_position(&x, &y);
    ASSERT_EQ(0, rc);
}

void run_platform_input_tests(void) {
    RUN_TEST(test_get_cursor_position_succeeds);
}
