/* Spec accompany: elegir IncuNest, no abandonar, fin de estancia, tiempo, recordatorio, dias.
 * Relojes sinteticos: s monotonos, dias desde 1970 y hora local. */
#include <stdio.h>
#include <string.h>

#include "acc_logic.h"
#include "unity.h"

#define DAY 20000 /* un dia cualquiera */

static acc_t a;

/* panel nuevo, sin espera para elegir */
static void fresh(void)
{
    acc_init(&a, acc_hash("incutwin-TEST01"));
}

/* historial sembrado: n segundos acompañados a `id` (gesto hoy, bebe a la vista) */
static void seed_hist(const char *id, uint32_t secs)
{
    for (int i = 0; i < a.p.n_hist; i++) {
        if (!strcmp(a.p.hist[i].id, id)) { a.p.hist[i].secs = secs; return; }
    }
    strcpy(a.p.hist[a.p.n_hist].id, id);
    a.p.hist[a.p.n_hist].secs = secs;
    a.p.n_hist++;
}

static void baby(const char *id, uint32_t now) { acc_seen_update(&a, id, true, false, now); }
static void free_(const char *id, uint32_t now) { acc_seen_update(&a, id, false, false, now); }

/* ------------------------------------------------------------------ elegir */

TEST_CASE("acc: prefiere la IncuNest que mas ha acompañado", "[acc]")
{
    fresh();
    seed_hist("353", 100);
    seed_hist("297", 5000);
    acc_hold_pick(&a, 5); /* llegan los retenidos de golpe */
    baby("353", 1);
    baby("297", 1);
    TEST_ASSERT_EQUAL(TWIN_ACC_SEARCHING, acc_phase(&a)); /* aun no elige */
    uint32_t r = acc_tick(&a, 5, DAY, 10, false);
    TEST_ASSERT_TRUE(r & ACC_R_SWITCH);
    TEST_ASSERT_EQUAL_STRING("297", acc_current(&a));
    TEST_ASSERT_EQUAL(TWIN_ACC_WAITING, acc_phase(&a));
}

TEST_CASE("acc: sin conexion no elige (espera muy larga, con desbordamiento)", "[acc]")
{
    fresh();
    acc_hold_pick(&a, 0xFFFFFF00u + 0x70000000u); /* "nunca" calculado cerca del desbordamiento */
    acc_seen_update(&a, "353", true, false, 0xFFFFFF00u);
    acc_tick(&a, 0xFFFFFF10u, DAY, 10, false);
    acc_tick(&a, 0x00000100u, DAY, 10, false); /* el reloj de 32 bits da la vuelta */
    TEST_ASSERT_EQUAL(TWIN_ACC_SEARCHING, acc_phase(&a));
    acc_hold_pick(&a, 0x00000200u); /* conecta */
    acc_tick(&a, 0x00000200u, DAY, 10, false);
    TEST_ASSERT_EQUAL_STRING("353", acc_current(&a));
}

TEST_CASE("acc: sin bebes sigue buscando y elige al primero que llegue", "[acc]")
{
    fresh();
    free_("353", 0);
    acc_seen_update(&a, "297", false, false, 0); /* desconectada */
    acc_tick(&a, 1, DAY, 10, false);
    TEST_ASSERT_EQUAL(TWIN_ACC_SEARCHING, acc_phase(&a));
    TEST_ASSERT_EQUAL_STRING("", acc_current(&a));
    uint32_t r = acc_seen_update(&a, "297", true, false, 2);
    TEST_ASSERT_TRUE(r & ACC_R_SWITCH);
    TEST_ASSERT_TRUE(r & ACC_R_FORWARD);
    TEST_ASSERT_EQUAL_STRING("297", acc_current(&a));
}

TEST_CASE("acc: un bebe que se va a casa no se elige", "[acc]")
{
    fresh();
    acc_seen_update(&a, "353", false, true, 0);
    TEST_ASSERT_EQUAL(TWIN_ACC_SEARCHING, acc_phase(&a));
}

