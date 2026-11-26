/* IncuTwin — corazones, racha larga, niveles, sellos, pasaporte y personalizar (spec rewards).
 * Modulo puro (sin NVS ni FreeRTOS): se prueba en test_apps. Todo lo que se gana sale de lo que
 * hace quien acompaña (gesto, super corazon) y de «se fue a casa»; nunca de lo que le pasa al bebe.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RW_VERSION      1
#define RW_PASSPORT_MAX 32
#define RW_SAVED_MAX    2   /* abrazos guardados */
#define RW_DAY_NONE     INT32_MIN

typedef enum {
    RW_SEAL_FIRST = 0, /* primer «Coge mi mano» */
    RW_SEAL_WEEK,      /* racha de 7 dias */
    RW_SEAL_MONTH_RUN, /* racha de 30 dias */
    RW_SEAL_30_DAYS,   /* 30 dias con gesto en total */
    RW_SEAL_SUPER,     /* primer super corazon */
    RW_SEAL_3_BABIES,  /* mano a 3 bebes distintos */
    RW_SEAL_HOME,      /* un bebe al que diste la mano se fue a casa */
    RW_SEAL_COUNT
} rw_seal_t;

typedef enum { RW_LEVEL_SPARK = 0, RW_LEVEL_WARMTH, RW_LEVEL_HUG, RW_LEVEL_BLANKET, RW_LEVEL_SUN, RW_LEVEL_COUNT } rw_level_t;

typedef enum {
    RW_ITEM_HALO_SKY = 0, /* de serie */
    RW_ITEM_HALO_LAVENDER,
    RW_ITEM_HALO_MINT,
    RW_ITEM_HALO_PEACH,
    RW_ITEM_HALO_LILAC,
    RW_ITEM_BG_DEFAULT,   /* de serie */
    RW_ITEM_BG_CREAM,
    RW_ITEM_BG_PINK,
    RW_ITEM_BG_AQUA,
    RW_ITEM_GARLAND,      /* gratis: 10 dias de diciembre acompañando */
    RW_ITEM_NAMED_INCUNEST, /* «Una IncuNest con tu nombre»: se canjea una vez; lo cumple MOW */
    RW_ITEM_COUNT
} rw_item_t;

#define RW_HALO_FIRST RW_ITEM_HALO_SKY
#define RW_HALO_LAST  RW_ITEM_HALO_LILAC
#define RW_BG_FIRST   RW_ITEM_BG_DEFAULT
#define RW_BG_LAST    RW_ITEM_BG_AQUA

typedef struct {
    char iso[2];
    uint8_t star; /* algun bebe de aqui se fue a casa contigo */
} rw_country_t;

/* Lo que se guarda (blob versionado) */
typedef struct {
    uint8_t version;
    uint8_t saved;          /* abrazos guardados 0..RW_SAVED_MAX */
    uint8_t seals;          /* bits de rw_seal_t */
    uint8_t n_countries;
    uint16_t balance;       /* saldo */
    uint16_t streak;        /* dias seguidos con gesto (los de descanso no cuentan ni rompen) */
    uint16_t total_days;    /* dias con gesto en total */
    uint16_t babies;        /* bebes distintos (estancias) con gesto */
    uint16_t unlocked;      /* bits de rw_item_t */
    uint16_t dec_year;      /* año de dec_days */
    uint32_t earned;        /* corazones ganados en total (nivel) */
    int32_t last_day;       /* ultimo dia local con gesto; RW_DAY_NONE = nunca */
    int32_t super_day;      /* ultimo dia con super corazon */
    uint32_t dec_days;      /* bits de los dias de diciembre con gesto (bit 1 = dia 1) */
    uint32_t last_stay;     /* estancia del ultimo gesto (hash); 0 = ninguna */
    char last_stay_cc[2];   /* su pais */
    uint8_t last_stay_home; /* ya se conto su «a casa» */
    uint8_t halo;           /* rw_item_t elegido para el halo en reposo */
    uint8_t bg;             /* rw_item_t elegido para el fondo de Inicio */
    uint8_t garland_on;     /* guirnalda puesta */
    rw_country_t passport[RW_PASSPORT_MAX];
    uint8_t redeem_sent;    /* el canje de la IncuNest con tu nombre ya llego al broker (cabe en el
                             * relleno: el blob no cambia de tamaño) */
} rw_state_t;

/* Un «Coge mi mano» completado */
typedef struct {
    bool time_valid;   /* sin hora no se sabe que dia es: no suma */
    int32_t day;       /* dia local (rw_day_of) */
    int year, month, mday;
    uint8_t rest_mask; /* dias sin bebes: bit k = hace k dias (accompany) */
    const char *country; /* ISO alfa-2 de la IncuNest; "" = no se sabe */
    uint32_t stay;     /* estancia actual (hash); 0 = no se sabe */
} rw_gesture_t;

/* Novedades de una operacion (para la franja y el «+N») */
typedef struct {
    uint8_t hearts;    /* corazones ganados */
    uint8_t new_seals; /* bits de rw_seal_t conseguidos ahora */
    bool level_up;
    bool garland;      /* se acaba de desbloquear la guirnalda */
} rw_news_t;

void rw_init(rw_state_t *s);
/* Dia civil -> dias desde 1970-01-01 (para contar dias seguidos con la hora local) */
int32_t rw_day_of(int year, int month, int mday);
rw_level_t rw_level(uint32_t earned);
uint32_t rw_level_min(rw_level_t level);
uint16_t rw_price(rw_item_t item);
bool rw_garland_season(int month, int mday); /* 1-dic .. 6-ene */

rw_news_t rw_on_gesture(rw_state_t *s, const rw_gesture_t *g);
rw_news_t rw_on_super(rw_state_t *s, bool time_valid, int32_t day);
rw_news_t rw_on_home(rw_state_t *s, uint32_t stay);
/* Corazones de la IncuPray: suman al saldo y al nivel fuera del tope diario */
rw_news_t rw_add(rw_state_t *s, uint8_t hearts);
/* Gasta `n` si hay saldo (la apuesta de la IncuPray); false y nada si no */
bool rw_spend(rw_state_t *s, uint16_t n);

/* Desbloquea (resta el precio) y elige; si ya estaba desbloqueado, solo elige. false si no hay
 * saldo (en *missing lo que falta), si es la guirnalda sin conseguir o un canje ya hecho. */
bool rw_unlock(rw_state_t *s, rw_item_t item, uint16_t *missing);
/* La racha de 7 dias de accompany (bit 0 = hoy) como punto de partida, solo la primera vez */
void rw_seed_streak(rw_state_t *s, uint8_t week, uint8_t rest, int32_t today);
/* Estado simulado de la demo */
void rw_demo_seed(rw_state_t *s);

#ifdef __cplusplus
}
#endif
