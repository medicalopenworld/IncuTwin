/* Ejecuta todos los TEST_CASE registrados sin menu interactivo y termina con un
 * resumen que lee tools/serial_capture.py. */
#include <stdio.h>

#include "unity.h"
#include "unity_test_runner.h"

void app_main(void)
{
    printf("\n===== IncuTwin test_apps =====\n");
    UNITY_BEGIN();
    unity_run_all_tests();
    UNITY_END();
    printf("===== fin de los tests =====\n");
}