TEST_CASE("acc: los empates se reparten por panel y no dependen del orden", "[acc]")
{
    int pick_353 = 0;
    for (int p = 0; p < 8; p++) {
        char cid[24], first[TWIN_INCUBATOR_ID_LEN];
        snprintf(cid, sizeof(cid), "incutwin-P%d", p);
        acc_init(&a, acc_hash(cid));
        acc_hold_pick(&a, 1);
        baby("297", 0);
        baby("353", 0);
        acc_tick(&a, 1, DAY, 10, false);
        strcpy(first, acc_current(&a));
        acc_init(&a, acc_hash(cid));
        acc_hold_pick(&a, 1);
        baby("353", 0);
        baby("297", 0);
        acc_tick(&a, 1, DAY, 10, false);
        TEST_ASSERT_EQUAL_STRING(first, acc_current(&a));
        if (!strcmp(first, "353")) pick_353++;
    }
    TEST_ASSERT_TRUE(pick_353 > 0 && pick_353 < 8);
}

/* --------------------------------------------------------- no abandonar */

TEST_CASE("acc: la favorita recibe un bebe y se sigue con el actual", "[acc]")
{
    fresh();
    seed_hist("297", 5000);
    baby("353", 0);
    TEST_ASSERT_EQUAL_STRING("353", acc_current(&a));
    acc_gesture(&a, DAY);
    uint32_t r = acc_seen_update(&a, "297", true, false, 10);
    TEST_ASSERT_FALSE(r & ACC_R_SWITCH);
    TEST_ASSERT_FALSE(r & ACC_R_FORWARD);
    TEST_ASSERT_EQUAL_STRING("353", acc_current(&a));
    TEST_ASSERT_EQUAL(TWIN_ACC_WITH, acc_phase(&a));
}

TEST_CASE("acc: esperando tambien es pegajoso", "[acc]")
{
    fresh();
    seed_hist("297", 5000);
    baby("353", 0);
    baby("297", 1);
    TEST_ASSERT_EQUAL_STRING("353", acc_current(&a));
    TEST_ASSERT_EQUAL(TWIN_ACC_WAITING, acc_phase(&a));
}

TEST_CASE("acc: los estados de la actual se aplican al gemelo", "[acc]")
{
    fresh();
    baby("353", 0);
    TEST_ASSERT_TRUE(acc_seen_update(&a, "353", true, false, 5) & ACC_R_FORWARD);
    TEST_ASSERT_FALSE(acc_seen_update(&a, "297", true, false, 5) & ACC_R_FORWARD);
}

/* ------------------------------------------------------ fin de estancia */

TEST_CASE("acc: se va a casa, despedida congelada hasta tocar", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_gesture(&a, DAY);
    baby("297", 0);
    uint32_t r = acc_seen_update(&a, "353", false, true, 100);
    TEST_ASSERT_TRUE(r & ACC_R_FORWARD); /* el "home" si se aplica: vista de casa y fanfarria */
    TEST_ASSERT_EQUAL(TWIN_ACC_FAREWELL, acc_phase(&a));
    r = acc_seen_update(&a, "353", false, false, 200); /* luego queda libre */
    TEST_ASSERT_FALSE(r & ACC_R_FORWARD);             /* no: la despedida sigue en pantalla */
    acc_tick(&a, 300, DAY, 12, false);
    TEST_ASSERT_EQUAL(TWIN_ACC_FAREWELL, acc_phase(&a));
    r = acc_touch(&a, 400);
    TEST_ASSERT_TRUE(r & ACC_R_SWITCH);
    TEST_ASSERT_EQUAL_STRING("297", acc_current(&a));
    TEST_ASSERT_EQUAL(TWIN_ACC_WAITING, acc_phase(&a));
}

TEST_CASE("acc: la despedida termina al cambiar el dia", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_seen_update(&a, "353", false, true, 10);
    acc_tick(&a, 11, DAY, 20, false);
    TEST_ASSERT_EQUAL(TWIN_ACC_FAREWELL, acc_phase(&a));
    acc_tick(&a, 20000, DAY + 1, 8, false);
    TEST_ASSERT_EQUAL(TWIN_ACC_SEARCHING, acc_phase(&a));
    TEST_ASSERT_EQUAL_STRING("", acc_current(&a));
}

