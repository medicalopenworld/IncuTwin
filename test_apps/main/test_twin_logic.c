/* Specs twin-display (barra de estado), twin-state-ingest (mapeo, dedup,
 * transiciones), wifi-connectivity (niveles RSSI), pairing (ids). */
#include <string.h>

#include "twin_logic.h"
#include "unity.h"

static twin_snapshot_t linked_snapshot(void)
{
    twin_snapshot_t s = { 0 };
    s.link.wifi = true;
    s.link.has_creds = true;
    s.link.broker_connected = true;
    s.link.broker_once = true;
    s.link.paired = true;
    s.link.state_rx = true;
    twin_incubator_reset(&s.inc, 1);
    s.inc.online = true;
    s.inc.baby = TWIN_BABY_IN;
    s.inc.thermo = TWIN_THERMO_STABLE;
    twin_derive(&s);
    return s;
}

static twin_msg_t base_msg(twin_msg_state_t st, uint32_t seq, const char *ev)
{
    twin_msg_t m = { 0 };
    strcpy(m.incubator_id, "353");
    m.state = st;
    m.bpm = -1;
    m.has_seq = true;
    m.seq = seq;
    strncpy(m.last_event, ev, sizeof(m.last_event) - 1);
    return m;
}

/* ------------------------------------------------------- barra de estado */

TEST_CASE("status: las 14 prioridades en orden", "[twin][status]")
{
    twin_snapshot_t s = linked_snapshot();
    TEST_ASSERT_EQUAL(TWIN_ST_SLEEP, twin_status_of(&s));
    s.inc.awake = true;
    TEST_ASSERT_EQUAL(TWIN_ST_AWAKE, twin_status_of(&s));
    strcpy(s.inc.name, "Lucía");
    TEST_ASSERT_EQUAL(TWIN_ST_NAME, twin_status_of(&s));
    s.inc.thermo = TWIN_THERMO_ALARM;
    TEST_ASSERT_EQUAL(TWIN_ST_ALARM, twin_status_of(&s)); /* alarma antes que nombre */
    s.inc.baby = TWIN_BABY_NONE; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_NO_BABY, twin_status_of(&s));
    s.inc.online = false; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_INC_OFF, twin_status_of(&s));
    s.link.state_rx = false; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_WAITING, twin_status_of(&s));
    s.link.paired = false; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_UNPAIRED, twin_status_of(&s));
    s.link.broker_lost = true; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_BROKER_LOST, twin_status_of(&s));
    s.link.broker_once = false; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_CONNECTING, twin_status_of(&s));
    s.link.has_creds = false; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_NO_CREDS, twin_status_of(&s));
    s.link.wifi = false; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_NO_WIFI, twin_status_of(&s));
}

TEST_CASE("status: con sus papas y en casa mandan sobre todo", "[twin][status]")
{
    twin_snapshot_t s = linked_snapshot();
    s.inc.baby = TWIN_BABY_PARENTS;
    s.inc.thermo = TWIN_THERMO_ALARM;
    twin_derive(&s);
    TEST_ASSERT_TRUE(s.parents_view);
    TEST_ASSERT_EQUAL(TWIN_ST_PARENTS, twin_status_of(&s));
    s.inc.baby = TWIN_BABY_HOME; twin_derive(&s);
    TEST_ASSERT_EQUAL(TWIN_ST_HOME, twin_status_of(&s));
    /* sin enlace no hay vista de papas */
    s.link.wifi = false; twin_derive(&s);
    TEST_ASSERT_FALSE(s.parents_view);
    TEST_ASSERT_EQUAL(TWIN_ST_NO_WIFI, twin_status_of(&s));
}

TEST_CASE("derive: caida breve del broker mantiene link_ok", "[twin][status]")
{
    twin_snapshot_t s = linked_snapshot();
    s.link.broker_connected = false; /* < 600 s: broker_lost sigue a false */
    twin_derive(&s);
    TEST_ASSERT_TRUE(s.link_ok);
    TEST_ASSERT_TRUE(s.show_baby);
    s.link.broker_lost = true;
    twin_derive(&s);
    TEST_ASSERT_FALSE(s.link_ok);
    TEST_ASSERT_FALSE(s.show_baby);
}

/* --------------------------------------------------------------- mapeo */

TEST_CASE("apply: bebe con calor y pulso", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 0;
    twin_incubator_reset(&inc, 1);
    twin_msg_t m = base_msg(TWIN_MSG_STATE_BABY, 10, "baby_in");
    m.heat = true; m.pulseox = true; m.bpm = 142;
    twin_apply_result_t r = twin_apply(&inc, &seq, &m);
    TEST_ASSERT_TRUE(r.is_new);
    TEST_ASSERT_EQUAL(TWIN_BABY_IN, inc.baby);
    TEST_ASSERT_EQUAL(TWIN_THERMO_STABLE, inc.thermo);
    TEST_ASSERT_FALSE(inc.photo);
    TEST_ASSERT_EQUAL(142, inc.bpm);
    TEST_ASSERT_TRUE(inc.online);
    TEST_ASSERT_EQUAL(10, seq);
    TEST_ASSERT_EQUAL(TWIN_TR_BABY_IN, r.transitions);
    TEST_ASSERT_TRUE(r.persist);
}

