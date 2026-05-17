#include "motor_service.h"
#include <avr/wdt.h>

static uint8_t cen_sm = 0;
static unsigned long cen_deadline = 0;

/** Pausa entre cambios de rele en agitado (reduce pico al conmutar giro con motor). */
static const unsigned long LAV_TR_GAP_MS = 350UL;

static int lav_applied_paso = -1;
static uint8_t lav_tr_target = 0;
static uint8_t lav_tr_sm = 0;
static unsigned long lav_tr_deadline = 0;

void motor_reset_service_state(void) {
  cen_sm = 0;
  cen_deadline = 0;
  lav_applied_paso = -1;
  lav_tr_sm = 0;
  lav_tr_deadline = 0;
}

static uint8_t lavado_normalize_paso(void) {
  if (paso < 0 || paso >= 4)
    paso = 0;
  return (uint8_t)paso;
}

static bool lavado_paso_spins(uint8_t p) {
  return p == 0 || p == 2;
}

static void lavado_motor_off(void) {
  digitalWrite(motor, HIGH);
}

static void lavado_hold_paso(uint8_t p) {
  switch (p) {
    case 0:
      digitalWrite(vel1, HIGH);
      digitalWrite(vel2, HIGH);
      digitalWrite(bomba, HIGH);
      digitalWrite(giro, HIGH);
      digitalWrite(motor, LOW);
      break;
    case 1:
      lavado_motor_off();
      break;
    case 2:
      digitalWrite(vel1, HIGH);
      digitalWrite(vel2, HIGH);
      digitalWrite(bomba, HIGH);
      digitalWrite(giro, LOW);
      digitalWrite(motor, LOW);
      break;
    case 3:
      lavado_motor_off();
      break;
    default:
      paso = 0;
      lavado_hold_paso(0);
      break;
  }
}

/* Agitado: paso 0/2 giro 5s, paso 1/3 pausa 3s (duraciones en lavadora.ino).
 * Al cambiar paso: motor OFF -> espera -> giro/vel -> espera -> motor ON (solo giros). */
static void lavado_apply_steady_outputs(void) {
  uint8_t p = lavado_normalize_paso();
  unsigned long now = millis();

  if (p == lav_applied_paso && lav_tr_sm == 0) {
    lavado_hold_paso(p);
    return;
  }

  lav_tr_target = p;

  if (lav_tr_sm == 0) {
    lav_tr_sm = 1;
    lavado_motor_off();
    lav_tr_deadline = now + LAV_TR_GAP_MS;
    return;
  }

  if (now < lav_tr_deadline)
    return;

  switch (lav_tr_sm) {
    case 1:
      if (lavado_paso_spins(lav_tr_target)) {
        digitalWrite(vel1, HIGH);
        digitalWrite(vel2, HIGH);
        digitalWrite(bomba, HIGH);
        digitalWrite(giro, lav_tr_target == 0 ? HIGH : LOW);
        lav_tr_sm = 2;
        lav_tr_deadline = now + LAV_TR_GAP_MS;
      } else {
        lavado_hold_paso(lav_tr_target);
        lav_applied_paso = (int)lav_tr_target;
        lav_tr_sm = 0;
      }
      break;
    case 2:
      digitalWrite(motor, LOW);
      lav_applied_paso = (int)lav_tr_target;
      lav_tr_sm = 0;
      break;
    default:
      lav_applied_paso = (int)lav_tr_target;
      lav_tr_sm = 0;
      lavado_hold_paso(lav_tr_target);
      break;
  }

  wdt_reset();
}

static void vaciado_apply_steady_outputs(void) {
  digitalWrite(val1, HIGH);
  digitalWrite(giro, HIGH);
  digitalWrite(vel1, HIGH);
  digitalWrite(vel2, HIGH);
  digitalWrite(motor, HIGH);
  digitalWrite(bomba, LOW);
}

static void centrifugar_apply_nonblocking(void) {
  unsigned long now = millis();

  if (acelerado != 0) {
    digitalWrite(val1, HIGH);
    digitalWrite(giro, LOW);
    digitalWrite(vel1, LOW);
    digitalWrite(vel2, LOW);
    digitalWrite(bomba, LOW);
    digitalWrite(motor, LOW);
    return;
  }

  switch (cen_sm) {
    case 0:
      digitalWrite(val1, HIGH);
      digitalWrite(giro, LOW);
      digitalWrite(vel1, HIGH);
      digitalWrite(vel2, HIGH);
      cen_deadline = now + 1000UL;
      cen_sm = 1;
      break;
    case 1:
      if (now < cen_deadline)
        break;
      digitalWrite(motor, LOW);
      cen_deadline = now + 3000UL;
      cen_sm = 2;
      break;
    case 2:
      if (now < cen_deadline)
        break;
      digitalWrite(motor, HIGH);
      cen_deadline = now + 10UL;
      cen_sm = 3;
      break;
    case 3:
      if (now < cen_deadline)
        break;
      digitalWrite(vel1, LOW);
      digitalWrite(vel2, LOW);
      cen_deadline = now + 10UL;
      cen_sm = 4;
      break;
    case 4:
      if (now < cen_deadline)
        break;
      digitalWrite(giro, LOW);
      cen_deadline = now + 100UL;
      cen_sm = 5;
      break;
    case 5:
      if (now < cen_deadline)
        break;
      digitalWrite(motor, LOW);
      acelerado = 1;
      cen_sm = 6;
      break;
    default:
      digitalWrite(vel1, LOW);
      digitalWrite(vel2, LOW);
      digitalWrite(bomba, LOW);
      digitalWrite(motor, LOW);
      break;
  }
}

void motor_refresh_outputs(void) {
  if (!encendida || fases == nullptr || faseActual < 0 || faseActual >= totalFases)
    return;

  FaseIndex f = fases[faseActual];

  if (f.funcion != CENTRIFUGAR)
    cen_sm = 0;

  switch (f.funcion) {
    case LLENADO_PRE_LAVADO:
    case LLENADO_LAVADO:
      llenado();
      if (tamborVacio == 0) {
        lavado_apply_steady_outputs();
      } else {
        lav_applied_paso = -1;
        lav_tr_sm = 0;
        digitalWrite(motor, HIGH);
        digitalWrite(giro, HIGH);
        digitalWrite(vel1, HIGH);
        digitalWrite(vel2, HIGH);
      }
      break;
    case LLENADO:
    case LLENADO_SUAVIZANTE:
      llenado();
      lav_applied_paso = -1;
      lav_tr_sm = 0;
      break;
    case LAVADO:
      digitalWrite(val1, HIGH);
      lavado_apply_steady_outputs();
      break;
    case VACIADO:
      lav_applied_paso = -1;
      lav_tr_sm = 0;
      digitalWrite(val1, HIGH);
      vaciado_apply_steady_outputs();
      break;
    case CENTRIFUGAR:
      lav_applied_paso = -1;
      lav_tr_sm = 0;
      centrifugar_apply_nonblocking();
      break;
    default:
      lav_applied_paso = -1;
      lav_tr_sm = 0;
      break;
  }
}