TEST_CASE("acc: corte corto, sigue con la misma", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_gesture(&a, DAY);
    baby("297", 0);
    uint32_t r = acc_seen_update(&a, "353", false, false, 1000); /* offline */
    TEST_ASSERT_TRUE(r & ACC_R_FORWARD);
    TEST_ASSERT_EQUAL(TWIN_ACC_ENDING, acc_phase(&a));
    acc_tick(&a, 1000 + 1800, DAY, 12, false);
    TEST_ASSERT_EQUAL(TWIN_ACC_ENDING, acc_phase(&a));
    baby("353", 1000 + 1800);
    TEST_ASSERT_EQUAL(TWIN_ACC_WITH, acc_phase(&a));
    TEST_ASSERT_EQUAL_STRING("353", acc_current(&a));
}

TEST_CASE("acc: estancia terminada sin alta, a las 2 h elige otra", "[acc]")
{
    fresh();
    baby("353", 0);
    baby("297", 0);
    free_("353", 1000);
    acc_tick(&a, 1000 + ACC_ENDING_S - 1, DAY, 12, false);
    TEST_ASSERT_EQUAL_STRING("353", acc_current(&a));
    uint32_t r = acc_tick(&a, 1000 + ACC_ENDING_S, DAY, 12, false);
    TEST_ASSERT_TRUE(r & ACC_R_SWITCH);
    TEST_ASSERT_EQUAL_STRING("297", acc_current(&a));
    TEST_ASSERT_EQUAL(TWIN_ACC_WAITING, acc_phase(&a)); /* sin gesto aun con el nuevo bebe */
}

TEST_CASE("acc: vuelve sin gesto previo, sigue esperando", "[acc]")
{
    fresh();
    baby("353", 0);
    free_("353", 10);
    baby("353", 20);
    TEST_ASSERT_EQUAL(TWIN_ACC_WAITING, acc_phase(&a));
}

/* ------------------------------------------------------ tiempo acompañado */

TEST_CASE("acc: sin gesto hoy no cuenta", "[acc]")
{
    fresh();
    baby("353", 0);
    for (uint32_t t = 1; t <= 3600; t++) acc_tick(&a, t, DAY, 10, true);
    TEST_ASSERT_EQUAL_UINT32(0, acc_hist_secs(&a, "353"));
}

TEST_CASE("acc: el gesto vale para el dia y solo con el bebe a la vista", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_gesture(&a, DAY);
    for (uint32_t t = 1; t <= 3600; t++) acc_tick(&a, t, DAY, 10, true);
    for (uint32_t t = 3601; t <= 3700; t++) acc_tick(&a, t, DAY, 11, false); /* en otra pagina */
    TEST_ASSERT_EQUAL_UINT32(3600, acc_hist_secs(&a, "353"));
    for (uint32_t t = 4000; t < 4100; t++) acc_tick(&a, t, DAY + 1, 10, true); /* dia siguiente */
    TEST_ASSERT_EQUAL_UINT32(3600, acc_hist_secs(&a, "353"));
    acc_gesture(&a, DAY + 1);
    acc_tick(&a, 4200, DAY + 1, 10, true);
    TEST_ASSERT_EQUAL_UINT32(3601, acc_hist_secs(&a, "353"));
}

TEST_CASE("acc: guarda cada 10 min mientras cuenta", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_gesture(&a, DAY);
    int saves = 0;
    for (uint32_t t = 1; t <= ACC_SAVE_S * 2; t++) {
        if (acc_tick(&a, t, DAY, 10, true) & ACC_R_SAVE) saves++;
    }
    TEST_ASSERT_EQUAL(2, saves);
}

TEST_CASE("acc: historial lleno descarta la de menos tiempo", "[acc]")
{
    fresh();
    char id[8];
    for (int i = 0; i < ACC_HIST_MAX; i++) {
        snprintf(id, sizeof(id), "I%02d", i);
        seed_hist(id, 100 + (uint32_t)i);
    }
    baby("NEW", 0);
    acc_gesture(&a, DAY);
    acc_tick(&a, 1, DAY, 10, true);
    TEST_ASSERT_EQUAL(ACC_HIST_MAX, a.p.n_hist);
    TEST_ASSERT_EQUAL_UINT32(1, acc_hist_secs(&a, "NEW"));
    TEST_ASSERT_EQUAL_UINT32(0, acc_hist_secs(&a, "I00"));
    TEST_ASSERT_EQUAL_UINT32(101, acc_hist_secs(&a, "I01"));
    acc_tick(&a, 2, DAY, 10, true); /* la actual no se descarta aunque sea la que menos tiene */
    TEST_ASSERT_EQUAL_UINT32(2, acc_hist_secs(&a, "NEW"));
}