TEST_CASE("apply: bpm sin pulseox no cuenta", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 0;
    twin_incubator_reset(&inc, 1);
    twin_msg_t m = base_msg(TWIN_MSG_STATE_BABY, 1, "heartbeat");
    m.heat = true; m.bpm = 142;
    twin_apply(&inc, &seq, &m);
    TEST_ASSERT_EQUAL(0, inc.bpm);
    m.pulseox = true; m.bpm = 301; m.seq = 2;
    twin_apply(&inc, &seq, &m);
    TEST_ASSERT_EQUAL(0, inc.bpm); /* fuera de rango */
}

TEST_CASE("apply: offline y tratamientos ausentes", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 0;
    twin_incubator_reset(&inc, 1);
    twin_msg_t m = base_msg(TWIN_MSG_STATE_OFFLINE, 5, "incubator_offline");
    twin_apply(&inc, &seq, &m);
    TEST_ASSERT_FALSE(inc.online);
    TEST_ASSERT_EQUAL(TWIN_THERMO_OFF, inc.thermo);
    TEST_ASSERT_FALSE(inc.photo);
    TEST_ASSERT_EQUAL(0, inc.bpm);
}

TEST_CASE("apply: extensiones prevalecen; nombre se borra si no viene", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 0;
    twin_incubator_reset(&inc, 1);
    twin_msg_t m = base_msg(TWIN_MSG_STATE_BABY, 1, "baby_in");
    m.heat = true;
    m.has_thermo = true; m.thermo = TWIN_THERMO_HEATING;
    m.has_baby = true; m.baby = TWIN_BABY_PARENTS;
    m.has_awake = true; m.awake = true;
    m.has_skin = true; m.skin = 4;
    m.has_name = true; strcpy(m.name, "Lucía");
    m.has_weight = true; m.weight_g = 1250;
    m.has_age = true; m.age_d = 3;
    twin_apply_result_t r = twin_apply(&inc, &seq, &m);
    TEST_ASSERT_EQUAL(TWIN_THERMO_HEATING, inc.thermo);
    TEST_ASSERT_EQUAL(TWIN_BABY_PARENTS, inc.baby);
    TEST_ASSERT_TRUE(inc.awake);
    TEST_ASSERT_EQUAL(4, inc.skin);
    TEST_ASSERT_EQUAL_STRING("Lucía", inc.name);
    TEST_ASSERT_EQUAL(1250, inc.weight_g);
    TEST_ASSERT_EQUAL(3, inc.age_d);
    TEST_ASSERT_EQUAL(TWIN_TR_BABY_PARENTS, r.transitions);

    twin_msg_t m2 = base_msg(TWIN_MSG_STATE_BABY, 2, "treatment_changed");
    m2.heat = true;
    twin_apply(&inc, &seq, &m2);
    TEST_ASSERT_EQUAL_STRING("", inc.name);
    TEST_ASSERT_EQUAL(0, inc.weight_g);
    TEST_ASSERT_EQUAL(-1, inc.age_d);
    TEST_ASSERT_FALSE(inc.awake);
    TEST_ASSERT_EQUAL(4, inc.skin); /* el tono se conserva */
    TEST_ASSERT_EQUAL(TWIN_BABY_IN, inc.baby);
}

/* --------------------------------------------------------------- dedup */

TEST_CASE("dedup: seq menor o igual es refresco sin transiciones", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 1300;
    twin_incubator_reset(&inc, 1);
    inc.baby = TWIN_BABY_NONE;
    twin_msg_t m = base_msg(TWIN_MSG_STATE_BABY, 1300, "baby_in");
    twin_apply_result_t r = twin_apply(&inc, &seq, &m);
    TEST_ASSERT_FALSE(r.is_new);
    TEST_ASSERT_EQUAL(0, r.transitions);
    TEST_ASSERT_FALSE(r.persist);
    TEST_ASSERT_EQUAL(TWIN_BABY_IN, inc.baby); /* el modelo si se refresca */
    TEST_ASSERT_EQUAL(1300, seq);
}

TEST_CASE("dedup: reinicio con bebe dentro no repite melodia", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 1287;
    twin_incubator_reset(&inc, 1);
    inc.baby = TWIN_BABY_IN; /* persistido en NVS antes del reinicio */
    twin_msg_t m = base_msg(TWIN_MSG_STATE_BABY, 1290, "heartbeat");
    m.heat = true; m.pulseox = true; m.bpm = 130;
    twin_apply_result_t r = twin_apply(&inc, &seq, &m);
    TEST_ASSERT_TRUE(r.is_new);
    TEST_ASSERT_EQUAL(0, r.transitions);
    TEST_ASSERT_FALSE(r.persist); /* heartbeat sin cambio de baby no gasta NVS */
    TEST_ASSERT_EQUAL(1290, seq);
}

