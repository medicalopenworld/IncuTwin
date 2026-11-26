/* Bebes que han pasado por cada IncuNest (stay_log): abrir, cerrar, stay_id, desconexion, orden. */
#include <string.h>

#include "stay_log.h"
#include "unity.h"

static stay_obs_t O(const char *inc, bool present, bool known, uint32_t now)
{
    return (stay_obs_t){ .inc = inc, .country = "TG", .stay_id = "", .present = present, .known = known,
                         .sex = TWIN_SEX_UNKNOWN, .now = now };
}

TEST_CASE("stays: entra, sale y entra otro", "[stays]") {
    static stay_log_t l; memset(&l, 0, sizeof(l));
    stay_obs_t o = O("297", true, true, 100);
    TEST_ASSERT_TRUE(stay_log_observe(&l, &o));
    TEST_ASSERT_FALSE(stay_log_observe(&l, &o)); /* el mismo estado otra vez: nada */
    o = O("297", false, true, 200); TEST_ASSERT_TRUE(stay_log_observe(&l, &o));
    o = O("297", true, true, 300);  TEST_ASSERT_TRUE(stay_log_observe(&l, &o));
    const stay_t *b[4];
    TEST_ASSERT_EQUAL_UINT8(2, stay_log_babies(&l, "297", b, 4));
    TEST_ASSERT_EQUAL_UINT32(300, b[0]->arrived_at); TEST_ASSERT_EQUAL_UINT8(1, b[0]->open);
    TEST_ASSERT_EQUAL_UINT32(100, b[1]->arrived_at); TEST_ASSERT_EQUAL_UINT32(200, b[1]->left_at);
    uint8_t idx[STAY_INC_MAX];
    TEST_ASSERT_EQUAL_UINT8(1, stay_log_incubators(&l, idx));
    TEST_ASSERT_EQUAL_UINT16(2, l.inc[idx[0]].babies);
    TEST_ASSERT_EQUAL_STRING("TG", l.inc[idx[0]].country);
}

TEST_CASE("stays: desconectada no cierra la estancia", "[stays]") {
    static stay_log_t l; memset(&l, 0, sizeof(l));
    stay_obs_t o = O("297", true, true, 100); stay_log_observe(&l, &o);
    o = O("297", false, false, 200); TEST_ASSERT_FALSE(stay_log_observe(&l, &o));
    o = O("297", true, true, 300);   TEST_ASSERT_FALSE(stay_log_observe(&l, &o));
    const stay_t *b[2];
    TEST_ASSERT_EQUAL_UINT8(1, stay_log_babies(&l, "297", b, 2));
}

TEST_CASE("stays: un stay_id nuevo es otro bebe; el sexo llega despues", "[stays]") {
    static stay_log_t l; memset(&l, 0, sizeof(l));
    stay_obs_t o = O("297", true, true, 100); stay_log_observe(&l, &o);
    o.stay_id = "297:4"; TEST_ASSERT_TRUE(stay_log_observe(&l, &o)); /* hereda el stay_id */
    o.sex = TWIN_SEX_FEMALE; TEST_ASSERT_TRUE(stay_log_observe(&l, &o));
    o.stay_id = "297:5"; o.now = 500; o.sex = TWIN_SEX_MALE; TEST_ASSERT_TRUE(stay_log_observe(&l, &o));
    const stay_t *b[4];
    TEST_ASSERT_EQUAL_UINT8(2, stay_log_babies(&l, "297", b, 4));
    TEST_ASSERT_EQUAL_UINT8(TWIN_SEX_MALE, b[0]->sex);
    TEST_ASSERT_EQUAL_UINT8(TWIN_SEX_FEMALE, b[1]->sex); TEST_ASSERT_EQUAL_UINT8(0, b[1]->open);
    TEST_ASSERT_EQUAL_UINT32(500, b[1]->left_at);
}

TEST_CASE("stays: sin hora al entrar, la pone en cuanto la hay", "[stays]") {
    static stay_log_t l; memset(&l, 0, sizeof(l));
    stay_obs_t o = O("297", true, true, 0); stay_log_observe(&l, &o);
    o.now = 900; TEST_ASSERT_TRUE(stay_log_observe(&l, &o));
    const stay_t *b[1];
    stay_log_babies(&l, "297", b, 1);
    TEST_ASSERT_EQUAL_UINT32(900, b[0]->arrived_at);
}

TEST_CASE("stays: varias IncuNest, la ultima en cambiar primero", "[stays]") {
    static stay_log_t l; memset(&l, 0, sizeof(l));
    stay_obs_t o = O("297", true, true, 100); stay_log_observe(&l, &o);
    o = O("353", true, true, 200); stay_log_observe(&l, &o); /* acompañar: otra IncuNest */
    uint8_t idx[STAY_INC_MAX];
    TEST_ASSERT_EQUAL_UINT8(2, stay_log_incubators(&l, idx));
    TEST_ASSERT_EQUAL_STRING("353", l.inc[idx[0]].inc);
    TEST_ASSERT_EQUAL_STRING("297", l.inc[idx[1]].inc);
    /* la de 297 sigue abierta: no se sabe si el bebe sigue */
    const stay_t *b[1];
    stay_log_babies(&l, "297", b, 1);
    TEST_ASSERT_EQUAL_UINT8(1, b[0]->open);
}

TEST_CASE("stays: el contador no baja al pisar el anillo", "[stays]") {
    static stay_log_t l; memset(&l, 0, sizeof(l));
    for (uint32_t i = 0; i < STAY_LOG_MAX + 5; i++) {
        stay_obs_t o = O("297", true, true, 1000 + 2 * i); stay_log_observe(&l, &o);
        o = O("297", false, true, 1001 + 2 * i); stay_log_observe(&l, &o);
    }
    uint8_t idx[STAY_INC_MAX];
    stay_log_incubators(&l, idx);
    TEST_ASSERT_EQUAL_UINT16(STAY_LOG_MAX + 5, l.inc[idx[0]].babies);
    TEST_ASSERT_EQUAL_UINT8(STAY_LOG_MAX, l.count);
}