/* ------------------------------------------------------------ recordatorio */

TEST_CASE("acc: recordatorio a las 19:00, una sola llamada", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_gesture(&a, DAY - 1); /* ayer si */
    TEST_ASSERT_FALSE(acc_tick(&a, 10, DAY, 18, true) & ACC_R_REMIND);
    TEST_ASSERT_FALSE(acc_remind_active(&a, DAY, 18));
    TEST_ASSERT_TRUE(acc_tick(&a, 11, DAY, 19, true) & ACC_R_REMIND);
    TEST_ASSERT_TRUE(acc_remind_active(&a, DAY, 19));
    TEST_ASSERT_FALSE(acc_tick(&a, 12, DAY, 20, true) & ACC_R_REMIND);
    acc_gesture(&a, DAY);
    TEST_ASSERT_FALSE(acc_remind_active(&a, DAY, 20));
}

TEST_CASE("acc: sin recordatorio si hoy ya hubo gesto, sin bebe o sin hora", "[acc]")
{
    fresh();
    TEST_ASSERT_FALSE(acc_tick(&a, 1, DAY, 19, false) & ACC_R_REMIND); /* buscando */
    baby("353", 0);
    TEST_ASSERT_FALSE(acc_tick(&a, 2, ACC_NO_DAY, -1, true) & ACC_R_REMIND);
    acc_gesture(&a, DAY);
    TEST_ASSERT_FALSE(acc_tick(&a, 3, DAY, 19, true) & ACC_R_REMIND);
}

TEST_CASE("acc: horario de silencio de 22 a 9", "[acc]")
{
    TEST_ASSERT_TRUE(acc_quiet(22));
    TEST_ASSERT_TRUE(acc_quiet(3));
    TEST_ASSERT_TRUE(acc_quiet(8));
    TEST_ASSERT_FALSE(acc_quiet(9));
    TEST_ASSERT_FALSE(acc_quiet(21));
    TEST_ASSERT_FALSE(acc_quiet(-1));
}

/* -------------------------------------------------------- dias acompañados */

TEST_CASE("acc: corazones de los ultimos 7 dias", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_gesture(&a, DAY - 3);
    acc_gesture(&a, DAY - 1);
    acc_gesture(&a, DAY);
    acc_gesture(&a, DAY); /* dos veces el mismo dia: igual */
    TEST_ASSERT_EQUAL_HEX8(0x0B, acc_week_mask(&a, DAY)); /* hoy, ayer y hace 3 */
    TEST_ASSERT_EQUAL_HEX8(0x16, acc_week_mask(&a, DAY + 1)); /* mañana se desplaza */
    TEST_ASSERT_EQUAL_HEX8(0x00, acc_week_mask(&a, DAY + 10));
    acc_gesture(&a, DAY + 10);
    TEST_ASSERT_EQUAL_HEX8(0x01, acc_week_mask(&a, DAY + 10));
}

/* --------------------------------------------------------------- guardar */

TEST_CASE("acc: restaurar lo guardado; basura = estado inicial", "[acc]")
{
    fresh();
    baby("353", 0);
    acc_gesture(&a, DAY);
    acc_tick(&a, 1, DAY, 10, true);
    acc_persist_t p = a.p;
    acc_restore(&a, &p, 7, 100);
    TEST_ASSERT_EQUAL_STRING("353", acc_current(&a));
    TEST_ASSERT_EQUAL(TWIN_ACC_WITH, acc_phase(&a));
    TEST_ASSERT_EQUAL_UINT32(1, acc_hist_secs(&a, "353"));
    TEST_ASSERT_EQUAL(0, a.n_seen); /* la tabla se rellena con los retenidos */

    p.version = 99;
    acc_restore(&a, &p, 7, 100);
    TEST_ASSERT_EQUAL(TWIN_ACC_SEARCHING, acc_phase(&a));
    TEST_ASSERT_EQUAL(0, a.p.n_hist);

    p = a.p;
    p.phase = 77;
    acc_restore(&a, &p, 7, 100);
    TEST_ASSERT_EQUAL(TWIN_ACC_SEARCHING, acc_phase(&a));
}