TEST_CASE("dedup: evento perdido durante un apagado", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 1290;
    twin_incubator_reset(&inc, 1);
    inc.baby = TWIN_BABY_IN;
    twin_msg_t m = base_msg(TWIN_MSG_STATE_FREE, 1300, "baby_out");
    twin_apply_result_t r = twin_apply(&inc, &seq, &m);
    TEST_ASSERT_TRUE(r.is_new);
    TEST_ASSERT_EQUAL(TWIN_BABY_NONE, inc.baby);
    TEST_ASSERT_EQUAL(0, r.transitions); /* baby_out no suena */
    TEST_ASSERT_TRUE(r.persist);
    TEST_ASSERT_EQUAL(1300, seq);
}

TEST_CASE("dedup: sin event_seq siempre es nuevo", "[twin][ingest]")
{
    twin_incubator_t inc; uint32_t seq = 50;
    twin_incubator_reset(&inc, 1);
    twin_msg_t m = base_msg(TWIN_MSG_STATE_BABY, 0, "");
    m.has_seq = false;
    twin_apply_result_t r = twin_apply(&inc, &seq, &m);
    TEST_ASSERT_TRUE(r.is_new);
    TEST_ASSERT_EQUAL(TWIN_TR_BABY_IN, r.transitions);
    TEST_ASSERT_EQUAL(50, seq); /* no se toca */
}

/* ---------------------------------------------------------- transiciones */

TEST_CASE("transiciones: solo in y parents/home suenan", "[twin]")
{
    TEST_ASSERT_EQUAL(TWIN_TR_BABY_IN, twin_transitions_between(TWIN_BABY_NONE, TWIN_BABY_IN));
    TEST_ASSERT_EQUAL(TWIN_TR_BABY_IN, twin_transitions_between(TWIN_BABY_PARENTS, TWIN_BABY_IN));
    TEST_ASSERT_EQUAL(0, twin_transitions_between(TWIN_BABY_IN, TWIN_BABY_IN));
    TEST_ASSERT_EQUAL(0, twin_transitions_between(TWIN_BABY_IN, TWIN_BABY_NONE));
    TEST_ASSERT_EQUAL(TWIN_TR_BABY_PARENTS, twin_transitions_between(TWIN_BABY_IN, TWIN_BABY_PARENTS));
    TEST_ASSERT_EQUAL(TWIN_TR_BABY_PARENTS, twin_transitions_between(TWIN_BABY_IN, TWIN_BABY_HOME));
    TEST_ASSERT_EQUAL(0, twin_transitions_between(TWIN_BABY_PARENTS, TWIN_BABY_HOME));
}

/* ------------------------------------------------------------------ varios */

TEST_CASE("rssi: umbrales de cobertura", "[wifi]")
{
    TEST_ASSERT_EQUAL(3, twin_rssi_level(-55));
    TEST_ASSERT_EQUAL(2, twin_rssi_level(-56));
    TEST_ASSERT_EQUAL(2, twin_rssi_level(-67));
    TEST_ASSERT_EQUAL(1, twin_rssi_level(-68));
    TEST_ASSERT_EQUAL(1, twin_rssi_level(-78));
    TEST_ASSERT_EQUAL(0, twin_rssi_level(-79));
}

TEST_CASE("pairing: validez del incubator_id", "[pairing]")
{
    TEST_ASSERT_TRUE(twin_incubator_id_valid("353"));
    TEST_ASSERT_TRUE(twin_incubator_id_valid("IncuNest-1_2"));
    TEST_ASSERT_TRUE(twin_incubator_id_valid("1234567890123456"));  /* 16 */
    TEST_ASSERT_FALSE(twin_incubator_id_valid("12345678901234567")); /* 17 */
    TEST_ASSERT_FALSE(twin_incubator_id_valid(""));
    TEST_ASSERT_FALSE(twin_incubator_id_valid(NULL));
    TEST_ASSERT_FALSE(twin_incubator_id_valid("353/state"));
    TEST_ASSERT_FALSE(twin_incubator_id_valid("a b"));
}

TEST_CASE("reset: estado inicial de la incubadora", "[twin]")
{
    twin_incubator_t inc;
    twin_incubator_reset(&inc, 3);
    TEST_ASSERT_FALSE(inc.online);
    TEST_ASSERT_EQUAL(TWIN_BABY_NONE, inc.baby);
    TEST_ASSERT_EQUAL(0, inc.bpm);
    TEST_ASSERT_EQUAL(3, inc.skin);
    TEST_ASSERT_EQUAL(-1, inc.age_d);
    twin_incubator_reset(&inc, 9);
    TEST_ASSERT_EQUAL(0, inc.skin); /* fuera de rango -> 0 */
}
